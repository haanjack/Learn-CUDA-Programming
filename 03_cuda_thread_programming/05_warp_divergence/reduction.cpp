#include <stdio.h>
#include <stdlib.h>

// cuda runtime
#include <cuda_runtime.h>
#include <helper_timer.h>

#include "reduction.h"

void run_benchmark(int (*reduce)(float*, float*, int, int), 
                   float *d_outPtr, float *d_inPtr, int size);
void init_input(float* data, int size);
float get_cpu_result(float *data, int size);
void message()
{
    puts("Invalid reduction request!! 0-1 are avaiable.");
    exit(EXIT_FAILURE);
}

////////////////////////////////////////////////////////////////////////////////
// Program main
////////////////////////////////////////////////////////////////////////////////
int 
main(int argc, char *argv[])
{
    float *h_inPtr;
    float *d_inPtr, *d_outPtr;

    unsigned int size = 1 << 24;

    float result_host, result_gpu;
    int mode = -1;

    if (argc > 1) {
        mode = atoi(argv[1]);
        if (mode < 0 || mode > 1)
            message();
    }
    else {
        message();
    }

    srand(2019);

    // Allocate memory
    h_inPtr = (float*)malloc(size * sizeof(float));
    
    // Data initialization with random values
    init_input(h_inPtr, size);

    // Prepare GPU resource
    cudaMalloc((void**)& d_inPtr, size * sizeof(float));
    cudaMalloc((void**)&d_outPtr, size * sizeof(float));

    cudaMemcpy(d_inPtr, h_inPtr, size * sizeof(float), cudaMemcpyHostToDevice);

    // Get reduction result from GPU (reduction 0)
    switch (mode)
    {
        case 0:
            printf("interleave addressing\n");
            run_benchmark(reduction_1, d_outPtr, d_inPtr, size); break;
        case 1:
            printf("sequential addressing\n");
            run_benchmark(reduction_2, d_outPtr, d_inPtr, size); break;
        default:
            message();
    }
    cudaMemcpy(&result_gpu, &d_outPtr[0], sizeof(float), cudaMemcpyDeviceToHost);

    // Get reduction result from GPU

    // Get all sum from CPU
    result_host = get_cpu_result(h_inPtr, size);
    printf("host: %f, device %f\n", result_host, result_gpu);
    
    // Terminates memory
    cudaFree(d_outPtr);
    cudaFree(d_inPtr);
    free(h_inPtr);

    return 0;
}

void
run_reduction(int (*reduce)(float*, float*, int, int), 
              float *d_outPtr, float *d_inPtr, int size, int n_threads)
{
    //printf("size: %d\n", size);
    while(size > 1) {
        size = reduce(d_outPtr, d_inPtr, size, n_threads);
        //printf("size: %d\n", size);
    }
}

void
run_benchmark(int (*reduce)(float*, float*, int, int), 
              float *d_outPtr, float *d_inPtr, int size)
{
    int num_threads = 256;
    int test_iter = 100;

    // warm-up
    reduce(d_outPtr, d_inPtr, size, num_threads);

    // initialize timer
    StopWatchInterface *timer;
    sdkCreateTimer(&timer);
    sdkStartTimer(&timer);

    ////////
    // Operation body
    ////////
    for (int i = 0; i < test_iter; i++) {
        cudaMemcpy(d_outPtr, d_inPtr, size * sizeof(float), cudaMemcpyDeviceToDevice);
        run_reduction(reduce, d_outPtr, d_outPtr, size, num_threads);
    }

    // getting elapsed time
    cudaDeviceSynchronize();
    sdkStopTimer(&timer);

    // Compute and print the performance
    float elapsed_time_msed = sdkGetTimerValue(&timer) / (float)test_iter;
    float bandwidth = size * sizeof(float) / elapsed_time_msed / 1e6;
    printf("Time= %.3f msec, bandwidth= %f GB/s\n", elapsed_time_msed, bandwidth);

    sdkDeleteTimer(&timer);
}

void
init_input(float *data, int size)
{
    for (int i = 0; i < size; i++) {
        // Keep the numbers small so we don't get truncation error in the sum
        data[i] = (rand() & 0xFF) / (float)RAND_MAX;
    }
}

float
get_cpu_result(float *data, int size)
{
    double result = 0.f;
    for (int i = 0; i < size; i++)
        result += data[i];

    return (float)result;
}
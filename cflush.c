/* Copyright 2019 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <metal/cache.h>
#include <metal/cpu.h>
#include <metal/timer.h>

/* The size of the cache line is used to allocate a naturally-aligned
 * array of equal size. This allows us to flush/discard the contents
 * of the array from the L1 cache without fear of discarding anything
 * important.
 *
 * TODO: Calculate the size of the cache line from the Devicetree */
#define CACHE_LINE_SIZE 64

volatile uint32_t cache_line[CACHE_LINE_SIZE/sizeof(uint32_t)] __attribute__((aligned(CACHE_LINE_SIZE)));

#define TEST_CYCLES 64

unsigned long long int start_cycles[TEST_CYCLES];
unsigned long long int measured_cycles[TEST_CYCLES];

#define UNUSED(x) ((void)(x))

unsigned long long int average_cycles(void) {
    unsigned long long int sum = 0;
    for (int i = 0; i < TEST_CYCLES; i++) {
        sum += measured_cycles[i] - start_cycles[i];
    }
    return (sum / TEST_CYCLES);
}

int main (void)
{
    if (!metal_dcache_l1_available()) {
        /* If no L1 D-cache is present, we have no ability to flush or discard
         * data cache lines. Exit without doing anything. */
        return 0;
    }

    /* Demo 1: Measure the average time to read from an address without flushing. */

    cache_line[0] = 0xA5A5A5A5;
    for (int i = 0; i < TEST_CYCLES; i++) {
        metal_timer_get_cyclecount(metal_cpu_get_current_hartid(), &(start_cycles[i]));
        uint32_t data = cache_line[0];
        metal_timer_get_cyclecount(metal_cpu_get_current_hartid(), &(measured_cycles[i]));

        UNUSED(data);
    }

    unsigned long long average_cycles_without_flush = average_cycles();
    printf("Read without flush: %llu cycles\n", average_cycles_without_flush);

    /* Demo 2: Measure the average time to read from an address with flushing. */

    cache_line[0] = 0x5A5A5A5A;
    for (int i = 0; i < TEST_CYCLES; i++) {
        /* Flush the cache to write back any data and invalidate the cache line */
        metal_dcache_l1_flush((uintptr_t)cache_line);

        metal_timer_get_cyclecount(metal_cpu_get_current_hartid(), &(start_cycles[i]));
        uint32_t data = cache_line[0];
        metal_timer_get_cyclecount(metal_cpu_get_current_hartid(), &(measured_cycles[i]));

        UNUSED(data);
    }

    unsigned long long average_cycles_with_flush = average_cycles();
    printf("Read with flush: %llu cycles\n", average_cycles_with_flush);

    /* Demo 3: Discard a cache line without writeback */

    /* Write 0 and flush */
    cache_line[0] = 0;
    metal_dcache_l1_flush((uintptr_t)cache_line);

    /* Write 1 and discard */
    cache_line[0] = 1;
    metal_dcache_l1_discard((uintptr_t)cache_line);

    /* The discard may or may not lose data depending on the microarchitecture,
     * presence of speculation, or pure luck. since we can't guarantee either
     * outcome, but still want to show that the discard operation succeeds,
     * merely observe the outcome. */
    if (cache_line[0] != 0) {
        puts("Writeback occurred despite discard of associated cache line");
    } else {
        puts("Write discarded from L1");
    }

    return 0;
}


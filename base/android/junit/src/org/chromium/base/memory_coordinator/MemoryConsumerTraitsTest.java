// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.memory_coordinator;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link MemoryConsumerTraits}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MemoryConsumerTraitsTest {
    @Test
    public void testActiveTraitsBuilder() {
        MemoryConsumerTraits traits =
                new MemoryConsumerTraits.Builder(
                                EstimatedMemoryUsage.SMALL,
                                ReleaseMemoryDataAccess.NONE,
                                InformationRetention.LOSSLESS,
                                ExecutionType.SYNCHRONOUS)
                        .setSupportsMemoryLimit(SupportsMemoryLimit.YES)
                        .setInProcess(InProcess.YES)
                        .setIsStateful(IsStateful.YES)
                        .setReleaseMemoryCpuCost(ReleaseMemoryCpuCost.EXPENSIVE)
                        .build();

        assertEquals(ConsumerType.ACTIVE, traits.getConsumerType());
        assertEquals(EstimatedMemoryUsage.SMALL, traits.getEstimatedMemoryUsage());
        assertEquals(ReleaseMemoryDataAccess.NONE, traits.getReleaseMemoryDataAccess());
        assertEquals(InformationRetention.LOSSLESS, traits.getInformationRetention());
        assertEquals(ExecutionType.SYNCHRONOUS, traits.getExecutionType());
        assertEquals(SupportsMemoryLimit.YES, traits.getSupportsMemoryLimit());
        assertEquals(InProcess.YES, traits.getInProcess());
        assertEquals(IsStateful.YES, traits.getIsStateful());
        assertEquals(ReleaseMemoryCpuCost.EXPENSIVE, traits.getReleaseMemoryCpuCost());
    }

    @Test
    public void testPassiveTraits() {
        MemoryConsumerTraits passive = MemoryConsumerTraits.createPassive();

        assertEquals(ConsumerType.PASSIVE, passive.getConsumerType());
        assertEquals(EstimatedMemoryUsage.NA, passive.getEstimatedMemoryUsage());
        assertEquals(ReleaseMemoryDataAccess.NA, passive.getReleaseMemoryDataAccess());
        assertEquals(InformationRetention.NA, passive.getInformationRetention());
        assertEquals(ExecutionType.SYNCHRONOUS, passive.getExecutionType());
        assertEquals(SupportsMemoryLimit.YES, passive.getSupportsMemoryLimit());
        assertEquals(InProcess.NA, passive.getInProcess());
        assertEquals(ReleaseGCReferences.NO, passive.getReleaseGCReferences());
        assertEquals(ReleaseMemoryCpuCost.NA, passive.getReleaseMemoryCpuCost());
    }

    @Test
    public void testEqualsAndHashCode() {
        MemoryConsumerTraits traitsA =
                new MemoryConsumerTraits.Builder(
                                EstimatedMemoryUsage.SMALL,
                                ReleaseMemoryDataAccess.NONE,
                                InformationRetention.LOSSLESS,
                                ExecutionType.SYNCHRONOUS)
                        .build();

        MemoryConsumerTraits traitsB =
                new MemoryConsumerTraits.Builder(
                                EstimatedMemoryUsage.SMALL,
                                ReleaseMemoryDataAccess.NONE,
                                InformationRetention.LOSSLESS,
                                ExecutionType.SYNCHRONOUS)
                        .build();

        MemoryConsumerTraits traitsC =
                new MemoryConsumerTraits.Builder(
                                EstimatedMemoryUsage.LARGE,
                                ReleaseMemoryDataAccess.READS,
                                InformationRetention.LOSSY,
                                ExecutionType.ASYNCHRONOUS)
                        .build();

        assertEquals(traitsA, traitsB);
        assertEquals(traitsA.hashCode(), traitsB.hashCode());
        org.junit.Assert.assertNotEquals(traitsA, traitsC);
        org.junit.Assert.assertNotEquals(traitsA, MemoryConsumerTraits.createPassive());
    }

    @Test
    public void testGetPackedTraits() {
        MemoryConsumerTraits traits =
                new MemoryConsumerTraits.Builder(
                                EstimatedMemoryUsage.SMALL,
                                ReleaseMemoryDataAccess.NONE,
                                InformationRetention.LOSSLESS,
                                ExecutionType.SYNCHRONOUS)
                        .setSupportsMemoryLimit(SupportsMemoryLimit.YES)
                        .setInProcess(InProcess.YES)
                        .setIsStateful(IsStateful.YES)
                        .setReleaseMemoryCpuCost(ReleaseMemoryCpuCost.EXPENSIVE)
                        .build();

        int packed = traits.getPackedTraits();
        assertEquals(ConsumerType.ACTIVE, packed & 0x1);
        assertEquals(EstimatedMemoryUsage.SMALL, (packed >> 1) & 0x3);
        assertEquals(ReleaseMemoryDataAccess.NONE, (packed >> 3) & 0x3);
        assertEquals(InformationRetention.LOSSLESS, (packed >> 5) & 0x3);
        assertEquals(ExecutionType.SYNCHRONOUS, (packed >> 7) & 0x1);
        assertEquals(SupportsMemoryLimit.YES, (packed >> 8) & 0x1);
        assertEquals(InProcess.YES, (packed >> 9) & 0x3);
        assertEquals(RecreateMemoryCost.NA, (packed >> 11) & 0x3);
        assertEquals(ReleaseGCReferences.NO, (packed >> 13) & 0x1);
        assertEquals(GarbageCollectsV8Heap.NO, (packed >> 14) & 0x1);
        assertEquals(IsStateful.YES, (packed >> 15) & 0x1);
        assertEquals(ReleaseMemoryCpuCost.EXPENSIVE, (packed >> 16) & 0x3);
    }
}

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
                                ReleaseMemoryCost.FREES_PAGES_WITHOUT_TRAVERSAL,
                                InformationRetention.LOSSLESS,
                                ExecutionType.SYNCHRONOUS)
                        .setSupportsMemoryLimit(SupportsMemoryLimit.YES)
                        .setInProcess(InProcess.YES)
                        .setIsStateful(IsStateful.YES)
                        .build();

        assertEquals(ConsumerType.ACTIVE, traits.getConsumerType());
        assertEquals(EstimatedMemoryUsage.SMALL, traits.getEstimatedMemoryUsage());
        assertEquals(
                ReleaseMemoryCost.FREES_PAGES_WITHOUT_TRAVERSAL, traits.getReleaseMemoryCost());
        assertEquals(InformationRetention.LOSSLESS, traits.getInformationRetention());
        assertEquals(ExecutionType.SYNCHRONOUS, traits.getExecutionType());
        assertEquals(SupportsMemoryLimit.YES, traits.getSupportsMemoryLimit());
        assertEquals(InProcess.YES, traits.getInProcess());
        assertEquals(IsStateful.YES, traits.getIsStateful());
    }

    @Test
    public void testPassiveTraits() {
        MemoryConsumerTraits passive = MemoryConsumerTraits.createPassive();

        assertEquals(ConsumerType.PASSIVE, passive.getConsumerType());
        assertEquals(EstimatedMemoryUsage.NA, passive.getEstimatedMemoryUsage());
        assertEquals(ReleaseMemoryCost.NA, passive.getReleaseMemoryCost());
        assertEquals(InformationRetention.NA, passive.getInformationRetention());
        assertEquals(ExecutionType.SYNCHRONOUS, passive.getExecutionType());
        assertEquals(SupportsMemoryLimit.YES, passive.getSupportsMemoryLimit());
        assertEquals(InProcess.NA, passive.getInProcess());
        assertEquals(ReleaseGCReferences.NO, passive.getReleaseGCReferences());
    }

    @Test
    public void testEqualsAndHashCode() {
        MemoryConsumerTraits traitsA =
                new MemoryConsumerTraits.Builder(
                                EstimatedMemoryUsage.SMALL,
                                ReleaseMemoryCost.FREES_PAGES_WITHOUT_TRAVERSAL,
                                InformationRetention.LOSSLESS,
                                ExecutionType.SYNCHRONOUS)
                        .build();

        MemoryConsumerTraits traitsB =
                new MemoryConsumerTraits.Builder(
                                EstimatedMemoryUsage.SMALL,
                                ReleaseMemoryCost.FREES_PAGES_WITHOUT_TRAVERSAL,
                                InformationRetention.LOSSLESS,
                                ExecutionType.SYNCHRONOUS)
                        .build();

        MemoryConsumerTraits traitsC =
                new MemoryConsumerTraits.Builder(
                                EstimatedMemoryUsage.LARGE,
                                ReleaseMemoryCost.REQUIRES_TRAVERSAL,
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
                                ReleaseMemoryCost.FREES_PAGES_WITHOUT_TRAVERSAL,
                                InformationRetention.LOSSLESS,
                                ExecutionType.SYNCHRONOUS)
                        .setSupportsMemoryLimit(SupportsMemoryLimit.YES)
                        .setInProcess(InProcess.YES)
                        .setIsStateful(IsStateful.YES)
                        .build();

        int packed = traits.getPackedTraits();
        assertEquals(ConsumerType.ACTIVE, packed & 0x1);
        assertEquals(EstimatedMemoryUsage.SMALL, (packed >> 1) & 0x3);
        assertEquals(ReleaseMemoryCost.FREES_PAGES_WITHOUT_TRAVERSAL, (packed >> 3) & 0x3);
        assertEquals(InformationRetention.LOSSLESS, (packed >> 5) & 0x3);
        assertEquals(ExecutionType.SYNCHRONOUS, (packed >> 7) & 0x1);
        assertEquals(SupportsMemoryLimit.YES, (packed >> 8) & 0x1);
        assertEquals(InProcess.YES, (packed >> 9) & 0x3);
        assertEquals(RecreateMemoryCost.NA, (packed >> 11) & 0x3);
        assertEquals(ReleaseGCReferences.NO, (packed >> 13) & 0x1);
        assertEquals(GarbageCollectsV8Heap.NO, (packed >> 14) & 0x1);
        assertEquals(IsStateful.YES, (packed >> 15) & 0x1);
    }
}

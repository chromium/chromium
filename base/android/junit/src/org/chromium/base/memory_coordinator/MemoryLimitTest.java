// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.memory_coordinator;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link MemoryLimit}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MemoryLimitTest {
    @Test
    public void testDefaultsAndPercentages() {
        assertEquals(100, MemoryLimit.DEFAULT.getPercent());
        assertEquals(1.0, MemoryLimit.DEFAULT.getRatio(), 0.0001);

        assertEquals(100, MemoryLimit.NO_PRESSURE.getPercent());
        assertEquals(1.0, MemoryLimit.NO_PRESSURE.getRatio(), 0.0001);

        assertEquals(50, MemoryLimit.MODERATE_PRESSURE.getPercent());
        assertEquals(0.5, MemoryLimit.MODERATE_PRESSURE.getRatio(), 0.0001);

        assertEquals(0, MemoryLimit.CRITICAL_PRESSURE.getPercent());
        assertEquals(0.0, MemoryLimit.CRITICAL_PRESSURE.getRatio(), 0.0001);

        MemoryLimit limit50 = new MemoryLimit(50);
        assertEquals(50, limit50.getPercent());
        assertEquals(0.5, limit50.getRatio(), 0.0001);

        MemoryLimit limit150 = new MemoryLimit(150);
        assertEquals(150, limit150.getPercent());
        assertEquals(1.5, limit150.getRatio(), 0.0001);
    }

    @Test
    public void testScale() {
        MemoryLimit limit0 = MemoryLimit.CRITICAL_PRESSURE;
        assertEquals(0, limit0.scale(100));
        assertEquals(0L, limit0.scale(1000L));

        MemoryLimit limit50 = new MemoryLimit(50);
        assertEquals(0, limit50.scale(0));
        assertEquals(0L, limit50.scale(0L));
        assertEquals(50, limit50.scale(100));
        assertEquals(500L, limit50.scale(1000L));

        MemoryLimit limit150 = new MemoryLimit(150);
        assertEquals(150, limit150.scale(100));
        assertEquals(1500L, limit150.scale(1000L));

        // Saturated scaling at boundary limits.
        assertEquals(Integer.MAX_VALUE, limit150.scale(Integer.MAX_VALUE));
        assertEquals(Long.MAX_VALUE, limit150.scale(Long.MAX_VALUE));

        // Scaling large long values without premature saturation.
        long largeBaseline = 100_000_000_000_000_000L;
        assertEquals(largeBaseline, MemoryLimit.DEFAULT.scale(largeBaseline));
        assertEquals(50_000_000_000_000_000L, limit50.scale(largeBaseline));
    }

    @Test
    public void testEqualsAndHashCode() {
        MemoryLimit limitA = new MemoryLimit(75);
        MemoryLimit limitB = new MemoryLimit(75);
        MemoryLimit limitC = new MemoryLimit(80);

        assertEquals(limitA, limitB);
        assertEquals(limitA.hashCode(), limitB.hashCode());
        assertNotEquals(limitA, limitC);
    }
}

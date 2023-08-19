// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 *  Tests for FrameMetricsStore.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FrameMetricsStoreTest {
    @Test
    public void addFrameMeasurementTest() {
        FrameMetricsStore store = new FrameMetricsStore();

        store.addFrameMeasurement(10_000_000L, false);
        store.addFrameMeasurement(12_000_000L, false);
        store.addFrameMeasurement(20_000_000L, true);
        store.addFrameMeasurement(8_000_000L, true);

        JankMetrics metrics = store.takeMetrics();

        assertArrayEquals(new long[] {10_000_000L, 12_000_000L, 20_000_000L, 8_000_000L},
                metrics.durationsNs);
        assertArrayEquals(new boolean[] {false, false, true, true}, metrics.isJanky);

        metrics = store.takeMetrics();
        assertEquals(0, metrics.durationsNs.length);
    }

    @Test
    public void takeMetrics_getMetricsWithoutAnyFrames() {
        FrameMetricsStore store = new FrameMetricsStore();
        JankMetrics metrics = store.takeMetrics();

        assertEquals(0, metrics.durationsNs.length);
    }
}

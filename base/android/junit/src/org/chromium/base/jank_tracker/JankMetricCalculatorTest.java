// Copyright 2021 The Chromium Authors. All rights reserved.
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
 *  Tests for JankMetricCalculator.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class JankMetricCalculatorTest {
    @Test
    public void getJankBurstDurationsTest() {
        FrameMetricsStore store = new FrameMetricsStore();
        store.startTrackingScenario(JankScenario.PERIODIC_REPORTING);

        store.addFrameMeasurement(1_000_000_000L, 15_000_000L, 1);
        store.addFrameMeasurement(1_020_000_000L, 15_000_000L, 0);
        store.addFrameMeasurement(1_040_000_000L, 50_000_000L, 1); // Burst starts here.
        store.addFrameMeasurement(1_060_000_000L, 30_000_000L, 1);
        store.addFrameMeasurement(1_080_000_000L, 10_000_000L, 2);
        store.addFrameMeasurement(1_120_000_000L, 30_000_000L, 0); // Burst ends here.
        store.addFrameMeasurement(1_100_000_000L, 10_000_000L, 0);

        FrameMetrics frameMetrics = store.stopTrackingScenario(JankScenario.PERIODIC_REPORTING);
        JankMetrics jankMetrics = JankMetricCalculator.calculateJankMetrics(frameMetrics);

        assertArrayEquals(new long[] {120_000_000L}, jankMetrics.jankBurstsNs);
        assertEquals(5, jankMetrics.skippedFrames);
    }

    @Test
    public void getJankBurstDurationsTest_TwoBursts() {
        FrameMetricsStore store = new FrameMetricsStore();
        store.startTrackingScenario(JankScenario.PERIODIC_REPORTING);

        store.addFrameMeasurement(1_000_000_000L, 50_000_000L, 0); // Burst starts here.
        store.addFrameMeasurement(1_020_000_000L, 50_000_000L, 0);
        store.addFrameMeasurement(1_040_000_000L, 10_000_000L, 0);
        store.addFrameMeasurement(1_060_000_000L, 50_000_000L, 0); // Burst ends here.
        store.addFrameMeasurement(1_080_000_000L, 10_000_000L, 0);
        store.addFrameMeasurement(1_100_000_000L, 10_000_000L, 0);
        store.addFrameMeasurement(1_120_000_000L, 50_000_000L, 0); // Burst starts here.
        store.addFrameMeasurement(1_140_000_000L, 50_000_000L, 0);
        store.addFrameMeasurement(1_160_000_000L, 50_000_000L, 0); // Burst ends here.

        FrameMetrics frameMetrics = store.stopTrackingScenario(JankScenario.PERIODIC_REPORTING);
        JankMetrics jankMetrics = JankMetricCalculator.calculateJankMetrics(frameMetrics);

        assertArrayEquals(new long[] {160_000_000L, 150_000_000L}, jankMetrics.jankBurstsNs);
    }

    @Test
    public void getJankBurstDurationsTest_OneLongBurst() {
        FrameMetricsStore store = new FrameMetricsStore();
        store.startTrackingScenario(JankScenario.PERIODIC_REPORTING);

        store.addFrameMeasurement(1_000_000_000L, 50_000_000L, 0); // Burst starts here.
        store.addFrameMeasurement(1_020_000_000L, 10_000_000L, 0);
        store.addFrameMeasurement(1_040_000_000L, 50_000_000L, 0);
        store.addFrameMeasurement(1_060_000_000L, 10_000_000L, 0);
        store.addFrameMeasurement(1_080_000_000L, 50_000_000L, 0);
        store.addFrameMeasurement(1_100_000_000L, 10_000_000L, 0);
        store.addFrameMeasurement(1_120_000_000L, 50_000_000L, 0);
        store.addFrameMeasurement(1_140_000_000L, 10_000_000L, 0);
        store.addFrameMeasurement(1_160_000_000L, 50_000_000L, 0); // Burst ends here.

        FrameMetrics frameMetrics = store.stopTrackingScenario(JankScenario.PERIODIC_REPORTING);
        JankMetrics jankMetrics = JankMetricCalculator.calculateJankMetrics(frameMetrics);

        assertArrayEquals(new long[] {290_000_000L}, jankMetrics.jankBurstsNs);
    }

    @Test
    public void getJankBurstDurationsTest_ThreeBursts_WithNonConsecutiveFrames() {
        FrameMetricsStore store = new FrameMetricsStore();
        store.startTrackingScenario(JankScenario.PERIODIC_REPORTING);

        store.addFrameMeasurement(1_000_000_000L, 50_000_000L, 0); // Burst starts here.
        store.addFrameMeasurement(1_020_000_000L, 50_000_000L, 0);
        store.addFrameMeasurement(1_040_000_000L, 50_000_000L, 0); // Burst ends here.
        store.addFrameMeasurement(
                2_000_000_000L, 50_000_000L, 0); // Burst starts here (~1000ms passed).
        store.addFrameMeasurement(2_020_000_000L, 50_000_000L, 0);
        store.addFrameMeasurement(2_040_000_000L, 10_000_000L, 0);
        store.addFrameMeasurement(2_060_000_000L, 50_000_000L, 0); // Burst ends here.
        store.addFrameMeasurement(3_000_000_000L, 10_000_000L, 0);
        store.addFrameMeasurement(3_020_000_000L, 50_000_000L, 0); // Burst starts and ends here.
        store.addFrameMeasurement(3_040_000_000L, 10_000_000L, 0);

        FrameMetrics frameMetrics = store.stopTrackingScenario(JankScenario.PERIODIC_REPORTING);
        JankMetrics jankMetrics = JankMetricCalculator.calculateJankMetrics(frameMetrics);

        assertArrayEquals(
                new long[] {150_000_000L, 160_000_000L, 50_000_000L}, jankMetrics.jankBurstsNs);
    }
}

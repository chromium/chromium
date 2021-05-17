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
 *  Tests for JankMetricMeasurement.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class JankMetricMeasurementTest {
    @Test
    public void addFrameMeasurementTest() {
        JankMetricMeasurement measurement = new JankMetricMeasurement();

        measurement.addFrameMeasurement(1_000_000_000L, 10_000_000L, 0);
        measurement.addFrameMeasurement(1_020_000_000L, 12_000_000L, 0);
        measurement.addFrameMeasurement(1_040_000_000L, 20_000_000L, 0);
        measurement.addFrameMeasurement(1_060_000_000L, 8_000_000L, 0);

        assertArrayEquals(new long[] {10_000_000L, 12_000_000L, 20_000_000L, 8_000_000L},
                measurement.getMetrics().getDurations());
    }

    @Test
    public void clearTest() {
        JankMetricMeasurement measurement = new JankMetricMeasurement();

        measurement.addFrameMeasurement(1_000_000_000L, 10_000_000L, 0);

        measurement.clear();

        assertEquals(0, measurement.getMetrics().getDurations().length);
    }

    @Test
    public void getJankBurstDurationsTest() {
        JankMetricMeasurement measurement = new JankMetricMeasurement();

        measurement.addFrameMeasurement(1_000_000_000L, 15_000_000L, 0);
        measurement.addFrameMeasurement(1_020_000_000L, 15_000_000L, 0);
        measurement.addFrameMeasurement(1_040_000_000L, 50_000_000L, 0); // Burst starts here.
        measurement.addFrameMeasurement(1_060_000_000L, 30_000_000L, 0);
        measurement.addFrameMeasurement(1_080_000_000L, 10_000_000L, 0);
        measurement.addFrameMeasurement(1_100_000_000L, 30_000_000L, 0); // Burst ends here.
        measurement.addFrameMeasurement(1_120_000_000L, 10_000_000L, 0);

        assertArrayEquals(new long[] {120_000_000L}, measurement.getMetrics().getJankBursts());
    }

    @Test
    public void getJankBurstDurationsTest_TwoBursts() {
        JankMetricMeasurement measurement = new JankMetricMeasurement();

        measurement.addFrameMeasurement(1_000_000_000L, 50_000_000L, 0); // Burst starts here.
        measurement.addFrameMeasurement(1_020_000_000L, 50_000_000L, 0);
        measurement.addFrameMeasurement(1_040_000_000L, 10_000_000L, 0);
        measurement.addFrameMeasurement(1_060_000_000L, 50_000_000L, 0); // Burst ends here.
        measurement.addFrameMeasurement(1_080_000_000L, 10_000_000L, 0);
        measurement.addFrameMeasurement(1_100_000_000L, 10_000_000L, 0);
        measurement.addFrameMeasurement(1_120_000_000L, 50_000_000L, 0); // Burst starts here.
        measurement.addFrameMeasurement(1_140_000_000L, 50_000_000L, 0);
        measurement.addFrameMeasurement(1_160_000_000L, 50_000_000L, 0); // Burst ends here.

        assertArrayEquals(
                new long[] {160_000_000L, 150_000_000L}, measurement.getMetrics().getJankBursts());
    }

    @Test
    public void getJankBurstDurationsTest_OneLongBurst() {
        JankMetricMeasurement measurement = new JankMetricMeasurement();

        measurement.addFrameMeasurement(1_000_000_000L, 50_000_000L, 0); // Burst starts here.
        measurement.addFrameMeasurement(1_020_000_000L, 10_000_000L, 0);
        measurement.addFrameMeasurement(1_040_000_000L, 50_000_000L, 0);
        measurement.addFrameMeasurement(1_060_000_000L, 10_000_000L, 0);
        measurement.addFrameMeasurement(1_080_000_000L, 50_000_000L, 0);
        measurement.addFrameMeasurement(1_100_000_000L, 10_000_000L, 0);
        measurement.addFrameMeasurement(1_120_000_000L, 50_000_000L, 0);
        measurement.addFrameMeasurement(1_140_000_000L, 10_000_000L, 0);
        measurement.addFrameMeasurement(1_160_000_000L, 50_000_000L, 0); // Burst ends here.

        assertArrayEquals(new long[] {290_000_000L}, measurement.getMetrics().getJankBursts());
    }

    @Test
    public void getJankBurstDurationsTest_ThreeBursts_WithNonConsecutiveFrames() {
        JankMetricMeasurement measurement = new JankMetricMeasurement();

        measurement.addFrameMeasurement(1_000_000_000L, 50_000_000L, 0); // Burst starts here.
        measurement.addFrameMeasurement(1_020_000_000L, 50_000_000L, 0);
        measurement.addFrameMeasurement(1_040_000_000L, 50_000_000L, 0); // Burst ends here.
        measurement.addFrameMeasurement(
                2_000_000_000L, 50_000_000L, 0); // Burst starts here (~1000ms passed).
        measurement.addFrameMeasurement(2_020_000_000L, 50_000_000L, 0);
        measurement.addFrameMeasurement(2_040_000_000L, 10_000_000L, 0);
        measurement.addFrameMeasurement(2_060_000_000L, 50_000_000L, 0); // Burst ends here.
        measurement.addFrameMeasurement(3_000_000_000L, 10_000_000L, 0);
        measurement.addFrameMeasurement(
                3_020_000_000L, 50_000_000L, 0); // Burst starts and ends here.
        measurement.addFrameMeasurement(3_040_000_000L, 10_000_000L, 0);

        assertArrayEquals(new long[] {150_000_000L, 160_000_000L, 50_000_000L},
                measurement.getMetrics().getJankBursts());
    }
}

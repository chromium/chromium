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
 *  Tests for FrameMetricsStore.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FrameMetricsStoreTest {
    @Test
    public void addFrameMeasurementTest() {
        FrameMetricsStore store = new FrameMetricsStore();

        store.startTrackingScenario(JankScenario.PERIODIC_REPORTING);

        store.addFrameMeasurement(1_000_000_000L, 10_000_000L, 0);
        store.addFrameMeasurement(1_020_000_000L, 12_000_000L, 1);
        store.addFrameMeasurement(1_040_000_000L, 20_000_000L, 2);
        store.addFrameMeasurement(1_060_000_000L, 8_000_000L, 0);

        FrameMetrics scenarioMetrics = store.stopTrackingScenario(JankScenario.PERIODIC_REPORTING);

        assertArrayEquals(
                new Long[] {1_000_000_000L, 1_020_000_000L, 1_040_000_000L, 1_060_000_000L},
                scenarioMetrics.timestampsNs);
        assertArrayEquals(new Long[] {10_000_000L, 12_000_000L, 20_000_000L, 8_000_000L},
                scenarioMetrics.durationsNs);
        assertArrayEquals(new Integer[] {0, 1, 2, 0}, scenarioMetrics.skippedFrames);
    }

    @Test
    public void addFrameMeasurementTest_MultipleScenarios() {
        JankMetricCalculator measurement = new JankMetricCalculator();

        FrameMetricsStore store = new FrameMetricsStore();

        store.startTrackingScenario(JankScenario.PERIODIC_REPORTING);

        store.addFrameMeasurement(1_000_000_000L, 10_000_000L, 0);
        store.addFrameMeasurement(1_020_000_000L, 12_000_000L, 0);

        FrameMetrics periodicReportingMetrics =
                store.stopTrackingScenario(JankScenario.PERIODIC_REPORTING);
        store.startTrackingScenario(JankScenario.OMNIBOX_FOCUS);

        store.addFrameMeasurement(1_040_000_000L, 20_000_000L, 0);
        store.addFrameMeasurement(1_060_000_000L, 8_000_000L, 0);

        FrameMetrics omniboxMetrics = store.stopTrackingScenario(JankScenario.OMNIBOX_FOCUS);

        assertArrayEquals(
                new Long[] {10_000_000L, 12_000_000L}, periodicReportingMetrics.durationsNs);
        assertArrayEquals(new Long[] {20_000_000L, 8_000_000L}, omniboxMetrics.durationsNs);
    }

    @Test
    public void addFrameMeasurement_MultipleOverlappingScenarios() {
        JankMetricCalculator measurement = new JankMetricCalculator();

        FrameMetricsStore store = new FrameMetricsStore();

        store.addFrameMeasurement(1_000_000_000L, 15_000_000L, 0);

        store.startTrackingScenario(JankScenario.PERIODIC_REPORTING);

        store.addFrameMeasurement(1_020_000_000L, 15_000_000L, 0);
        store.addFrameMeasurement(1_040_000_000L, 50_000_000L, 0);
        store.addFrameMeasurement(1_060_000_000L, 30_000_000L, 0);

        store.startTrackingScenario(JankScenario.OMNIBOX_FOCUS);

        store.addFrameMeasurement(1_080_000_000L, 10_000_000L, 0);
        store.addFrameMeasurement(1_100_000_000L, 30_000_000L, 0);

        FrameMetrics periodicReportingMetrics =
                store.stopTrackingScenario(JankScenario.PERIODIC_REPORTING);

        store.addFrameMeasurement(1_120_000_000L, 10_000_000L, 0);

        FrameMetrics omniboxMetrics = store.stopTrackingScenario(JankScenario.OMNIBOX_FOCUS);

        assertArrayEquals(
                new Long[] {15_000_000L, 50_000_000L, 30_000_000L, 10_000_000L, 30_000_000L},
                periodicReportingMetrics.durationsNs);
        assertArrayEquals(
                new Long[] {10_000_000L, 30_000_000L, 10_000_000L}, omniboxMetrics.durationsNs);
    }

    @Test
    public void addFrameMeasurementTest_MultipleStartCallsAreIgnored() {
        JankMetricCalculator measurement = new JankMetricCalculator();

        FrameMetricsStore store = new FrameMetricsStore();

        store.startTrackingScenario(JankScenario.PERIODIC_REPORTING);

        store.addFrameMeasurement(1_000_000_000L, 10_000_000L, 0);
        store.addFrameMeasurement(1_020_000_000L, 12_000_000L, 0);

        // Any duplicate calls to startTracking should be ignored.
        store.startTrackingScenario(JankScenario.PERIODIC_REPORTING);

        store.addFrameMeasurement(1_040_000_000L, 20_000_000L, 0);

        store.startTrackingScenario(JankScenario.PERIODIC_REPORTING);

        store.addFrameMeasurement(1_060_000_000L, 8_000_000L, 0);

        FrameMetrics periodicReportingMetrics =
                store.stopTrackingScenario(JankScenario.PERIODIC_REPORTING);

        // The returned metrics should begin at the first call to startTrackingScenario.
        assertArrayEquals(new Long[] {10_000_000L, 12_000_000L, 20_000_000L, 8_000_000L},
                periodicReportingMetrics.durationsNs);
    }

    @Test
    public void stopTrackingScenario_StopWithoutAnyFrames() {
        JankMetricCalculator measurement = new JankMetricCalculator();

        FrameMetricsStore store = new FrameMetricsStore();

        store.startTrackingScenario(JankScenario.PERIODIC_REPORTING);
        FrameMetrics periodicReportingMetrics =
                store.stopTrackingScenario(JankScenario.PERIODIC_REPORTING);

        store.startTrackingScenario(JankScenario.OMNIBOX_FOCUS);
        FrameMetrics omniboxMetrics = store.stopTrackingScenario(JankScenario.OMNIBOX_FOCUS);

        assertEquals(0, periodicReportingMetrics.durationsNs.length);
        assertEquals(0, omniboxMetrics.durationsNs.length);
    }

    @Test
    public void stopTrackingScenario_EmptyScenarioAfterOneFrame() {
        JankMetricCalculator measurement = new JankMetricCalculator();

        FrameMetricsStore store = new FrameMetricsStore();

        // Start a scenario just to start recording.
        store.startTrackingScenario(JankScenario.OMNIBOX_FOCUS);

        // Add a frame measurement.
        store.addFrameMeasurement(1_060_000_000L, 8_000_000L, 0);

        // Start and stop tracking scenario.
        store.startTrackingScenario(JankScenario.PERIODIC_REPORTING);
        FrameMetrics periodicReportingMetrics =
                store.stopTrackingScenario(JankScenario.PERIODIC_REPORTING);

        // The resulting metrics should be empty.
        assertEquals(0, periodicReportingMetrics.durationsNs.length);
    }

    @Test
    public void stopTrackingScenario_StopWithoutStartingReturnsEmptyMetrics() {
        JankMetricCalculator measurement = new JankMetricCalculator();

        FrameMetricsStore store = new FrameMetricsStore();

        FrameMetrics periodicReportingMetrics =
                store.stopTrackingScenario(JankScenario.PERIODIC_REPORTING);

        FrameMetrics omniboxMetrics = store.stopTrackingScenario(JankScenario.OMNIBOX_FOCUS);

        assertEquals(0, periodicReportingMetrics.durationsNs.length);
        assertEquals(0, omniboxMetrics.durationsNs.length);
    }

    @Test
    public void stopTrackingScenario_ClearsUnneededFrames() {
        FrameMetricsStore store = new FrameMetricsStore();

        store.addFrameMeasurement(1_000_000_000L, 15_000_000L, 0);

        store.startTrackingScenario(JankScenario.PERIODIC_REPORTING);

        store.addFrameMeasurement(1_020_000_000L, 15_000_000L, 0);
        store.addFrameMeasurement(1_040_000_000L, 50_000_000L, 0);
        // This frame should be kept after PERIODIC_REPORTING ends because OMNIBOX_FOCUS uses its
        // timestamp to track the scenario's start.
        store.addFrameMeasurement(1_060_000_000L, 33_333_333L, 0);

        store.startTrackingScenario(JankScenario.OMNIBOX_FOCUS);

        store.addFrameMeasurement(1_080_000_000L, 10_000_000L, 0);
        store.addFrameMeasurement(1_100_000_000L, 30_000_000L, 0);

        store.stopTrackingScenario(JankScenario.PERIODIC_REPORTING);
        FrameMetrics storedMetricsAfterPeriodic = store.getAllStoredMetricsForTesting();

        store.addFrameMeasurement(1_120_000_000L, 10_000_000L, 0);

        store.stopTrackingScenario(JankScenario.OMNIBOX_FOCUS);
        FrameMetrics storedMetricsAfterOmnibox = store.getAllStoredMetricsForTesting();

        // When we stop tracking periodic reporting we should remove all frames that aren't used by
        // any other scenarios.
        assertArrayEquals(new Long[] {33_333_333L, 10_000_000L, 30_000_000L},
                storedMetricsAfterPeriodic.durationsNs);
        // When we stop tracking omnibox there are no other scenarios being tracked, so we clear all
        // frame data.
        assertEquals(0, storedMetricsAfterOmnibox.durationsNs.length);
    }
}

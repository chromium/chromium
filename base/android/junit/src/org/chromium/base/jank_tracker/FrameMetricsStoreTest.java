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

/** Tests for FrameMetricsStore. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FrameMetricsStoreTest {
    @Test
    public void addFrameMeasurementTest() {
        FrameMetricsStore store = new FrameMetricsStore();
        store.initialize();

        store.startTrackingScenario(JankScenario.NEW_TAB_PAGE);

        long frame_start_vsync_ts = 0;
        store.addFrameMeasurement(10_000_000L, false, frame_start_vsync_ts);
        store.addFrameMeasurement(12_000_000L, false, frame_start_vsync_ts);
        store.addFrameMeasurement(20_000_000L, true, frame_start_vsync_ts);
        store.addFrameMeasurement(8_000_000L, true, frame_start_vsync_ts);

        JankMetrics metrics = store.stopTrackingScenario(JankScenario.NEW_TAB_PAGE);

        assertArrayEquals(
                new long[] {10_000_000L, 12_000_000L, 20_000_000L, 8_000_000L},
                metrics.durationsNs);
        assertArrayEquals(new boolean[] {false, false, true, true}, metrics.isJanky);

        metrics = store.stopTrackingScenario(JankScenario.NEW_TAB_PAGE);
        assertEquals(0, metrics.durationsNs.length);
    }

    @Test
    public void takeMetrics_getMetricsWithoutAnyFrames() {
        FrameMetricsStore store = new FrameMetricsStore();
        store.initialize();
        store.startTrackingScenario(JankScenario.NEW_TAB_PAGE);
        JankMetrics metrics = store.stopTrackingScenario(JankScenario.NEW_TAB_PAGE);

        assertEquals(0, metrics.durationsNs.length);
    }

    @Test
    public void concurrentScenarios() {
        // We want to test 2 things.
        // 1) that concurrent scenarios get the correct frames
        // 2) that the deletion logic runs correctly. Note however that deletion logic is not
        // actually public behaviour but we just want this test to explicitly exercise it to
        // uncover potential bugs.
        FrameMetricsStore store = new FrameMetricsStore();
        store.initialize();

        store.startTrackingScenario(JankScenario.NEW_TAB_PAGE);

        long frame_start_vsync_ts = 1_000_000L;
        store.addFrameMeasurement(10_000_000L, false, frame_start_vsync_ts);
        store.addFrameMeasurement(12_000_000L, false, frame_start_vsync_ts + 1);
        store.startTrackingScenario(JankScenario.FEED_SCROLLING);
        store.addFrameMeasurement(20_000_000L, true, frame_start_vsync_ts + 2);
        store.addFrameMeasurement(8_000_000L, true, frame_start_vsync_ts + 3);

        // Stop NEW_TAB_PAGE and now the first two frames will be deleted from the
        // FrameMetricsStore().
        JankMetrics metrics = store.stopTrackingScenario(JankScenario.NEW_TAB_PAGE);

        assertArrayEquals(
                new long[] {10_000_000L, 12_000_000L, 20_000_000L, 8_000_000L},
                metrics.durationsNs);
        assertArrayEquals(new boolean[] {false, false, true, true}, metrics.isJanky);

        metrics = store.stopTrackingScenario(JankScenario.NEW_TAB_PAGE);
        assertEquals(0, metrics.durationsNs.length);

        // Only after that will we stop FEED_SCROLLING and we should only see the last two frames.
        metrics = store.stopTrackingScenario(JankScenario.FEED_SCROLLING);
        assertArrayEquals(new long[] {20_000_000L, 8_000_000L}, metrics.durationsNs);
        assertArrayEquals(new boolean[] {true, true}, metrics.isJanky);
    }
}

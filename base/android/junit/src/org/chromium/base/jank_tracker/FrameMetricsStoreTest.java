// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for FrameMetricsStore. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FrameMetricsStoreTest {
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Test
    public void addFrameMeasurementTest() {
        FrameMetricsStore store = new FrameMetricsStore();
        store.initialize();

        store.startTrackingScenario(JankScenario.NEW_TAB_PAGE);

        long frame_start_vsync_ts = 0;
        store.addFrameMeasurement(10_000_000L, 0, frame_start_vsync_ts);
        store.addFrameMeasurement(12_000_000L, 0, frame_start_vsync_ts);
        store.addFrameMeasurement(20_000_000L, 1, frame_start_vsync_ts);
        store.addFrameMeasurement(8_000_000L, 2, frame_start_vsync_ts);

        JankMetrics metrics = store.stopTrackingScenario(JankScenario.NEW_TAB_PAGE);

        assertArrayEquals(
                new long[] {10_000_000L, 12_000_000L, 20_000_000L, 8_000_000L},
                metrics.durationsNs);
        assertArrayEquals(new int[] {0, 0, 1, 2}, metrics.missedVsyncs);

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
        store.addFrameMeasurement(10_000_000L, 0, frame_start_vsync_ts);
        store.addFrameMeasurement(12_000_000L, 0, frame_start_vsync_ts + 1);
        store.startTrackingScenario(JankScenario.FEED_SCROLLING);
        store.addFrameMeasurement(20_000_000L, 2, frame_start_vsync_ts + 2);
        store.addFrameMeasurement(8_000_000L, 1, frame_start_vsync_ts + 3);

        // Stop NEW_TAB_PAGE and now the first two frames will be deleted from the
        // FrameMetricsStore().
        JankMetrics metrics = store.stopTrackingScenario(JankScenario.NEW_TAB_PAGE);

        assertArrayEquals(
                new long[] {10_000_000L, 12_000_000L, 20_000_000L, 8_000_000L},
                metrics.durationsNs);
        assertArrayEquals(new int[] {0, 0, 2, 1}, metrics.missedVsyncs);

        metrics = store.stopTrackingScenario(JankScenario.NEW_TAB_PAGE);
        assertEquals(0, metrics.durationsNs.length);

        // Only after that will we stop FEED_SCROLLING and we should only see the last two frames.
        metrics = store.stopTrackingScenario(JankScenario.FEED_SCROLLING);
        assertArrayEquals(new long[] {20_000_000L, 8_000_000L}, metrics.durationsNs);
        assertArrayEquals(new int[] {2, 1}, metrics.missedVsyncs);
    }

    @Test
    public void restartScenario() {
        // This test is setup to mainly test removeUnusedFrames() method codepaths.
        FrameMetricsStore store = new FrameMetricsStore();
        store.initialize();

        store.startTrackingScenario(JankScenario.WEBVIEW_SCROLLING);

        long frame_start_vsync_ts = 1_000_000L;
        store.addFrameMeasurement(10_000_000L, 0, frame_start_vsync_ts);
        store.addFrameMeasurement(12_000_000L, 1, frame_start_vsync_ts + 1);

        JankMetrics metrics = store.stopTrackingScenario(JankScenario.WEBVIEW_SCROLLING);

        assertArrayEquals(new long[] {10_000_000L, 12_000_000L}, metrics.durationsNs);
        assertArrayEquals(new int[] {0, 1}, metrics.missedVsyncs);

        store.startTrackingScenario(JankScenario.WEBVIEW_SCROLLING);

        store.addFrameMeasurement(10_000_100L, 2, frame_start_vsync_ts + 2);
        store.addFrameMeasurement(11_000_100L, 0, frame_start_vsync_ts + 3);

        store.startTrackingScenario(JankScenario.NEW_TAB_PAGE);

        store.addFrameMeasurement(12_000_100L, 0, frame_start_vsync_ts + 4);

        metrics = store.stopTrackingScenario(JankScenario.WEBVIEW_SCROLLING);
        assertArrayEquals(new long[] {10_000_100L, 11_000_100L, 12_000_100L}, metrics.durationsNs);
        assertArrayEquals(new int[] {2, 0, 0}, metrics.missedVsyncs);

        metrics = store.stopTrackingScenario(JankScenario.NEW_TAB_PAGE);
        assertArrayEquals(new long[] {12_000_100L}, metrics.durationsNs);
        assertArrayEquals(new int[] {0}, metrics.missedVsyncs);
    }

    @Test
    public void startPendingScenarioBeforeScenarioEnd() {
        FrameMetricsStore store = new FrameMetricsStore();
        store.initialize();

        store.startTrackingScenario(JankScenario.WEBVIEW_SCROLLING);

        long now = TimeUtils.uptimeMillis();

        long[] frame_timestamps_ns =
                new long[] {
                    now * TimeUtils.NANOSECONDS_PER_MILLISECOND, // Frame1 start time
                    (now + 1) * TimeUtils.NANOSECONDS_PER_MILLISECOND, // Frame2 start time
                    (now + 2) * TimeUtils.NANOSECONDS_PER_MILLISECOND, // scenario end time
                    (now + 3) * TimeUtils.NANOSECONDS_PER_MILLISECOND, // new scenario start time
                    (now + 4) * TimeUtils.NANOSECONDS_PER_MILLISECOND, // Frame3 start time
                    (now + 5) * TimeUtils.NANOSECONDS_PER_MILLISECOND, // Frame4 start time
                };

        store.addFrameMeasurement(10_000_000L, 0, frame_timestamps_ns[0]);
        store.addFrameMeasurement(12_000_000L, 3, frame_timestamps_ns[1]);

        // This start scenario shouldn't be blanket ignored instead this should
        // trigger start of the scenario after stop scenario call.
        mFakeTimeTestRule.advanceMillis(
                (frame_timestamps_ns[3] / TimeUtils.NANOSECONDS_PER_MILLISECOND) - now);
        store.startTrackingScenario(JankScenario.WEBVIEW_SCROLLING);

        // The end time of scenario is before the start time of last start calls,
        // this can occur in the field as well since we delay stopTrackingScenario
        // to receive metrics for all the frames in given time range.
        JankMetrics metrics =
                store.stopTrackingScenario(JankScenario.WEBVIEW_SCROLLING, frame_timestamps_ns[2]);

        assertArrayEquals(new long[] {10_000_000L, 12_000_000L}, metrics.durationsNs);
        assertArrayEquals(new int[] {0, 3}, metrics.missedVsyncs);

        store.addFrameMeasurement(10_000_100L, 2, frame_timestamps_ns[4]);
        store.addFrameMeasurement(11_000_100L, 0, frame_timestamps_ns[5]);

        metrics = store.stopTrackingScenario(JankScenario.WEBVIEW_SCROLLING);

        assertArrayEquals(new long[] {10_000_100L, 11_000_100L}, metrics.durationsNs);
        assertArrayEquals(new int[] {2, 0}, metrics.missedVsyncs);
    }

    @Test
    public void multipleStartScenarioBeforeScenarioEnd() {
        /*
         * Testing a scenario where might have a complete scroll within the delay period of
         * processing a finishTrackingScenario request. We typically wait for ~100ms to receive
         * frame metrics corresponding to last few frames in scenario. This test simulates
         * scenario depicted below. '|______|' is the actual scroll start and end, while
         * '-----------|' is the delay in processing finishTrackingScenario call i.e. when we
         * call stopTrackingScenario.
         *
         * Scroll1: |________________________|-----------|
         * Scroll2:                           |____|-----------|
         * Scroll3:                                    |_____________|-----------|
         *
         * As per current architecture expectation is we will loose metrics for scroll2 and scroll3,
         * but this should be fine since the scenario is pretty unrealistic so shouldn't occur often
         * in field data.
         */
        FrameMetricsStore store = new FrameMetricsStore();
        store.initialize();

        store.startTrackingScenario(JankScenario.WEBVIEW_SCROLLING);

        long now = TimeUtils.uptimeMillis();

        long[] frame_timestamps_ns =
                new long[] {
                    now * TimeUtils.NANOSECONDS_PER_MILLISECOND, // Frame1 start time
                    (now + 1) * TimeUtils.NANOSECONDS_PER_MILLISECOND, // Frame2 start time
                    (now + 2) * TimeUtils.NANOSECONDS_PER_MILLISECOND, // Scroll1 end time
                    (now + 3) * TimeUtils.NANOSECONDS_PER_MILLISECOND, // Scroll2 start time
                    (now + 4) * TimeUtils.NANOSECONDS_PER_MILLISECOND, // Frame3 start time
                    (now + 5) * TimeUtils.NANOSECONDS_PER_MILLISECOND, // Frame4 start time
                    (now + 6) * TimeUtils.NANOSECONDS_PER_MILLISECOND, // Scroll2 end time
                    (now + 7) * TimeUtils.NANOSECONDS_PER_MILLISECOND, // Scroll3 start time
                    (now + 8) * TimeUtils.NANOSECONDS_PER_MILLISECOND, // Frame5 start time
                    (now + 9) * TimeUtils.NANOSECONDS_PER_MILLISECOND, // Frame6 start time
                    (now + 10) * TimeUtils.NANOSECONDS_PER_MILLISECOND, // Scroll3 end time
                };

        store.addFrameMeasurement(10_000_000L, 0, frame_timestamps_ns[0]); // Frame1
        store.addFrameMeasurement(12_000_000L, 1, frame_timestamps_ns[1]); // Frame2

        mFakeTimeTestRule.advanceMillis(
                (frame_timestamps_ns[3] / TimeUtils.NANOSECONDS_PER_MILLISECOND) - now);
        store.startTrackingScenario(JankScenario.WEBVIEW_SCROLLING); // Scroll2 start
        store.addFrameMeasurement(10_000_100L, 2, frame_timestamps_ns[4]); // Frame3
        store.addFrameMeasurement(11_000_100L, 0, frame_timestamps_ns[5]); // Frame4

        mFakeTimeTestRule.advanceMillis(
                (frame_timestamps_ns[7] / TimeUtils.NANOSECONDS_PER_MILLISECOND)
                        - (frame_timestamps_ns[3] / TimeUtils.NANOSECONDS_PER_MILLISECOND));
        store.startTrackingScenario(JankScenario.WEBVIEW_SCROLLING); // Scroll3 start
        store.addFrameMeasurement(10_000_100L, 1, frame_timestamps_ns[8]); // Frame5
        store.addFrameMeasurement(11_000_100L, 0, frame_timestamps_ns[9]); // Frame6

        // Scroll1 end.
        JankMetrics metrics =
                store.stopTrackingScenario(JankScenario.WEBVIEW_SCROLLING, frame_timestamps_ns[2]);

        assertArrayEquals(new long[] {10_000_000L, 12_000_000L}, metrics.durationsNs);
        assertArrayEquals(new int[] {0, 1}, metrics.missedVsyncs);

        // Scroll2 end.
        metrics =
                store.stopTrackingScenario(JankScenario.WEBVIEW_SCROLLING, frame_timestamps_ns[6]);

        assertArrayEquals(new long[] {}, metrics.durationsNs);
        assertArrayEquals(new int[] {}, metrics.missedVsyncs);

        // Scroll3 end.
        metrics =
                store.stopTrackingScenario(JankScenario.WEBVIEW_SCROLLING, frame_timestamps_ns[10]);

        assertArrayEquals(new long[] {}, metrics.durationsNs);
        assertArrayEquals(new int[] {}, metrics.missedVsyncs);
    }
}

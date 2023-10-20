// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.view.FrameMetrics;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for FrameMetricsListener. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FrameMetricsListenerTest {
    @Test
    public void testMetricRecording_OffByDefault() {
        FrameMetricsStore store = new FrameMetricsStore();
        store.initialize();

        FrameMetricsListener metricsListener = new FrameMetricsListener(store);
        FrameMetrics frameMetrics = mock(FrameMetrics.class);

        when(frameMetrics.getMetric(FrameMetrics.TOTAL_DURATION)).thenReturn(10_000_000L);
        store.startTrackingScenario(JankScenario.NEW_TAB_PAGE);

        metricsListener.onFrameMetricsAvailable(null, frameMetrics, 0);

        // By default metrics shouldn't be logged.
        Assert.assertEquals(
                0, store.stopTrackingScenario(JankScenario.NEW_TAB_PAGE).durationsNs.length);
        verifyNoMoreInteractions(frameMetrics);
    }

    @Test
    public void testMetricRecording_EnableRecording() {
        FrameMetricsStore store = new FrameMetricsStore();
        store.initialize();

        FrameMetricsListener metricsListener = new FrameMetricsListener(store);
        FrameMetrics frameMetrics = mock(FrameMetrics.class);

        when(frameMetrics.getMetric(FrameMetrics.TOTAL_DURATION)).thenReturn(10_000_000L);

        store.startTrackingScenario(JankScenario.NEW_TAB_PAGE);
        metricsListener.setIsListenerRecording(true);
        metricsListener.onFrameMetricsAvailable(null, frameMetrics, 0);

        Assert.assertArrayEquals(
                new long[] {10_000_000L},
                store.stopTrackingScenario(JankScenario.NEW_TAB_PAGE).durationsNs);
    }
}

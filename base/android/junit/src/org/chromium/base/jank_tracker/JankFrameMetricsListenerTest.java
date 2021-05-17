// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;

import android.view.FrameMetrics;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 *  Tests for JankFrameMetricsListener.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class JankFrameMetricsListenerTest {
    @Test
    public void testMetricRecording_OffByDefault() {
        JankMetricMeasurement measurement = new JankMetricMeasurement();
        JankFrameMetricsListener metricsListener = new JankFrameMetricsListener(measurement);
        FrameMetrics frameMetrics = mock(FrameMetrics.class);

        when(frameMetrics.getMetric(FrameMetrics.TOTAL_DURATION)).thenReturn(10_000_000L);

        metricsListener.onFrameMetricsAvailable(null, frameMetrics, 0);

        // By default metrics shouldn't be logged.
        Assert.assertEquals(0, measurement.getMetrics().getDurations().length);
        verifyZeroInteractions(frameMetrics);
    }

    @Test
    public void testMetricRecording_EnableRecording() {
        JankMetricMeasurement measurement = new JankMetricMeasurement();
        JankFrameMetricsListener metricsListener = new JankFrameMetricsListener(measurement);
        FrameMetrics frameMetrics = mock(FrameMetrics.class);

        when(frameMetrics.getMetric(FrameMetrics.TOTAL_DURATION)).thenReturn(10_000_000L);

        metricsListener.setIsListenerRecording(true);
        metricsListener.onFrameMetricsAvailable(null, frameMetrics, 0);

        Assert.assertArrayEquals(new long[] {10_000_000L}, measurement.getMetrics().getDurations());
    }
}

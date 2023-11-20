// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;

/** Tests for JankMetricUMARecorder. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class JankMetricUMARecorderTest {
    @Rule public JniMocker mocker = new JniMocker();

    @Mock JankMetricUMARecorder.Natives mNativeMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(JankMetricUMARecorderJni.TEST_HOOKS, mNativeMock);
    }

    @Test
    public void testRecordMetricsToNative() {
        long[] timestampsNs = new long[] {5L, 8L, 3L};
        long[] durationsNs = new long[] {5_000_000L, 8_000_000L, 30_000_000L};
        int[] missedVsyncs = new int[] {0, 0, 1};

        JankMetrics metric = new JankMetrics(timestampsNs, durationsNs, missedVsyncs);

        JankMetricUMARecorder.recordJankMetricsToUMA(metric, 0, 1000, 1);

        // Ensure that the relevant fields are sent down to native.
        verify(mNativeMock).recordJankMetrics(durationsNs, missedVsyncs, 0, 1000, 1);
    }

    @Test
    public void testRecordNullMetrics() {
        JankMetricUMARecorder.recordJankMetricsToUMA(null, 0, 0, 1);
        verify(mNativeMock, never())
                .recordJankMetrics(
                        ArgumentMatchers.any(),
                        ArgumentMatchers.any(),
                        ArgumentMatchers.anyLong(),
                        ArgumentMatchers.anyLong(),
                        ArgumentMatchers.anyInt());
    }
}

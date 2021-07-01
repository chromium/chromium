// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;

/**
 *  Tests for JankMetricUMARecorder.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class JankMetricUMARecorderTest {
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    JankMetricUMARecorder.Natives mNativeMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(JankMetricUMARecorderJni.TEST_HOOKS, mNativeMock);
    }

    @Test
    public void testRecordMetricsToNative() {
        long[] timestampsNs = new long[] {1_000_000_000L, 2_000_000_000L, 3_000_000_000L};
        long[] durationsNs = new long[] {5_000_000L, 8_000_000L, 30_000_000L};
        long[] jankBurstsNs = new long[] {30_000L};
        int missedFrames = 3;

        JankMetrics metric = new JankMetrics(timestampsNs, durationsNs, jankBurstsNs, missedFrames);

        JankMetricUMARecorder.recordJankMetricsToUMA(metric, JankScenario.OMNIBOX_FOCUS);

        // Ensure that the relevant fields are sent down to native.
        verify(mNativeMock)
                .recordJankMetrics("OmniboxFocus", durationsNs, jankBurstsNs, missedFrames);
    }
}

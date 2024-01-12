// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.os.Handler;
import android.os.Looper;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;

/** Tests for JankReportingRunnable. */
@RunWith(BaseRobolectricTestRunner.class)
public class JankReportingRunnableTest {
    ShadowLooper mShadowLooper;
    Handler mHandler;
    @Rule public JniMocker mocker = new JniMocker();

    @Mock JankMetricUMARecorder.Natives mNativeMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(JankMetricUMARecorderJni.TEST_HOOKS, mNativeMock);
        mShadowLooper = ShadowLooper.shadowMainLooper();
        mHandler = new Handler(Looper.getMainLooper());
    }

    @Test
    public void testStartTracking() {
        FrameMetricsStore metricsStore = Mockito.spy(new FrameMetricsStore());
        metricsStore.initialize();

        JankReportingRunnable reportingRunnable =
                new JankReportingRunnable(
                        metricsStore,
                        JankScenario.TAB_SWITCHER,
                        /* isStartingTracking= */ true,
                        mHandler,
                        null);
        reportingRunnable.run();

        verify(metricsStore).initialize();
        verify(metricsStore).startTrackingScenario(JankScenario.TAB_SWITCHER);
        verifyNoMoreInteractions(metricsStore);
    }

    @Test
    public void testStopTracking_withoutDelay() {
        FrameMetricsStore metricsStore = Mockito.spy(new FrameMetricsStore());
        metricsStore.initialize();

        JankReportingRunnable startReportingRunnable =
                new JankReportingRunnable(
                        metricsStore,
                        JankScenario.TAB_SWITCHER,
                        /* isStartingTracking= */ true,
                        mHandler,
                        null);
        startReportingRunnable.run();

        metricsStore.addFrameMeasurement(1_000_000L, 2, 1);

        JankReportingRunnable stopReportingRunnable =
                new JankReportingRunnable(
                        metricsStore,
                        JankScenario.TAB_SWITCHER,
                        /* isStartingTracking= */ false,
                        mHandler,
                        null);
        stopReportingRunnable.run();

        verify(metricsStore).initialize();
        verify(metricsStore).startTrackingScenario(JankScenario.TAB_SWITCHER);
        verify(metricsStore).stopTrackingScenario(JankScenario.TAB_SWITCHER);

        verify(mNativeMock)
                .recordJankMetrics(
                        new long[] {1_000_000L},
                        new int[] {2},
                        0L,
                        1L,
                        JankScenario.Type.TAB_SWITCHER);
    }

    @Test
    public void testStopTracking_withDelay() {
        final long frameTime = 50L * TimeUtils.NANOSECONDS_PER_MILLISECOND;
        FrameMetricsStore metricsStore = Mockito.spy(new FrameMetricsStore());
        metricsStore.initialize();

        JankEndScenarioTime endScenarioTime = JankEndScenarioTime.endAt(frameTime);
        Assert.assertTrue(endScenarioTime != null);
        Assert.assertEquals(endScenarioTime.endScenarioTimeNs, frameTime);

        JankReportingRunnable startReportingRunnable =
                new JankReportingRunnable(
                        metricsStore,
                        JankScenario.TAB_SWITCHER,
                        /* isStartingTracking= */ true,
                        mHandler,
                        endScenarioTime);
        startReportingRunnable.run();

        metricsStore.addFrameMeasurement(1_000_000L, 2, 1 * TimeUtils.NANOSECONDS_PER_MILLISECOND);

        JankReportingRunnable stopReportingRunnable =
                new JankReportingRunnable(
                        metricsStore,
                        JankScenario.TAB_SWITCHER,
                        /* isStartingTracking= */ false,
                        mHandler,
                        endScenarioTime);
        stopReportingRunnable.run();

        // Add two frames, one added before the frame time of 50ms above and one after. The first
        // should be included and the second ignored.
        metricsStore.addFrameMeasurement(1_000_001L, 0, 5 * TimeUtils.NANOSECONDS_PER_MILLISECOND);
        metricsStore.addFrameMeasurement(
                1_000_002L, 1, (frameTime + 5) * TimeUtils.NANOSECONDS_PER_MILLISECOND);

        mShadowLooper.runOneTask();

        verify(metricsStore).initialize();
        verify(metricsStore).startTrackingScenario(JankScenario.TAB_SWITCHER);
        verify(metricsStore).stopTrackingScenario(JankScenario.TAB_SWITCHER, frameTime);

        verify(mNativeMock)
                .recordJankMetrics(
                        new long[] {1_000_000L, 1_000_001L},
                        new int[] {2, 0},
                        1L,
                        5L,
                        JankScenario.Type.TAB_SWITCHER);
    }

    @Test
    public void testStopTracking_emptyStoreShouldntRecordAnything() {
        // Create a store but don't add any measurements.
        FrameMetricsStore metricsStore = Mockito.spy(new FrameMetricsStore());
        metricsStore.initialize();

        JankReportingRunnable startReportingRunnable =
                new JankReportingRunnable(
                        metricsStore,
                        JankScenario.TAB_SWITCHER,
                        /* isStartingTracking= */ true,
                        mHandler,
                        null);
        startReportingRunnable.run();

        JankReportingRunnable stopReportingRunnable =
                new JankReportingRunnable(
                        metricsStore,
                        JankScenario.TAB_SWITCHER,
                        /* isStartingTracking= */ false,
                        mHandler,
                        null);
        stopReportingRunnable.run();

        verify(metricsStore).initialize();
        verify(metricsStore).startTrackingScenario(JankScenario.TAB_SWITCHER);
        verify(metricsStore).stopTrackingScenario(JankScenario.TAB_SWITCHER);

        // Native shouldn't be called when there are no measurements.
        verifyNoMoreInteractions(mNativeMock);
    }
}

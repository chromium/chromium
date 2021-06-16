// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.verifyZeroInteractions;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;

/**
 *  Tests for JankReportingRunnable.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class JankReportingRunnableTest {
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
    public void testStartTracking() {
        FrameMetricsStore metricsStore = Mockito.spy(new FrameMetricsStore());

        JankReportingRunnable reportingRunnable = new JankReportingRunnable(
                metricsStore, JankScenario.TAB_SWITCHER, /* isStartingTracking= */ true);
        reportingRunnable.run();

        verify(metricsStore).startTrackingScenario(JankScenario.TAB_SWITCHER);
        verifyNoMoreInteractions(metricsStore);
    }

    @Test
    public void testStopTracking() {
        FrameMetricsStore metricsStore = Mockito.spy(new FrameMetricsStore());

        JankReportingRunnable startReportingRunnable = new JankReportingRunnable(
                metricsStore, JankScenario.TAB_SWITCHER, /* isStartingTracking= */ true);
        startReportingRunnable.run();

        metricsStore.addFrameMeasurement(1_000_000L, 1_000L, 1);
        LibraryLoader.getInstance().setLibrariesLoadedForNativeTests();

        JankReportingRunnable stopReportingRunnable = new JankReportingRunnable(
                metricsStore, JankScenario.TAB_SWITCHER, /* isStartingTracking= */ false);
        stopReportingRunnable.run();

        verify(metricsStore).startTrackingScenario(JankScenario.TAB_SWITCHER);
        verify(metricsStore).stopTrackingScenario(JankScenario.TAB_SWITCHER);

        verify(mNativeMock).recordJankMetrics("TabSwitcher", new long[] {1_000L}, new long[0], 1);
    }

    @Test
    public void testStopTracking_emptyStoreShouldntRecordAnything() {
        // Create a store but don't add any measurements.
        FrameMetricsStore metricsStore = Mockito.spy(new FrameMetricsStore());

        JankReportingRunnable startReportingRunnable = new JankReportingRunnable(
                metricsStore, JankScenario.TAB_SWITCHER, /* isStartingTracking= */ true);
        startReportingRunnable.run();

        LibraryLoader.getInstance().setLibrariesLoadedForNativeTests();

        JankReportingRunnable stopReportingRunnable = new JankReportingRunnable(
                metricsStore, JankScenario.TAB_SWITCHER, /* isStartingTracking= */ false);
        stopReportingRunnable.run();

        verify(metricsStore).startTrackingScenario(JankScenario.TAB_SWITCHER);
        verify(metricsStore).stopTrackingScenario(JankScenario.TAB_SWITCHER);

        // Native shouldn't be called when there are no measurements.
        verifyZeroInteractions(mNativeMock);
    }
}

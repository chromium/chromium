// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for JankReportingScheduler. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class JankReportingSchedulerTest {
    ShadowLooper mShadowLooper;

    @Mock private FrameMetricsStore mFrameMetricsStore;

    JankReportingScheduler createJankReportingScheduler() {
        JankReportingScheduler scheduler = new JankReportingScheduler(mFrameMetricsStore);
        mShadowLooper = Shadow.extract(scheduler.getOrCreateHandler().getLooper());

        return scheduler;
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void jankScenarioTracking_startTracking() {
        JankReportingScheduler jankReportingScheduler = createJankReportingScheduler();

        jankReportingScheduler.startTrackingScenario(JankScenario.NEW_TAB_PAGE);

        // When first getting the handler we need to run the initialize on the handler.
        mShadowLooper.runOneTask();
        // Starting tracking posts a task to begin recording metrics in FrameMetricsStore.
        mShadowLooper.runOneTask();

        verify(mFrameMetricsStore).initialize();
        verify(mFrameMetricsStore).startTrackingScenario(JankScenario.NEW_TAB_PAGE);
    }

    @Test
    public void jankScenarioTracking_startAndStopTracking() {
        JankReportingScheduler jankReportingScheduler = createJankReportingScheduler();

        jankReportingScheduler.startTrackingScenario(JankScenario.NEW_TAB_PAGE);
        jankReportingScheduler.finishTrackingScenario(JankScenario.NEW_TAB_PAGE);

        // When first getting the handler we need to run the initialize on the handler.
        mShadowLooper.runOneTask();
        // Starting tracking posts a task to begin recording metrics in FrameMetricsStore.
        mShadowLooper.runOneTask();
        // Stopping tracking posts a task to finish tracking and upload the calculated metrics.
        mShadowLooper.runOneTask();

        InOrder orderVerifier = Mockito.inOrder(mFrameMetricsStore);

        // After both tasks we should have started and stopped tracking the periodic reporting
        // scenario.
        orderVerifier.verify(mFrameMetricsStore).initialize();
        orderVerifier.verify(mFrameMetricsStore).startTrackingScenario(JankScenario.NEW_TAB_PAGE);
        orderVerifier.verify(mFrameMetricsStore).stopTrackingScenario(JankScenario.NEW_TAB_PAGE);

        Assert.assertFalse(mShadowLooper.getScheduler().areAnyRunnable());
    }

    @Test
    public void jankReportingSchedulerTest_StartPeriodicReporting() {
        JankReportingScheduler jankReportingScheduler = createJankReportingScheduler();

        jankReportingScheduler.startReportingPeriodicMetrics();

        // When first getting the handler we need to run the initialize on the handler.
        mShadowLooper.runOneTask();
        // When periodic reporting is enabled a task is immediately posted to begin tracking.
        mShadowLooper.runOneTask();
        // Then a delayed task is posted for the reporting loop.
        mShadowLooper.runOneTask();
        // The reporting loop task posts an immediate task to stop tracking and record the data.
        mShadowLooper.runOneTask();

        InOrder orderVerifier = Mockito.inOrder(mFrameMetricsStore);

        // After both tasks we should have started and stopped tracking the periodic reporting
        // scenario.
        orderVerifier.verify(mFrameMetricsStore).initialize();
        orderVerifier
                .verify(mFrameMetricsStore)
                .startTrackingScenario(JankScenario.PERIODIC_REPORTING);
        orderVerifier
                .verify(mFrameMetricsStore)
                .stopTrackingScenario(JankScenario.PERIODIC_REPORTING);

        // There should be another task posted to continue the loop.
        Assert.assertTrue(mShadowLooper.getScheduler().areAnyRunnable());
    }

    @Test
    public void jankReportingSchedulerTest_StopPeriodicReporting() {
        JankReportingScheduler jankReportingScheduler = createJankReportingScheduler();

        jankReportingScheduler.startReportingPeriodicMetrics();

        // When first getting the handler we need to run the initialize on the handler.
        mShadowLooper.runOneTask();
        // Run tracking initialization task.
        mShadowLooper.runOneTask();
        // Run the first reporting loop (delayed 30s).
        mShadowLooper.runOneTask();
        // Run task to stop tracking 1st loop and record data.
        mShadowLooper.runOneTask();
        // Run task to start tracking the 2nd reporting loop.
        mShadowLooper.runOneTask();

        jankReportingScheduler.stopReportingPeriodicMetrics();

        // Stopping periodic metric recording posts a reporting loop task immediately to stop
        // tracking and record results.
        mShadowLooper.runOneTask();
        // The reporting loop task posts another immediate task to stop tracking and report data.
        mShadowLooper.runOneTask();

        InOrder orderVerifier = Mockito.inOrder(mFrameMetricsStore);

        // This start/stop pair corresponds to the first reporting period.
        orderVerifier.verify(mFrameMetricsStore).initialize();
        orderVerifier
                .verify(mFrameMetricsStore)
                .startTrackingScenario(JankScenario.PERIODIC_REPORTING);
        orderVerifier
                .verify(mFrameMetricsStore)
                .stopTrackingScenario(JankScenario.PERIODIC_REPORTING);

        // Stopping reporting forces an immediate report of recorded frames, if any.
        orderVerifier
                .verify(mFrameMetricsStore)
                .startTrackingScenario(JankScenario.PERIODIC_REPORTING);
        orderVerifier
                .verify(mFrameMetricsStore)
                .stopTrackingScenario(JankScenario.PERIODIC_REPORTING);

        // There should not be another task posted to continue the loop.
        Assert.assertFalse(mShadowLooper.getScheduler().areAnyRunnable());
    }
}

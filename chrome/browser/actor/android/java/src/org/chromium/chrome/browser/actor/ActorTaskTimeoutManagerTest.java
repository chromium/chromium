// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link ActorTaskTimeoutManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ANDROID_ACTOR_TASK_TIMEOUT)
public class ActorTaskTimeoutManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    // TODO(crbug.com/528360965): Add integration tests for changing states and seeing warning
    // notifications based on time elapsed.
    private static final int TASK_ID = 123;
    private static final int TASK_ID_2 = 456;

    @Mock private ActorKeyedService mKeyedService;
    @Mock private ActorForegroundServiceManager mForegroundManager;
    @Mock private ActorTask mTask;
    @Mock private ActorTask mTask2;

    private ActorTaskTimeoutManager mTimeoutManager;

    @Before
    public void setUp() {
        when(mKeyedService.getTask(TASK_ID)).thenReturn(mTask);
        when(mTask.getId()).thenReturn(TASK_ID);
        when(mKeyedService.getTask(TASK_ID_2)).thenReturn(mTask2);
        when(mTask2.getId()).thenReturn(TASK_ID_2);
        when(mKeyedService.getCurrentActiveTask()).thenReturn(mTask);
        mTimeoutManager = new ActorTaskTimeoutManager(mKeyedService, mForegroundManager);
    }

    @Test
    public void testRunningTimeoutTriggersWarningAndPause() {
        mTimeoutManager.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        // Advance time by the running timeout.
        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getRunningTimeoutMs(),
                java.util.concurrent.TimeUnit.MILLISECONDS);

        assertTrue(mTimeoutManager.isWarningMode(TASK_ID));
        verify(mTask).pause();

        // Simulate the pause state change from native.
        mTimeoutManager.onTaskStateChanged(TASK_ID, ActorTaskState.PAUSED_BY_ACTOR);

        // Advance terminal timeout.
        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getWarningTimeoutMs(),
                java.util.concurrent.TimeUnit.MILLISECONDS);
        verify(mKeyedService).stopTask(TASK_ID, StoppedReason.TIMEOUT);
    }

    @Test
    public void testPausedTimeoutTriggersWarningAndRefresh() {
        mTimeoutManager.onTaskStateChanged(TASK_ID, ActorTaskState.PAUSED_BY_USER);

        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getPausedTimeoutMs(),
                java.util.concurrent.TimeUnit.MILLISECONDS);

        assertTrue(mTimeoutManager.isWarningMode(TASK_ID));
        verify(mForegroundManager).refreshTaskUI(eq(TASK_ID), eq(ActorTaskState.PAUSED_BY_USER));
    }

    @Test
    public void testTerminalTimeoutStopsTask() {
        // Trigger a warning first (simulate already paused task)
        mTimeoutManager.onTaskStateChanged(TASK_ID, ActorTaskState.PAUSED_BY_USER);
        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getPausedTimeoutMs(),
                java.util.concurrent.TimeUnit.MILLISECONDS);
        assertTrue(mTimeoutManager.isWarningMode(TASK_ID));

        // Now in warning mode, wait for the terminal timeout.
        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getWarningTimeoutMs(),
                java.util.concurrent.TimeUnit.MILLISECONDS);

        verify(mKeyedService).stopTask(TASK_ID, StoppedReason.TIMEOUT);
    }

    @Test
    public void testTimerResetOnLogicalStateChange() {
        mTimeoutManager.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        // Advance half time.
        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getRunningTimeoutMs() / 2,
                java.util.concurrent.TimeUnit.MILLISECONDS);

        // Change to PAUSED. Timer should reset.
        mTimeoutManager.onTaskStateChanged(TASK_ID, ActorTaskState.PAUSED_BY_USER);

        // Advance the rest of the running timeout. Nothing should happen for RUNNING.
        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getRunningTimeoutMs() / 2,
                java.util.concurrent.TimeUnit.MILLISECONDS);
        verify(mTask, never()).pause();

        // Advance the rest of the PAUSED timeout.
        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getPausedTimeoutMs() / 2,
                java.util.concurrent.TimeUnit.MILLISECONDS);
        assertTrue(mTimeoutManager.isWarningMode(TASK_ID));
    }

    @Test
    public void testTimerDoesNotResetOnRunningStateChange() {
        mTimeoutManager.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getRunningTimeoutMs() / 2,
                java.util.concurrent.TimeUnit.MILLISECONDS);

        // Change to REFLECTING (still RUNNING group). Timer should not reset.
        mTimeoutManager.onTaskStateChanged(TASK_ID, ActorTaskState.REFLECTING);

        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getRunningTimeoutMs() / 2,
                java.util.concurrent.TimeUnit.MILLISECONDS);
        assertTrue(mTimeoutManager.isWarningMode(TASK_ID));
    }

    @Test
    public void testResumeFromWarningResetsTimer() {
        // Trigger a warning first
        mTimeoutManager.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getRunningTimeoutMs(),
                java.util.concurrent.TimeUnit.MILLISECONDS);
        assertTrue(mTimeoutManager.isWarningMode(TASK_ID));
        verify(mTask).pause();

        // User resumes task.
        mTimeoutManager.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        assertFalse(mTimeoutManager.isWarningMode(TASK_ID));

        // Should NOT trigger termination after terminal timeout if resumed.
        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getWarningTimeoutMs(),
                java.util.concurrent.TimeUnit.MILLISECONDS);
        verify(mKeyedService, never()).stopTask(eq(TASK_ID), anyInt());
    }

    @Test
    public void testIgnoreUpdatesFromOtherTasks() {
        mTimeoutManager.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        // This should be ignored as we are locked onto TASK_ID.
        mTimeoutManager.onTaskStateChanged(TASK_ID_2, ActorTaskState.ACTING);

        // Advance time for TASK_ID.
        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getRunningTimeoutMs(),
                java.util.concurrent.TimeUnit.MILLISECONDS);

        assertTrue(mTimeoutManager.isWarningMode(TASK_ID));
        assertFalse(mTimeoutManager.isWarningMode(TASK_ID_2));
    }

    @Test
    public void testTransitionToNewTaskAfterCompletion() {
        mTimeoutManager.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);
        mTimeoutManager.onTaskStateChanged(TASK_ID, ActorTaskState.FINISHED);

        // Now the manager should be able to lock onto TASK_ID_2.
        mTimeoutManager.onTaskStateChanged(TASK_ID_2, ActorTaskState.ACTING);

        // Advance time for TASK_ID_2.
        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getRunningTimeoutMs(),
                java.util.concurrent.TimeUnit.MILLISECONDS);

        assertTrue(mTimeoutManager.isWarningMode(TASK_ID_2));
        verify(mTask2).pause();
    }
}

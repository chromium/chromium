// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.os.PowerManager;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowPowerManager;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;

/** Tests for the {@link OmahaServiceStartDelayer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
public class OmahaServiceStartDelayerTest {
    @Mock private Activity mActivity;

    @Spy private Runnable mRunnable;
    private OmahaServiceStartDelayer mOmahaServiceStartDelayer;
    private ShadowPowerManager mShadowPowerManager;

    @Before
    public void setUp() throws Exception {
        Context appContext = ApplicationProvider.getApplicationContext();
        MockitoAnnotations.initMocks(this);
        mShadowPowerManager =
                Shadows.shadowOf((PowerManager) appContext.getSystemService(Context.POWER_SERVICE));
        mOmahaServiceStartDelayer = new OmahaServiceStartDelayer();
        mOmahaServiceStartDelayer.setOmahaRunnableForTesting(mRunnable);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
    }

    @After
    public void tearDown() {
        verifyNoTaskScheduled();

        // In case it has not happened yet forcefully clean up automatically for the next test.
        mOmahaServiceStartDelayer.cancelAndCleanup();
    }

    private void startSession() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        mOmahaServiceStartDelayer.onForegroundSessionStart();
    }

    private void stopSession() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        mOmahaServiceStartDelayer.onForegroundSessionEnd();
    }

    private void setInteractive() {
        mShadowPowerManager.setIsInteractive(true);
    }

    private void setNonInteractive() {
        mShadowPowerManager.setIsInteractive(false);
    }

    private void verifyTaskScheduled() {
        Assert.assertTrue(mOmahaServiceStartDelayer.hasRunnableController());
    }

    private void verifyNoTaskScheduled() {
        Assert.assertFalse(mOmahaServiceStartDelayer.hasRunnableController());
    }

    /** Check if the runnable is posted and run while the screen is on. */
    @Test
    @MediumTest
    @Feature({"Omaha"})
    public void testRunnableRunsWithScreenOn() {
        startSession();
        verifyTaskScheduled();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mRunnable, times(1)).run();
        verifyNoTaskScheduled();
    }

    /** Check that the runnable gets posted and canceled when the app is sent to the background. */
    @Test
    @Feature({"Omaha"})
    public void testRunnableGetsCanceledWhenAppIsBackgrounded() {
        // Starting a session should schedule a delayed task.
        startSession();
        verifyTaskScheduled();

        // Stop happened before the runnable has a chance to run.
        stopSession();
        verifyNoTaskScheduled();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Since the session was stopped before executing the task, the runnable should not have
        // been executed.
        verify(mRunnable, times(0)).run();
    }

    /**
     * Check that the runnable gets posted and canceled when the app is sent to the background, but
     * then restarted when the app goes to the foreground.
     */
    @Test
    @Feature({"Omaha"})
    public void testRunnableExecutesOnlyOnceIfStartedAndStoppedInQuickSuccession() {
        // Starting a session should schedule a delayed task.
        startSession();
        verifyTaskScheduled();
        stopSession();
        verifyNoTaskScheduled();
        startSession();
        verifyTaskScheduled();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // There should in total only be a single execution.
        verify(mRunnable, times(1)).run();
    }

    /** Check that the runnable gets run only while the screen is on. */
    @Test
    @Feature({"Omaha"})
    public void testRunnableGetsRunWhenScreenIsTurnedOn() {
        // Claim the screen is off.
        setNonInteractive();

        // Because the screen is off, nothing should happen.
        startSession();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mRunnable, times(0)).run();
        verifyNoTaskScheduled();

        // Pretend to turn the screen on and bring the app to the foreground, which should schedule
        // the task.
        setInteractive();
        startSession();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mRunnable, times(1)).run();
        verifyNoTaskScheduled();
    }

    /** Check that the runnable is not executed while the screen is off, but app is in foreground. */
    @Test
    @Feature({"Omaha"})
    public void testRunnableIsNotRunWhileScreenIsOff() {
        startSession();
        verifyTaskScheduled();

        // Turn screen off without stopping before task is executed.
        setNonInteractive();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Since the screen was off when trying to execute the task, the runnable should not have
        // been executed.
        verify(mRunnable, times(0)).run();
    }

    /** Verify that the runnable is only executed once even if the public API contract is not upheld. */
    @Test
    @MediumTest
    @Feature({"Omaha"})
    public void testRunnableIsOnlyExecutedOnce() {
        startSession();
        verifyTaskScheduled();

        // Start the session again, in case we at some point get two start calls without any
        // stop in between.
        startSession();

        // Now execute the tasks. The runnable should still only be invoked once.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mRunnable, times(1)).run();
        verifyNoTaskScheduled();
    }
}

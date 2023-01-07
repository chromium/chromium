// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.PowerManager;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

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
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.PowerBroadcastReceiver.ServiceRunnable.State;

/**
 * Tests for the PowerBroadcastReceiver.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PowerBroadcastReceiverTest {
    @Mock
    private Activity mActivity;

    @Spy
    private PowerBroadcastReceiver.ServiceRunnable mRunnable;
    private PowerBroadcastReceiver mReceiver;
    private ShadowPowerManager mShadowPowerManager;

    @Before
    public void setUp() throws Exception {
        Context appContext = ApplicationProvider.getApplicationContext();
        MockitoAnnotations.initMocks(this);
        mShadowPowerManager =
                Shadows.shadowOf((PowerManager) appContext.getSystemService(Context.POWER_SERVICE));
        mReceiver = new PowerBroadcastReceiver();
        mReceiver.setServiceRunnableForTests(mRunnable);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);

        // Initially claim that the screen is on.
        mShadowPowerManager.setIsInteractive(true);
    }

    private void startSession() {
        mReceiver.onForegroundSessionStart();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
    }

    private void pauseSession() {
        mReceiver.onForegroundSessionEnd();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
    }

    /**
     * Check if the runnable is posted and run while the screen is on.
     */
    @Test
    @MediumTest
    @Feature({"Omaha", "Sync"})
    public void testRunnableRunsWithScreenOn() throws Exception {
        startSession();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        pauseSession();

        verify(mRunnable, times(1)).setState(State.POSTED);
        verify(mRunnable, times(1)).setState(State.COMPLETED);
        verify(mRunnable, times(0)).setState(State.CANCELED);
        verify(mRunnable, times(1)).runActions();
        Assert.assertFalse("Still listening for power broadcasts.", mReceiver.isRegistered());
    }

    /**
     * Check that the runnable gets posted and canceled when Main is sent to the background.
     */
    @Test
    @Feature({"Omaha", "Sync"})
    public void testRunnableGetsCanceled() throws Exception {
        startSession();
        pauseSession();
        // Pause happened before the runnable has a chance to run.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mRunnable, times(1)).setState(State.POSTED);
        verify(mRunnable, times(0)).setState(State.COMPLETED);
        verify(mRunnable, times(1)).setState(State.CANCELED);
        verify(mRunnable, times(0)).runActions();
        Assert.assertFalse("Still listening for power broadcasts.", mReceiver.isRegistered());
    }

    /**
     * Check that the runnable gets run only while the screen is on.
     */
    @Test
    @Feature({"Omaha", "Sync"})
    public void testRunnableGetsRunWhenScreenIsOn() throws Exception {
        // Claim the screen is off.
        mShadowPowerManager.setIsInteractive(false);

        // Because the screen is off, nothing should happen.
        startSession();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        Assert.assertTrue("Isn't waiting for power broadcasts.", mReceiver.isRegistered());
        verify(mRunnable, times(0)).setState(State.POSTED);
        verify(mRunnable, times(0)).setState(State.COMPLETED);
        verify(mRunnable, times(0)).setState(State.CANCELED);
        verify(mRunnable, times(0)).runActions();

        // Pretend to turn the screen on.
        mShadowPowerManager.setIsInteractive(true);
        Intent intent = new Intent(Intent.ACTION_SCREEN_ON);
        mReceiver.onReceive(ApplicationProvider.getApplicationContext(), intent);

        // Now the event should be processed.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mRunnable, times(1)).setState(State.POSTED);
        verify(mRunnable, times(1)).setState(State.COMPLETED);
        verify(mRunnable, times(0)).setState(State.CANCELED);
        verify(mRunnable, times(1)).runActions();

        Assert.assertFalse("Still listening for power broadcasts.", mReceiver.isRegistered());
    }
}

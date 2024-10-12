// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.os.Handler;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Robolectric unit tests for TrampolineActivityTracker. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrampolineActivityTrackerTest {
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @After
    public void tearDown() {
        TrampolineActivityTracker.destroy();
    }

    @Test
    public void testTrampolineActivityFinishedAfterDelay() {
        NotificationIntentInterceptor.TrampolineActivity activity =
                new NotificationIntentInterceptor.TrampolineActivity();
        assertTrue(TrampolineActivityTracker.getInstance().tryTrackActivity(activity));
        assertFalse(activity.isFinishing());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testWillNotTrackSecondTrampolineActivity() {
        assertTrue(
                TrampolineActivityTracker.getInstance()
                        .tryTrackActivity(new NotificationIntentInterceptor.TrampolineActivity()));
        assertFalse(
                TrampolineActivityTracker.getInstance()
                        .tryTrackActivity(new NotificationIntentInterceptor.TrampolineActivity()));
    }

    @Test
    public void testFinishTrackedActivity() {
        NotificationIntentInterceptor.TrampolineActivity activity =
                new NotificationIntentInterceptor.TrampolineActivity();
        assertTrue(TrampolineActivityTracker.getInstance().tryTrackActivity(activity));
        TrampolineActivityTracker.getInstance().finishTrackedActivity();
        assertTrue(activity.isFinishing());

        // Now new activity can be tracked.
        assertTrue(
                TrampolineActivityTracker.getInstance()
                        .tryTrackActivity(new NotificationIntentInterceptor.TrampolineActivity()));
    }

    @Test
    public void testTrampolineActivityTimeoutDelay() {
        Handler mockHandler = mock(Handler.class);
        TrampolineActivityTracker.getInstance().setHandlerForTesting(mockHandler);
        NotificationIntentInterceptor.TrampolineActivity activity =
                new NotificationIntentInterceptor.TrampolineActivity();
        assertTrue(TrampolineActivityTracker.getInstance().tryTrackActivity(activity));
        verify(mockHandler, times(1)).postDelayed(any(Runnable.class), eq(5000L));

        // Timeout delay will be changed to 1 seconds.
        TrampolineActivityTracker.getInstance().onNativeInitialized();
        verify(mockHandler, times(1)).postDelayed(any(Runnable.class), eq(1000L));

        // Since native is initialized, new activity will get 1 seconds to timeout.
        TrampolineActivityTracker.getInstance().finishTrackedActivity();
        assertTrue(activity.isFinishing());
        assertTrue(
                TrampolineActivityTracker.getInstance()
                        .tryTrackActivity(new NotificationIntentInterceptor.TrampolineActivity()));
        verify(mockHandler, times(2)).postDelayed(any(Runnable.class), eq(1000L));
    }

    @Test
    public void testIntentProcessingWithoutTrackedActivity() {
        Handler mockHandler = mock(Handler.class);
        TrampolineActivityTracker.getInstance().setHandlerForTesting(mockHandler);
        assertEquals(TrampolineActivityTracker.getInstance().startProcessingNewIntent(1000L), -1);
        TrampolineActivityTracker.getInstance().onIntentCompleted(-1);
        TrampolineActivityTracker.getInstance().onIntentCompleted(0);
        verify(mockHandler, times(0)).postDelayed(any(Runnable.class), anyLong());
    }

    @Test
    public void testExtendTimeoutWithNewIntent() {
        Handler mockHandler = mock(Handler.class);
        TrampolineActivityTracker.getInstance().setHandlerForTesting(mockHandler);
        NotificationIntentInterceptor.TrampolineActivity activity =
                new NotificationIntentInterceptor.TrampolineActivity();
        assertTrue(TrampolineActivityTracker.getInstance().tryTrackActivity(activity));
        verify(mockHandler, times(1)).removeCallbacks(any(Runnable.class));
        verify(mockHandler, times(1)).postDelayed(any(Runnable.class), eq(5000L));

        // Adding a new job with 8 seconds of estimated process time.
        assertEquals(TrampolineActivityTracker.getInstance().startProcessingNewIntent(8000L), 0);
        verify(mockHandler, times(2)).removeCallbacks(any(Runnable.class));
        verify(mockHandler, times(1)).postDelayed(any(Runnable.class), eq(8000L));

        // Advance the clock by 2 seconds, and add another job of 7 seconds.
        mFakeTimeTestRule.advanceMillis(2000L);
        assertEquals(TrampolineActivityTracker.getInstance().startProcessingNewIntent(7000L), 1);
        verify(mockHandler, times(3)).removeCallbacks(any(Runnable.class));
        verify(mockHandler, times(1)).postDelayed(any(Runnable.class), eq(7000L));

        // Advance the clock by 2 second, and finish the second job
        mFakeTimeTestRule.advanceMillis(2000L);
        TrampolineActivityTracker.getInstance().onIntentCompleted(1);
        // Only 4 seconds remaining for the first job to finish.
        verify(mockHandler, times(4)).removeCallbacks(any(Runnable.class));
        verify(mockHandler, times(1)).postDelayed(any(Runnable.class), eq(4000L));

        assertFalse(activity.isFinishing());
        TrampolineActivityTracker.getInstance().onIntentCompleted(0);
        verify(mockHandler, times(5)).removeCallbacks(any(Runnable.class));
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testNativeInitializationAfterIntentProcessing() {
        Handler mockHandler = mock(Handler.class);
        TrampolineActivityTracker.getInstance().setHandlerForTesting(mockHandler);
        NotificationIntentInterceptor.TrampolineActivity activity =
                new NotificationIntentInterceptor.TrampolineActivity();
        assertTrue(TrampolineActivityTracker.getInstance().tryTrackActivity(activity));
        verify(mockHandler, times(1)).removeCallbacks(any(Runnable.class));
        verify(mockHandler, times(1)).postDelayed(any(Runnable.class), eq(5000L));

        // Adding a new job with 4 seconds of estimated process time before native initialization.
        // Since this is less than the default 5 seconds timeout, nothing will change.
        assertEquals(TrampolineActivityTracker.getInstance().startProcessingNewIntent(4000L), 0);
        verify(mockHandler, times(1)).removeCallbacks(any(Runnable.class));

        // Initialize native, it shouldn't impact the existing job's remaining time.
        TrampolineActivityTracker.getInstance().onNativeInitialized();
        verify(mockHandler, times(1)).removeCallbacks(any(Runnable.class));

        assertFalse(activity.isFinishing());
        TrampolineActivityTracker.getInstance().onIntentCompleted(0);
        verify(mockHandler, times(2)).removeCallbacks(any(Runnable.class));
        assertTrue(activity.isFinishing());
    }
}

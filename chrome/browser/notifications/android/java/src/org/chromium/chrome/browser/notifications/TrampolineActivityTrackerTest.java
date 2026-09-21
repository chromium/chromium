// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.os.Handler;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;

/** Robolectric unit tests for TrampolineActivityTracker. */
@RunWith(BaseRobolectricTestRunner.class)
public class TrampolineActivityTrackerTest {
    private static final String TEST_JOB_ID = "foo";
    private static final String TEST_JOB_ID_2 = "foo2";

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Handler mHandler;

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

        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
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
        TrampolineActivityTracker.getInstance().setHandlerForTesting(mHandler);
        NotificationIntentInterceptor.TrampolineActivity activity =
                new NotificationIntentInterceptor.TrampolineActivity();
        assertTrue(TrampolineActivityTracker.getInstance().tryTrackActivity(activity));
        verify(mHandler, times(1)).postDelayed(any(Runnable.class), eq(5000L));

        // Timeout delay will be changed to 1 seconds.
        TrampolineActivityTracker.getInstance().onNativeInitialized();
        verify(mHandler, times(1)).postDelayed(any(Runnable.class), eq(1000L));

        // Since native is initialized, new activity will get 1 seconds to timeout.
        TrampolineActivityTracker.getInstance().finishTrackedActivity();
        assertTrue(activity.isFinishing());
        assertTrue(
                TrampolineActivityTracker.getInstance()
                        .tryTrackActivity(new NotificationIntentInterceptor.TrampolineActivity()));
        verify(mHandler, times(2)).postDelayed(any(Runnable.class), eq(1000L));
    }

    @Test
    public void testIntentProcessingWithoutTrackedActivity() {
        TrampolineActivityTracker.getInstance().setHandlerForTesting(mHandler);
        TrampolineActivityTracker.getInstance()
                .startProcessingNewIntent(
                        TEST_JOB_ID, TrampolineActivityTracker.JobDuration.NORMAL);

        TrampolineActivityTracker.getInstance().onIntentCompleted(TEST_JOB_ID);
        verify(mHandler, times(0)).postDelayed(any(Runnable.class), anyLong());
    }

    @Test
    public void testExtendTimeoutWithNewIntent() {
        TrampolineActivityTracker.getInstance().setHandlerForTesting(mHandler);
        NotificationIntentInterceptor.TrampolineActivity activity =
                new NotificationIntentInterceptor.TrampolineActivity();
        assertTrue(TrampolineActivityTracker.getInstance().tryTrackActivity(activity));
        verify(mHandler, times(1)).removeCallbacks(any(Runnable.class));
        verify(mHandler, times(1)).postDelayed(any(Runnable.class), eq(5000L));

        // Adding a new long running job.
        TrampolineActivityTracker.getInstance()
                .startProcessingNewIntent(TEST_JOB_ID, TrampolineActivityTracker.JobDuration.LONG);
        verify(mHandler, times(2)).removeCallbacks(any(Runnable.class));
        verify(mHandler, times(1)).postDelayed(any(Runnable.class), eq(8000L));

        // Advance the clock by 2 seconds, and add another long running job.
        mFakeTimeTestRule.advanceMillis(2000L);
        TrampolineActivityTracker.getInstance()
                .startProcessingNewIntent(
                        TEST_JOB_ID_2, TrampolineActivityTracker.JobDuration.LONG);
        verify(mHandler, times(3)).removeCallbacks(any(Runnable.class));
        verify(mHandler, times(2)).postDelayed(any(Runnable.class), eq(8000L));

        // Advance the clock by 2 second, and finish the second job
        mFakeTimeTestRule.advanceMillis(2000L);
        TrampolineActivityTracker.getInstance().onIntentCompleted(TEST_JOB_ID_2);
        // Only 4 seconds remaining for the first job to finish.
        verify(mHandler, times(4)).removeCallbacks(any(Runnable.class));
        verify(mHandler, times(1)).postDelayed(any(Runnable.class), eq(4000L));

        assertFalse(activity.isFinishing());
        TrampolineActivityTracker.getInstance().onIntentCompleted(TEST_JOB_ID);
        verify(mHandler, times(5)).removeCallbacks(any(Runnable.class));
        assertTrue(activity.isFinishing());
    }

    @Test
    public void testNativeInitializationAfterIntentProcessing() {
        TrampolineActivityTracker.getInstance().setHandlerForTesting(mHandler);
        NotificationIntentInterceptor.TrampolineActivity activity =
                new NotificationIntentInterceptor.TrampolineActivity();
        assertTrue(TrampolineActivityTracker.getInstance().tryTrackActivity(activity));
        verify(mHandler, times(1)).removeCallbacks(any(Runnable.class));
        verify(mHandler, times(1)).postDelayed(any(Runnable.class), eq(5000L));

        // Adding a new immediate job before native initialization.
        // Since this is less than the default 5 seconds timeout, nothing will change.
        TrampolineActivityTracker.getInstance()
                .startProcessingNewIntent(
                        TEST_JOB_ID, TrampolineActivityTracker.JobDuration.IMMEDIATE);
        verify(mHandler, times(1)).removeCallbacks(any(Runnable.class));

        // Initialize native, it shouldn't impact the existing job's remaining time.
        TrampolineActivityTracker.getInstance().onNativeInitialized();
        verify(mHandler, times(1)).removeCallbacks(any(Runnable.class));

        assertFalse(activity.isFinishing());
        TrampolineActivityTracker.getInstance().onIntentCompleted(TEST_JOB_ID);
        verify(mHandler, times(2)).removeCallbacks(any(Runnable.class));
        assertTrue(activity.isFinishing());
    }
}

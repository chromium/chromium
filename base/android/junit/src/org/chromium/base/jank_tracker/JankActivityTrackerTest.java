// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.Window;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 *  Tests for JankActivityTracker.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class JankActivityTrackerTest {
    ShadowLooper mShadowLooper;

    @Mock
    private Activity mActivity;

    @Mock
    private Window mWindow;

    @Mock
    private JankFrameMetricsListener mJankFrameMetricsListener;

    @Mock
    private JankMetricMeasurement mJankMetricMeasurement;

    JankActivityTracker createJankActivityTracker(Activity activity) {
        JankActivityTracker tracker = new JankActivityTracker(
                activity, mJankFrameMetricsListener, mJankMetricMeasurement);
        mShadowLooper = Shadow.extract(tracker.getOrCreateHandler().getLooper());

        return tracker;
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mActivity.getWindow()).thenReturn(mWindow);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
    }

    @Test
    public void jankTrackerTest_TestInitialize() {
        JankActivityTracker jankActivityTracker = createJankActivityTracker(mActivity);
        jankActivityTracker.initialize();

        // Verify that we are listening to frame metrics.
        // Initialize also starts listening to activity lifecycle events, but that's harder to
        // verify.
        verify(mWindow).addOnFrameMetricsAvailableListener(any(), any());
    }

    @Test
    public void jankTrackerTest_TestActivityResume() {
        JankActivityTracker jankActivityTracker = createJankActivityTracker(mActivity);
        jankActivityTracker.initialize();

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);

        // When an activity resumes we schedule a repeating task to report metrics on the tracker's
        // handler thread.
        mShadowLooper.runOneTask();

        // The reporting task should clear the jank measurement.
        verify(mJankMetricMeasurement).clear();

        // The reporting task should be looping, so another task should be posted.
        Assert.assertTrue(mShadowLooper.getScheduler().areAnyRunnable());

        // When an activity resumes we start recording metrics.
        verify(mJankFrameMetricsListener, atLeastOnce()).setIsListenerRecording(true);
    }

    @Test
    public void jankTrackerTest_TestActivityPause() {
        JankActivityTracker jankActivityTracker = createJankActivityTracker(mActivity);
        jankActivityTracker.initialize();

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);

        mShadowLooper.runOneTask();

        // When an activity pauses the reporting task should still be looping.
        Assert.assertTrue(mShadowLooper.getScheduler().areAnyRunnable());

        InOrder orderVerifier = Mockito.inOrder(mJankFrameMetricsListener);

        orderVerifier.verify(mJankFrameMetricsListener, atLeastOnce()).setIsListenerRecording(true);
        // When an activity pauses we stop recording metrics.
        orderVerifier.verify(mJankFrameMetricsListener).setIsListenerRecording(false);
    }

    @Test
    public void jankTrackerTest_TestActivityStop() {
        JankActivityTracker jankActivityTracker = createJankActivityTracker(mActivity);
        jankActivityTracker.initialize();

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        // When an activity stops we run the reporting task one last time and stop it.
        mShadowLooper.runOneTask();

        Assert.assertFalse(mShadowLooper.getScheduler().areAnyRunnable());
    }

    @Test
    public void jankTrackerTest_TestAttachTrackerOnResumedActivity() {
        // Modify the activity's state before attaching JankActivityTracker.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STARTED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);

        JankActivityTracker jankActivityTracker = createJankActivityTracker(mActivity);
        jankActivityTracker.initialize();

        // Verify that JankActivityTracker is running as expected for the Resumed state.
        // Reporting task should be running and looping.
        mShadowLooper.runOneTask();
        verify(mJankMetricMeasurement).clear();
        Assert.assertTrue(mShadowLooper.getScheduler().areAnyRunnable());
        // Metric recording should be enabled.
        verify(mJankFrameMetricsListener).setIsListenerRecording(true);
    }

    @Test
    public void jankTrackerTest_TestOutOfOrderStateChange() {
        JankActivityTracker jankActivityTracker = createJankActivityTracker(mActivity);
        jankActivityTracker.initialize();

        // Move the activity from STOPPED to RESUMED without calling STARTED.
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);

        // Verify that JankActivityTracker is running as expected for the Resumed state.
        // Reporting task should be running and looping.
        mShadowLooper.runOneTask();
        verify(mJankMetricMeasurement).clear();
        Assert.assertTrue(mShadowLooper.getScheduler().areAnyRunnable());
        // Metric recording should be enabled.
        verify(mJankFrameMetricsListener).setIsListenerRecording(true);
    }
}

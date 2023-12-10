// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import android.app.Activity;
import android.os.Build.VERSION_CODES;

import androidx.annotation.RequiresApi;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ThreadUtils.ThreadChecker;
import org.chromium.base.lifetime.DestroyChecker;

import java.lang.ref.WeakReference;

/**
 * This class takes an Activity and attaches a FrameMetricsListener to it, in addition it controls
 * periodic jank metric reporting and frame metric recording based on the Activity's lifecycle
 * events.
 */
@RequiresApi(api = VERSION_CODES.N)
class JankActivityTracker extends JankTrackerStateController implements ActivityStateListener {
    private final ThreadChecker mThreadChecker = new ThreadChecker();
    private final DestroyChecker mDestroyChecker = new DestroyChecker();

    private WeakReference<Activity> mActivityReference;

    JankActivityTracker(
            Activity context,
            FrameMetricsListener listener,
            JankReportingScheduler reportingScheduler) {
        super(listener, reportingScheduler);
        mActivityReference = new WeakReference<>(context);
    }

    @Override
    public void initialize() {
        assertValidState();
        Activity activity = mActivityReference.get();
        if (activity != null) {
            ApplicationStatus.registerStateListenerForActivity(this, activity);
            @ActivityState int activityState = ApplicationStatus.getStateForActivity(activity);
            onActivityStateChange(activity, activityState);
            startMetricCollection(activity.getWindow());
        }
    }

    @Override
    public void destroy() {
        mThreadChecker.assertOnValidThread();
        ApplicationStatus.unregisterActivityStateListener(this);
        stopPeriodicReporting();
        Activity activity = mActivityReference.get();
        if (activity != null) {
            stopMetricCollection(activity.getWindow());
        }
        mDestroyChecker.destroy();
    }

    private void assertValidState() {
        mThreadChecker.assertOnValidThread();
        mDestroyChecker.checkNotDestroyed();
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        assertValidState();
        switch (newState) {
            case ActivityState.STARTED: // Intentional fallthrough.
            case ActivityState.RESUMED:
                startPeriodicReporting();
                startMetricCollection(null);
                break;
            case ActivityState.PAUSED:
                // This method can be called at any moment safely, we want to report metrics even
                // when the activity is paused.
                startPeriodicReporting();
                stopMetricCollection(null);
                break;
            case ActivityState.STOPPED:
                stopPeriodicReporting();
                stopMetricCollection(null);
                break;
        }
    }
}

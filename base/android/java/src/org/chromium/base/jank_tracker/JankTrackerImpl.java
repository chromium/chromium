// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import android.app.Activity;
import android.os.Build;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.lang.ref.WeakReference;

/**
 * Class for recording janky frame metrics for a specific Activity.
 *
 * It should be constructed when the activity is created, recording starts and stops automatically
 * based on activity state. When the activity is being destroyed {@link #destroy()} should be called
 * to clear the activity state observer. All methods should be called from the UI thread.
 */
public class JankTrackerImpl implements JankTracker {
    // We use the DEADLINE field in the Android FrameMetrics which was added in S.
    private static final boolean IS_TRACKING_ENABLED =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.S;

    private boolean mIsInitialized;
    private JankTrackerStateController mController;
    private JankReportingScheduler mReportingScheduler;
    private boolean mDestroyed;

    /**
     * Creates a new JankTracker instance tracking UI rendering of an activity. Metric recording
     * starts when the activity starts, and it's paused when the activity stops. Optionally the
     * construction can be delayed by passing a value for {@param constructionDelayMs}. This allows
     * for the impact on startup to be mitigated.
     */
    public JankTrackerImpl(Activity activity, int constructionDelayMs) {
        // This class shouldn't keep the activity alive.
        WeakReference<Activity> ref = new WeakReference<Activity>(activity);
        Runnable init =
                new Runnable() {
                    @Override
                    public void run() {
                        // If we've been destroyed or the Activity is gone early out.
                        Activity innerActivity = ref.get();
                        if (mDestroyed || innerActivity == null || innerActivity.isDestroyed()) {
                            return;
                        }

                        // Initialize the system.
                        FrameMetricsStore metricsStore = new FrameMetricsStore();
                        if (!constructInternalPreController(
                                new JankReportingScheduler(metricsStore))) {
                            return;
                        }

                        constructInternalFinal(
                                new JankActivityTracker(
                                        innerActivity,
                                        new FrameMetricsListener(metricsStore),
                                        mReportingScheduler));
                    }
                };
        if (constructionDelayMs <= 0) {
            init.run();
        } else {
            PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, init, constructionDelayMs);
        }
    }

    /**
     * Creates a new JankTracker which allows the controller to determine when it should start and
     * stop metric scenarios/collection.
     */
    public JankTrackerImpl(JankTrackerStateController controller) {
        if (!constructInternalPreController(controller.mReportingScheduler)) return;
        constructInternalFinal(controller);
    }

    private boolean constructInternalPreController(JankReportingScheduler scheduler) {
        if (!IS_TRACKING_ENABLED) {
            mReportingScheduler = null;
            mController = null;
            return false;
        }
        mReportingScheduler = scheduler;
        return true;
    }

    private void constructInternalFinal(JankTrackerStateController controller) {
        mController = controller;
        mController.initialize();
        mIsInitialized = true;
    }

    @Override
    public void startTrackingScenario(JankScenario scenario) {
        if (!IS_TRACKING_ENABLED || !mIsInitialized) return;

        mReportingScheduler.startTrackingScenario(scenario);
    }

    @Override
    public void finishTrackingScenario(JankScenario scenario) {
        finishTrackingScenario(scenario, -1);
    }

    @Override
    public void finishTrackingScenario(JankScenario scenario, long endScenarioTimeNs) {
        if (!IS_TRACKING_ENABLED || !mIsInitialized) return;

        mReportingScheduler.finishTrackingScenario(scenario, endScenarioTimeNs);
    }

    /** Stops listening for Activity state changes. */
    @Override
    public void destroy() {
        mDestroyed = true;
        if (!IS_TRACKING_ENABLED || !mIsInitialized) return;
        mController.destroy();
    }
}

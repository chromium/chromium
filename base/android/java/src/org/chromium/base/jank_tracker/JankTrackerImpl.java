// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import android.app.Activity;
import android.os.Build;

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

    private JankTrackerStateController mController;
    private JankReportingScheduler mReportingScheduler;

    /**
     * Creates a new JankTracker instance tracking UI rendering of an activity. Metric recording
     * starts when the activity starts, and it's paused when the activity stops.
     */
    public JankTrackerImpl(Activity activity) {
        FrameMetricsStore metricsStore = new FrameMetricsStore();
        if (!constructInternalPreController(new JankReportingScheduler(metricsStore))) return;

        constructInternalFinal(new JankActivityTracker(
                activity, new FrameMetricsListener(metricsStore), mReportingScheduler));
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
    }

    @Override
    public void startTrackingScenario(@JankScenario int scenario) {
        if (!IS_TRACKING_ENABLED) return;

        mReportingScheduler.startTrackingScenario(scenario);
    }

    @Override
    public void finishTrackingScenario(@JankScenario int scenario) {
        finishTrackingScenario(scenario, -1);
    }

    @Override
    public void finishTrackingScenario(@JankScenario int scenario, long endScenarioTimeNs) {
        if (!IS_TRACKING_ENABLED) return;

        mReportingScheduler.finishTrackingScenario(scenario, endScenarioTimeNs);
    }

    /**
     * Stops listening for Activity state changes.
     */
    @Override
    public void destroy() {
        if (!IS_TRACKING_ENABLED) return;

        mController.destroy();
    }
}

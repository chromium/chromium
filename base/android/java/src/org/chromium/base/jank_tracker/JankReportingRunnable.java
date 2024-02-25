// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import android.os.Handler;

import org.chromium.base.TraceEvent;

/**
 * This runnable receives a FrameMetricsStore instance and starts/stops tracking a given scenario.
 * When a scenario stops it takes its metrics and sends them to native to be recorded in UMA.
 * This is executed by JankReportingScheduler on its own thread.
 */
class JankReportingRunnable implements Runnable {
    private final FrameMetricsStore mMetricsStore;
    private final JankScenario mScenario;
    private final boolean mIsStartingTracking;
    // This must be the same handler that this is running on.
    private final Handler mHandler;
    // If metrics should be collected based on the state (scrolling) specify a
    // JankEndScenarioTime.
    private final JankEndScenarioTime mJankEndScenarioTime;

    // When a JankEndScenarioTime is specified we don't immediately collect the metrics but instead
    // post a task (this runnable). However to keep code reuse the same between delay/no-delay we
    // use the same runnable but don't post it when there is no delay.
    private class FinalReportingRunnable implements Runnable {
        @Override
        public void run() {
            try (TraceEvent e = TraceEvent.scoped("ReportingCUJScenarioData", mScenario.type())) {
                JankMetrics metrics;
                if (mJankEndScenarioTime == null) {
                    metrics = mMetricsStore.stopTrackingScenario(mScenario);
                } else {
                    // Since this is after the timeout we just unconditionally get the metrics.
                    metrics =
                            mMetricsStore.stopTrackingScenario(
                                    mScenario, mJankEndScenarioTime.endScenarioTimeNs);
                }

                if (metrics == null || metrics.timestampsNs.length == 0) {
                    TraceEvent.instant("no metrics");
                    return;
                }

                long startTime = metrics.timestampsNs[0] / 1000000;
                long lastTime = metrics.timestampsNs[metrics.timestampsNs.length - 1] / 1000000;
                long lastDuration = metrics.durationsNs[metrics.durationsNs.length - 1] / 1000000;
                // The time that we have metrics covering is from the first VSYNC_TIMESTAMP
                // (startTime) to the last frame has finished (lastTime + lastDuration).
                long endTime = lastTime - startTime + lastDuration;

                // Confirm that the current call context is valid.
                // Debug builds will assert and fail; release builds will optimize this out.
                JankMetricUMARecorderJni.get();
                // TODO(salg@): Cache metrics in case native takes >30s to initialize.
                JankMetricUMARecorder.recordJankMetricsToUMA(
                        metrics, startTime, endTime, mScenario.type());
            }
        }
    }

    JankReportingRunnable(
            FrameMetricsStore metricsStore,
            JankScenario scenario,
            boolean isStartingTracking,
            Handler handler,
            JankEndScenarioTime endScenarioTime) {
        mMetricsStore = metricsStore;
        mScenario = scenario;
        mIsStartingTracking = isStartingTracking;
        mHandler = handler;
        mJankEndScenarioTime = endScenarioTime;
    }

    @Override
    public void run() {
        try (TraceEvent e =
                TraceEvent.scoped(
                        "StartingOrStoppingJankScenario",
                        "StartingScenario:"
                                + Boolean.toString(mIsStartingTracking)
                                + ",Scenario:"
                                + Integer.toString(mScenario.type()))) {
            if (mIsStartingTracking) {
                if (mMetricsStore == null) {
                    TraceEvent.instant("StartTrackingScenario metrics store null");
                    return;
                }
                mMetricsStore.startTrackingScenario(mScenario);
                return;
            }
            boolean dataIsReady =
                    mJankEndScenarioTime == null
                            || (mJankEndScenarioTime != null
                                    && mMetricsStore.hasReceivedMetricsPast(
                                            mJankEndScenarioTime.endScenarioTimeNs));

            if (dataIsReady) {
                new FinalReportingRunnable().run();
            } else {
                mHandler.postDelayed(
                        new FinalReportingRunnable(), mJankEndScenarioTime.timeoutDelayMs);
            }
        }
    }
}

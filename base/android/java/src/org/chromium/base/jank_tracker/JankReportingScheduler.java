// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import android.os.Handler;
import android.os.HandlerThread;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * This class receives requests to start and stop jank scenario tracking and runs them in a
 * HandlerThread it creates. In addition it handles the recording of periodic jank metrics.
 */
public class JankReportingScheduler {
    private static final long PERIODIC_METRIC_DELAY_MS = 5_000;
    private final FrameMetricsStore mFrameMetricsStore;

    public JankReportingScheduler(FrameMetricsStore frameMetricsStore) {
        mFrameMetricsStore = frameMetricsStore;
        LazyHolder.HANDLER.post(
                new Runnable() {
                    @Override
                    public void run() {
                        mFrameMetricsStore.initialize();
                    }
                });
    }

    private final Runnable mPeriodicMetricReporter =
            new Runnable() {
                @Override
                public void run() {
                    // delay should never be null.
                    finishTrackingScenario(JankScenario.PERIODIC_REPORTING);

                    if (mIsPeriodicReporterLooping.get()) {
                        // We delay starting the next periodic reporting until the timeout has
                        // finished by taking the delay and +1 so that it will run in order (it
                        // was posted above).
                        startTrackingScenario(JankScenario.PERIODIC_REPORTING);
                        LazyHolder.HANDLER.postDelayed(
                                mPeriodicMetricReporter, PERIODIC_METRIC_DELAY_MS);
                    }
                }
            };

    private static class LazyHolder {
        private static final HandlerThread HANDLER_THREAD;
        private static final Handler HANDLER;

        static {
            HANDLER_THREAD = new HandlerThread("Jank-Tracker");
            HANDLER_THREAD.start();
            HANDLER = new Handler(HANDLER_THREAD.getLooper());
        }
    }

    private final AtomicBoolean mIsPeriodicReporterLooping = new AtomicBoolean(false);

    public void startTrackingScenario(JankScenario scenario) {
        LazyHolder.HANDLER.post(
                new JankReportingRunnable(
                        mFrameMetricsStore,
                        scenario,
                        /* isStartingTracking= */ true,
                        LazyHolder.HANDLER,
                        null));
    }

    public void finishTrackingScenario(JankScenario scenario) {
        finishTrackingScenario(scenario, -1);
    }

    public void finishTrackingScenario(JankScenario scenario, long endScenarioTimeNs) {
        finishTrackingScenario(scenario, JankEndScenarioTime.endAt(endScenarioTimeNs));
    }

    public void finishTrackingScenario(JankScenario scenario, JankEndScenarioTime endScenarioTime) {
        // We store the stop task in case the delay is greater than zero and we start this scenario
        // again.
        JankReportingRunnable runnable =
                new JankReportingRunnable(
                        mFrameMetricsStore,
                        scenario,
                        /* isStartingTracking= */ false,
                        LazyHolder.HANDLER,
                        endScenarioTime);
        LazyHolder.HANDLER.post(runnable);
    }

    public Handler getOrCreateHandler() {
        return LazyHolder.HANDLER;
    }

    public void startReportingPeriodicMetrics() {
        // If mIsPeriodicReporterLooping was already true then there's no need to post another task.
        if (mIsPeriodicReporterLooping.getAndSet(true)) {
            return;
        }
        startTrackingScenario(JankScenario.PERIODIC_REPORTING);
        LazyHolder.HANDLER.postDelayed(mPeriodicMetricReporter, PERIODIC_METRIC_DELAY_MS);
    }

    public void stopReportingPeriodicMetrics() {
        // Disable mPeriodicMetricReporter looping, and return early if it was already disabled.
        if (!mIsPeriodicReporterLooping.getAndSet(false)) {
            return;
        }
        // Remove any existing mPeriodicMetricReporter delayed tasks.
        LazyHolder.HANDLER.removeCallbacks(mPeriodicMetricReporter);
        // Run mPeriodicMetricReporter one last time immediately.
        LazyHolder.HANDLER.post(mPeriodicMetricReporter);
    }
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import android.os.Handler;
import android.os.HandlerThread;

import androidx.annotation.Nullable;

import java.util.HashMap;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * This class receives requests to start and stop jank scenario tracking and runs them in a
 * HandlerThread it creates. In addition it handles the recording of periodic jank metrics.
 */
public class JankReportingScheduler {
    private static final long PERIODIC_METRIC_DELAY_MS = 5_000;
    private final FrameMetricsStore mFrameMetricsStore;
    // TODO(b/308551047): Fix/cleanup this member variable. We do query the map but we never add
    // anything to it.
    private final HashMap<Integer, JankReportingRunnable> mRunnableStore;

    public JankReportingScheduler(FrameMetricsStore frameMetricsStore) {
        mFrameMetricsStore = frameMetricsStore;
        mRunnableStore = new HashMap<Integer, JankReportingRunnable>();
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
                        getOrCreateHandler()
                                .postDelayed(mPeriodicMetricReporter, PERIODIC_METRIC_DELAY_MS);
                    }
                }
            };

    @Nullable protected HandlerThread mHandlerThread;
    @Nullable private Handler mHandler;
    private final AtomicBoolean mIsPeriodicReporterLooping = new AtomicBoolean(false);

    public void startTrackingScenario(@JankScenario int scenario) {
        // We check to see if there was already a stop task queued at some point and attempt to
        // cancel it. Regardless we send the startTracking runnable because we will ignore this
        // start if the stop did get canceled and the stopTask already ran we'll start a new
        // scenario.
        JankReportingRunnable stopTask = mRunnableStore.get(scenario);
        if (stopTask != null) {
            getOrCreateHandler().removeCallbacks(stopTask);
            mRunnableStore.remove(scenario);
        }
        getOrCreateHandler()
                .post(
                        new JankReportingRunnable(
                                mFrameMetricsStore,
                                scenario,
                                /* isStartingTracking= */ true,
                                mHandler,
                                null));
    }

    public void finishTrackingScenario(@JankScenario int scenario) {
        finishTrackingScenario(scenario, -1);
    }

    public void finishTrackingScenario(@JankScenario int scenario, long endScenarioTimeNs) {
        finishTrackingScenario(scenario, JankEndScenarioTime.endAt(endScenarioTimeNs));
    }

    public void finishTrackingScenario(
            @JankScenario int scenario, JankEndScenarioTime endScenarioTime) {
        // We store the stop task in case the delay is greater than zero and we start this scenario
        // again.
        JankReportingRunnable runnable =
                mRunnableStore.getOrDefault(
                        scenario,
                        new JankReportingRunnable(
                                mFrameMetricsStore,
                                scenario,
                                /* isStartingTracking= */ false,
                                mHandler,
                                endScenarioTime));
        getOrCreateHandler().post(runnable);
    }

    public Handler getOrCreateHandler() {
        if (mHandler == null) {
            mHandlerThread = new HandlerThread("Jank-Tracker");
            mHandlerThread.start();
            mHandler = new Handler(mHandlerThread.getLooper());
            mHandler.post(
                    new Runnable() {
                        @Override
                        public void run() {
                            mFrameMetricsStore.initialize();
                        }
                    });
        }
        return mHandler;
    }

    public void startReportingPeriodicMetrics() {
        // If mIsPeriodicReporterLooping was already true then there's no need to post another task.
        if (mIsPeriodicReporterLooping.getAndSet(true)) {
            return;
        }
        startTrackingScenario(JankScenario.PERIODIC_REPORTING);
        getOrCreateHandler().postDelayed(mPeriodicMetricReporter, PERIODIC_METRIC_DELAY_MS);
    }

    public void stopReportingPeriodicMetrics() {
        // Disable mPeriodicMetricReporter looping, and return early if it was already disabled.
        if (!mIsPeriodicReporterLooping.getAndSet(false)) {
            return;
        }
        // Remove any existing mPeriodicMetricReporter delayed tasks.
        getOrCreateHandler().removeCallbacks(mPeriodicMetricReporter);
        // Run mPeriodicMetricReporter one last time immediately.
        getOrCreateHandler().post(mPeriodicMetricReporter);
    }
}

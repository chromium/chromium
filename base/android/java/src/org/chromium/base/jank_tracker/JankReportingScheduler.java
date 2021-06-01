// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import android.os.Handler;
import android.os.HandlerThread;

import androidx.annotation.Nullable;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * This class receives requests to start and stop jank scenario tracking and runs them in a
 * HandlerThread it creates. In addition it handles the recording of periodic jank metrics.
 */
class JankReportingScheduler {
    private static final long PERIODIC_METRIC_DELAY_MS = 30_000;
    private final FrameMetricsStore mFrameMetricsStore;

    JankReportingScheduler(FrameMetricsStore frameMetricsStore) {
        mFrameMetricsStore = frameMetricsStore;
    }

    private final Runnable mPeriodicMetricReporter = new Runnable() {
        @Override
        public void run() {
            finishTrackingScenario(JankScenario.PERIODIC_REPORTING);

            if (mIsPeriodicReporterLooping.get()) {
                startTrackingScenario(JankScenario.PERIODIC_REPORTING);
                getOrCreateHandler().postDelayed(mPeriodicMetricReporter, PERIODIC_METRIC_DELAY_MS);
            }
        }
    };

    @Nullable
    protected HandlerThread mHandlerThread;
    @Nullable
    private Handler mHandler;
    private final AtomicBoolean mIsPeriodicReporterLooping = new AtomicBoolean(false);

    void startTrackingScenario(@JankScenario int scenario) {
        getOrCreateHandler().post(new JankReportingRunnable(
                mFrameMetricsStore, scenario, /* isStartingTracking= */ true));
    }

    void finishTrackingScenario(@JankScenario int scenario) {
        getOrCreateHandler().post(new JankReportingRunnable(
                mFrameMetricsStore, scenario, /* isStartingTracking= */ false));
    }

    protected Handler getOrCreateHandler() {
        if (mHandler == null) {
            mHandlerThread = new HandlerThread("Jank-Tracker");
            mHandlerThread.start();
            mHandler = new Handler(mHandlerThread.getLooper());
        }
        return mHandler;
    }

    void startReportingPeriodicMetrics() {
        // If mIsPeriodicReporterLooping was already true then there's no need to post another task.
        if (mIsPeriodicReporterLooping.getAndSet(true)) {
            return;
        }
        startTrackingScenario(JankScenario.PERIODIC_REPORTING);
        getOrCreateHandler().postDelayed(mPeriodicMetricReporter, PERIODIC_METRIC_DELAY_MS);
    }

    void stopReportingPeriodicMetrics() {
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
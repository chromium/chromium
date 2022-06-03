// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import org.chromium.base.library_loader.LibraryLoader;

/**
 * This runnable receives a FrameMetricsStore instance and starts/stops tracking a given scenario.
 * When a scenario stops it takes its metrics and sends them to native to be recorded in UMA.
 * This is executed by JankReportingScheduler on its own thread.
 */
class JankReportingRunnable implements Runnable {
    private final FrameMetricsStore mMetricsStore;
    private final @JankScenario int mScenario;
    private final boolean mIsStartingTracking;

    JankReportingRunnable(FrameMetricsStore metricsStore, @JankScenario int scenario,
            boolean isStartingTracking) {
        mMetricsStore = metricsStore;
        mScenario = scenario;
        mIsStartingTracking = isStartingTracking;
    }

    @Override
    public void run() {
        if (mIsStartingTracking) {
            mMetricsStore.startTrackingScenario(mScenario);
        } else {
            FrameMetrics frames = mMetricsStore.stopTrackingScenario(mScenario);
            if (frames == null || frames.timestampsNs.length == 0) {
                return;
            }

            if (!LibraryLoader.getInstance().isInitialized()) {
                return;
            }

            JankMetrics metrics = JankMetricCalculator.calculateJankMetrics(frames);
            // TODO(salg@): Cache metrics in case native takes >30s to initialize.
            JankMetricUMARecorder.recordJankMetricsToUMA(metrics, mScenario);
        }
    }
}

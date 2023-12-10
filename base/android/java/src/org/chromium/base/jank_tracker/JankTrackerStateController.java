// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import android.view.Window;

import org.chromium.base.Log;

/**
 * A simple holder class to enable easy starting and stopping of metric listening as well as
 * periodic reporting. This is used by JankTrackerImpl to hold the listener reference and this class
 * should be hooked up to some sort of listener to when to start/stop listening and periodic
 * metrics.
 */
public class JankTrackerStateController {
    private static final String TAG = "JankTracker";
    protected final FrameMetricsListener mFrameMetricsListener;
    protected final JankReportingScheduler mReportingScheduler;

    public JankTrackerStateController(
            FrameMetricsListener listener, JankReportingScheduler scheduler) {
        mFrameMetricsListener = listener;
        mReportingScheduler = scheduler;
    }

    public void startPeriodicReporting() {
        mReportingScheduler.startReportingPeriodicMetrics();
    }

    public void stopPeriodicReporting() {
        mReportingScheduler.stopReportingPeriodicMetrics();
    }

    public void startMetricCollection(Window window) {
        mFrameMetricsListener.setIsListenerRecording(true);
        if (window != null) {
            window.addOnFrameMetricsAvailableListener(
                    mFrameMetricsListener, mReportingScheduler.getOrCreateHandler());
        }
    }

    public void stopMetricCollection(Window window) {
        mFrameMetricsListener.setIsListenerRecording(false);
        if (window != null) {
            try {
                window.removeOnFrameMetricsAvailableListener(mFrameMetricsListener);
            } catch (IllegalArgumentException e) {
                // Adding the listener failed for whatever reason, so it could not be unregistered.
                Log.e(
                        TAG,
                        "Could not remove listener %s from window %s",
                        mFrameMetricsListener,
                        window);
            }
        }
    }

    // Extra methods for subclasses that need to perform extra work on initialization/destruction.
    public void initialize() {}

    public void destroy() {}
}

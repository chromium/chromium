// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import android.os.Build.VERSION_CODES;
import android.view.FrameMetrics;
import android.view.Window;
import android.view.Window.OnFrameMetricsAvailableListener;

import androidx.annotation.RequiresApi;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * This class receives  OnFrameMetricsAvailableListener.onFrameMetricsAvailable() callbacks and
 * records frame durations in a FrameMetricsStore instance. Recording can be toggled from any
 * thread, but the actual recording occurs in the thread specified when this is attached to a window
 * with addOnFrameMetricsAvailableListener(). This class only adds data to the provided
 * FrameMetricsStore instance, its owner must clear it to avoid OOMs.
 */
@RequiresApi(api = VERSION_CODES.N)
class FrameMetricsListener implements OnFrameMetricsAvailableListener {
    private final FrameMetricsStore mFrameMetricsStore;
    private final AtomicBoolean mIsRecording;

    FrameMetricsListener(FrameMetricsStore frameMetricsStore) {
        mFrameMetricsStore = frameMetricsStore;
        mIsRecording = new AtomicBoolean(false);
    }

    /**
     * Toggles recording into JankMetricsStore, can be invoked from any thread.
     * @param isRecording
     */
    public void setIsListenerRecording(boolean isRecording) {
        mIsRecording.set(isRecording);
    }

    @RequiresApi(api = VERSION_CODES.N)
    @Override
    public void onFrameMetricsAvailable(
            Window window, FrameMetrics frameMetrics, int dropCountSinceLastInvocation) {
        if (!mIsRecording.get()) {
            return;
        }

        long frameTotalDurationNs = frameMetrics.getMetric(FrameMetrics.TOTAL_DURATION);

        long timestampNs = frameMetrics.getMetric(FrameMetrics.VSYNC_TIMESTAMP);

        mFrameMetricsStore.addFrameMeasurement(
                timestampNs, frameTotalDurationNs, dropCountSinceLastInvocation);
    }
}
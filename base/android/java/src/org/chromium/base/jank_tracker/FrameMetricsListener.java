// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import android.os.Build.VERSION_CODES;
import android.view.FrameMetrics;
import android.view.Window;
import android.view.Window.OnFrameMetricsAvailableListener;

import androidx.annotation.RequiresApi;

import org.chromium.base.TraceEvent;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * This class receives OnFrameMetricsAvailableListener.onFrameMetricsAvailable() callbacks and
 * records frame durations in a FrameMetricsStore instance.
 */
@RequiresApi(api = VERSION_CODES.N)
public class FrameMetricsListener implements OnFrameMetricsAvailableListener {
    private final FrameMetricsStore mFrameMetricsStore;
    private AtomicBoolean mIsRecording = new AtomicBoolean(false);

    public FrameMetricsListener(FrameMetricsStore frameMetricsStore) {
        mFrameMetricsStore = frameMetricsStore;
    }

    /**
     * Toggles recording into FrameMetricsStore. When recording is stopped, reports accumulated
     * metrics.
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
        long frame_start_vsync_ts = frameMetrics.getMetric(FrameMetrics.VSYNC_TIMESTAMP);

        try (TraceEvent e = TraceEvent.scoped(
                     "onFrameMetricsAvailable", Long.toString(frameTotalDurationNs))) {
            long deadlineNs = frameMetrics.getMetric(FrameMetrics.DEADLINE);
            boolean isJanky = frameTotalDurationNs >= deadlineNs;
            mFrameMetricsStore.addFrameMeasurement(
                    frameTotalDurationNs, isJanky, frame_start_vsync_ts);
        }
    }
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import android.os.Build.VERSION_CODES;
import android.os.SystemClock;
import android.view.FrameMetrics;
import android.view.Window;
import android.view.Window.OnFrameMetricsAvailableListener;

import androidx.annotation.RequiresApi;

import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;

/**
 * This class receives OnFrameMetricsAvailableListener.onFrameMetricsAvailable() callbacks and
 * records frame durations in a FrameMetricsStore instance.
 */
@RequiresApi(api = VERSION_CODES.N)
public class FrameMetricsListener implements OnFrameMetricsAvailableListener {
    private final FrameMetricsStore mFrameMetricsStore;
    private boolean mIsRecording;

    // The reporting interval start and duration are passed to the reporting code and used in the
    // 'JankMetricsReportingInterval' trace event.
    private long mReportingIntervalStartTime;
    private long mReportingIntervalDurationMillis;

    private final ThreadUtils.ThreadChecker mThreadChecker = new ThreadUtils.ThreadChecker();

    public FrameMetricsListener(FrameMetricsStore frameMetricsStore) {
        mFrameMetricsStore = frameMetricsStore;
    }

    /**
     * Toggles recording into FrameMetricsStore. When recording is stopped, reports accumulated
     * metrics.
     * @param isRecording
     */
    public void setIsListenerRecording(boolean isRecording) {
        mThreadChecker.assertOnValidThread();
        mIsRecording = isRecording;
        if (isRecording && mReportingIntervalStartTime == 0) {
            mReportingIntervalStartTime = SystemClock.uptimeMillis();
        } else if (!isRecording) {
            if (mReportingIntervalStartTime != 0) {
                mReportingIntervalDurationMillis =
                        SystemClock.uptimeMillis() - mReportingIntervalStartTime;
            }
            reportMetrics();
        }
    }

    @RequiresApi(api = VERSION_CODES.N)
    @Override
    public void onFrameMetricsAvailable(
            Window window, FrameMetrics frameMetrics, int dropCountSinceLastInvocation) {
        mThreadChecker.assertOnValidThread();
        if (!mIsRecording) {
            return;
        }

        long frameTotalDurationNs = frameMetrics.getMetric(FrameMetrics.TOTAL_DURATION);

        try (TraceEvent e = TraceEvent.scoped(
                     "onFrameMetricsAvailable", Long.toString(frameTotalDurationNs))) {
            long deadlineNs = frameMetrics.getMetric(FrameMetrics.DEADLINE);
            boolean isJanky = frameTotalDurationNs >= deadlineNs;
            mFrameMetricsStore.addFrameMeasurement(frameTotalDurationNs, isJanky);
        }
    }

    private void reportMetrics() {
        JankMetricUMARecorder.recordJankMetricsToUMA(mFrameMetricsStore.takeMetrics(),
                mReportingIntervalStartTime, mReportingIntervalDurationMillis);
        mReportingIntervalStartTime = 0;
        mReportingIntervalDurationMillis = 0;
    }
}

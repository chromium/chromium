// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import android.content.Context;
import android.hardware.display.DisplayManager;
import android.hardware.display.DisplayManager.DisplayListener;
import android.os.Build.VERSION_CODES;
import android.view.Display;
import android.view.FrameMetrics;
import android.view.Window;
import android.view.Window.OnFrameMetricsAvailableListener;

import androidx.annotation.RequiresApi;

import org.chromium.base.ContextUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.TraceEvent;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * This class receives OnFrameMetricsAvailableListener.onFrameMetricsAvailable() callbacks and
 * records frame durations in a FrameMetricsStore instance.
 */
@RequiresApi(api = VERSION_CODES.N)
public class FrameMetricsListener implements OnFrameMetricsAvailableListener {
    private class DisplayListenerBackend implements DisplayListener {
        public void startListening() {
            Context appCtx = ContextUtils.getApplicationContext();
            DisplayManager displayManager =
                    (DisplayManager) appCtx.getSystemService(Context.DISPLAY_SERVICE);
            displayManager.registerDisplayListener(this, /* handler= */ null);
        }

        @Override
        public void onDisplayAdded(int sdkDisplayId) {}

        @Override
        public void onDisplayRemoved(int sdkDisplayId) {}

        @Override
        public void onDisplayChanged(int sdkDisplayId) {
            maybeUpdateRefreshRate();
        }
    }

    private DisplayListenerBackend mBackend = new DisplayListenerBackend();

    private final FrameMetricsStore mFrameMetricsStore;
    private AtomicBoolean mIsRecording = new AtomicBoolean(false);
    // Microseconds between each frame.
    private long mVsyncInterval;

    public FrameMetricsListener(FrameMetricsStore frameMetricsStore) {
        mFrameMetricsStore = frameMetricsStore;
        mBackend.startListening();
        maybeUpdateRefreshRate();
    }

    private void maybeUpdateRefreshRate() {
        Context appCtx = ContextUtils.getApplicationContext();
        DisplayManager displayManager =
                (DisplayManager) appCtx.getSystemService(Context.DISPLAY_SERVICE);
        Display display = displayManager.getDisplay(Display.DEFAULT_DISPLAY);
        if (display == null) {
            return;
        }
        float refreshRate = display.getRefreshRate();
        final long kMicrosecondsPerSecond = 1000_000L;
        mVsyncInterval = kMicrosecondsPerSecond / ((long) refreshRate);
        TraceEvent.instant(
                "FrameMetricsListener.maybeUpdateRefreshRate", Long.toString(mVsyncInterval));
    }

    /**
     * Toggles recording into FrameMetricsStore. When recording is stopped, reports accumulated
     * metrics.
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

        try (TraceEvent e =
                TraceEvent.scoped("onFrameMetricsAvailable", Long.toString(frameTotalDurationNs))) {
            // FrameMetrics.DEADLINE was added in API level 31(S).
            // TODO(b/311139161): Update RequiresApi level to Android S.
            long deadlineNs = frameMetrics.getMetric(FrameMetrics.DEADLINE);
            int missedVsyncs = 0;
            if (frameTotalDurationNs >= deadlineNs) {
                long frameDeadlineDeltaUs =
                        (frameTotalDurationNs - deadlineNs) / TimeUtils.NANOSECONDS_PER_MICROSECOND;
                missedVsyncs = (int) ((frameDeadlineDeltaUs + mVsyncInterval) / mVsyncInterval);
            }
            mFrameMetricsStore.addFrameMeasurement(
                    frameTotalDurationNs, missedVsyncs, frame_start_vsync_ts);
        }
    }
}

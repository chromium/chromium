// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import android.app.Activity;
import android.os.Build.VERSION_CODES;
import android.os.Handler;
import android.os.HandlerThread;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ThreadUtils.ThreadChecker;
import org.chromium.base.library_loader.LibraryLoader;

import java.lang.ref.WeakReference;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * This class takes an activity, listens and records FrameMetrics in a JankMetricMeasurement
 * instance. A new HandlerThread is started to listen to FrameMetric events.
 * In addition it starts/stops recording based on activity lifecycle.
 */
@RequiresApi(api = VERSION_CODES.N)
class JankActivityTracker implements ActivityStateListener {
    private static final long METRIC_DELAY_MS = 30_000;

    static JankActivityTracker create(Activity context) {
        JankMetricMeasurement measurement = new JankMetricMeasurement();
        JankFrameMetricsListener listener = new JankFrameMetricsListener(measurement);
        return new JankActivityTracker(context, listener, measurement);
    }

    private final JankFrameMetricsListener mFrameMetricsListener;
    private final JankMetricMeasurement mMeasurement;
    private final AtomicBoolean mIsMetricReporterLooping = new AtomicBoolean(false);
    private final ThreadChecker mThreadChecker = new ThreadChecker();

    private final Runnable mMetricReporter = new Runnable() {
        @Override
        public void run() {
            if (LibraryLoader.getInstance().isInitialized()) {
                JankMetricUMARecorder.recordJankMetricsToUMA(mMeasurement.getMetrics());
            }
            // TODO(salg@): Cache metrics in case native takes >30s to initialize.
            mMeasurement.clear();

            if (mIsMetricReporterLooping.get()) {
                getOrCreateHandler().postDelayed(mMetricReporter, METRIC_DELAY_MS);
            }
        }
    };

    @Nullable
    protected HandlerThread mHandlerThread;
    @Nullable
    private Handler mHandler;
    private WeakReference<Activity> mActivityReference;

    JankActivityTracker(Activity context, JankFrameMetricsListener listener,
            JankMetricMeasurement measurement) {
        mActivityReference = new WeakReference<>(context);
        mFrameMetricsListener = listener;
        mMeasurement = measurement;
    }

    void initialize() {
        mThreadChecker.assertOnValidThreadAndState();
        Activity activity = mActivityReference.get();
        if (activity != null) {
            ApplicationStatus.registerStateListenerForActivity(this, activity);
            @ActivityState
            int activityState = ApplicationStatus.getStateForActivity(activity);
            onActivityStateChange(activity, activityState);
            activity.getWindow().addOnFrameMetricsAvailableListener(
                    mFrameMetricsListener, getOrCreateHandler());
        }
    }

    void destroy() {
        mThreadChecker.assertOnValidThreadAndState();
        ApplicationStatus.unregisterActivityStateListener(this);
        stopMetricRecording();
        stopReportingTimer();
        Activity activity = mActivityReference.get();
        if (activity != null) {
            activity.getWindow().removeOnFrameMetricsAvailableListener(mFrameMetricsListener);
        }
        mThreadChecker.destroy();
    }

    protected Handler getOrCreateHandler() {
        if (mHandler == null) {
            mHandlerThread = new HandlerThread("Jank-Tracker");
            mHandlerThread.start();
            mHandler = new Handler(mHandlerThread.getLooper());
        }
        return mHandler;
    }

    private void startReportingTimer() {
        mThreadChecker.assertOnValidThreadAndState();
        // If mIsMetricReporterLooping was already true then there's no need to post another task.
        if (mIsMetricReporterLooping.getAndSet(true)) {
            return;
        }
        getOrCreateHandler().postDelayed(mMetricReporter, METRIC_DELAY_MS);
    }

    private void stopReportingTimer() {
        mThreadChecker.assertOnValidThreadAndState();
        if (!mIsMetricReporterLooping.get()) {
            return;
        }
        // Remove any existing mMetricReporter delayed tasks.
        getOrCreateHandler().removeCallbacks(mMetricReporter);
        // Disable mMetricReporter looping.
        mIsMetricReporterLooping.set(false);
        // Run mMetricReporter one last time immediately.
        getOrCreateHandler().post(mMetricReporter);
    }

    private void startMetricRecording() {
        mThreadChecker.assertOnValidThreadAndState();
        mFrameMetricsListener.setIsListenerRecording(true);
    }

    private void stopMetricRecording() {
        mThreadChecker.assertOnValidThreadAndState();
        mFrameMetricsListener.setIsListenerRecording(false);
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        switch (newState) {
            case ActivityState.STARTED: // Intentional fallthrough.
            case ActivityState.RESUMED:
                startReportingTimer();
                startMetricRecording();
                break;
            case ActivityState.PAUSED:
                // This method can be called at any moment safely, we want to report metrics even
                // when the activity is paused.
                startReportingTimer();
                stopMetricRecording();
                break;
            case ActivityState.STOPPED:
                stopMetricRecording();
                stopReportingTimer();
                break;
        }
    }
}
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Handler;
import android.os.Looper;

import androidx.annotation.IntDef;

import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.cached_flags.IntCachedFieldTrialParameter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Class to track the trampoline activity to ensure it is properly destroyed after handling the
 * notifications. For each notification intent, this class will give some default timeout for it to
 * complete before the trampoline activity is killed. Notification handlers can request an extension
 * of the timeout when it starts processing the intent. Once a handler finishes processing its
 * notification, it should inform this class about the corresponding job ID. The timeout will then
 * be set to the latest remaining job that is expected to finish.
 */
public class TrampolineActivityTracker {
    // Grace period to keep the activity alive.
    private static final String TIMEOUT_PRIOR_NATIVE_INIT_PARAM =
            "timeout_in_millis_prior_native_init";
    private static final String IMMEDIATE_JOB_DURATION_MILLIS = "minimum_job_duration_millis";
    private static final String NORMAL_JOB_DURATION_MILLIS = "normal_job_duration_millis";
    private static final String LONG_JOB_DURATION_MILLIS = "long_job_duration_millis";
    private static final int TIMEOUT_PRIOR_NATIVE_INIT_IN_MILLISECONDS = 5 * 1000;
    private static final int IMMEDIATE_JOB_DURATION_IN_MILLISECONDS = 10;
    private static final int NORMAL_JOB_DURATION_IN_MILLISECONDS = 1 * 1000;
    private static final int LONG_JOB_DURATION_IN_MILLISECONDS = 8 * 1000;
    static final int INVALID_JOB_HANDLE = -1;

    public static final IntCachedFieldTrialParameter TIMEOUT_PRIOR_NATIVE_INIT_VALUE =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.NOTIFICATION_TRAMPOLINE,
                    TIMEOUT_PRIOR_NATIVE_INIT_PARAM,
                    TIMEOUT_PRIOR_NATIVE_INIT_IN_MILLISECONDS);

    public static final IntCachedFieldTrialParameter IMMEDIATE_JOB_DURATION_VALUE =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.NOTIFICATION_TRAMPOLINE,
                    IMMEDIATE_JOB_DURATION_MILLIS,
                    IMMEDIATE_JOB_DURATION_IN_MILLISECONDS);

    public static final IntCachedFieldTrialParameter NORMAL_JOB_DURATION_VALUE =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.NOTIFICATION_TRAMPOLINE,
                    NORMAL_JOB_DURATION_MILLIS,
                    NORMAL_JOB_DURATION_IN_MILLISECONDS);

    public static final IntCachedFieldTrialParameter LONG_JOB_DURATION_VALUE =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.NOTIFICATION_TRAMPOLINE,
                    LONG_JOB_DURATION_MILLIS,
                    LONG_JOB_DURATION_IN_MILLISECONDS);

    @SuppressLint("StaticFieldLeak")
    private static TrampolineActivityTracker sInstance;

    private final Runnable mRunnable;
    private final Map<String, Long> mEstimatedJobCompletionTimeMap = new HashMap<>();

    private boolean mNativeInitialized;
    private Activity mTrackedActivity;

    // Number of activities waiting for notification intent handling.
    private Handler mHandler;

    // The elapsed real time in Milliseconds to finish the tracked activity.
    private long mActivityFinishTimeInMillis;

    // The latest time when notification intent is started.
    private long mNotificationIntentStartTime;
    // Whether native is initialized when the most recent notification intent is started.
    private boolean mIsNativeInitializedWhenIntentStarted;
    // Number of notifications that is currently processed in parallel.
    private int mNotificationInProcessing;

    /** Defines the job duration */
    @IntDef({JobDuration.IMMEDIATE, JobDuration.NORMAL, JobDuration.LONG})
    @Retention(RetentionPolicy.SOURCE)
    public @interface JobDuration {
        int IMMEDIATE = 0; /* The job will immediately finish*/
        int NORMAL = 1; /* The job will take normal amount of time */
        int LONG = 2; /* The job may take a longer time */
    }

    /**
     * Returns the singleton instance, lazily creating one if needed.
     *
     * @return The singleton instance.
     */
    public static TrampolineActivityTracker getInstance() {
        ThreadUtils.assertOnUiThread();

        if (sInstance == null) {
            sInstance = new TrampolineActivityTracker();
        }
        return sInstance;
    }

    TrampolineActivityTracker() {
        mHandler = new Handler(Looper.getMainLooper());
        mRunnable = this::finishTrackedActivity;
    }

    /**
     * Called when TrampolineActivity is created.
     *
     * @param hasVisibleActivity Whether Chrome has activities visible to user.
     */
    public void onNotificationIntentStarted() {
        long lastStartTime = mNotificationIntentStartTime;
        mNotificationIntentStartTime = TimeUtils.elapsedRealtimeMillis();
        mIsNativeInitializedWhenIntentStarted = mNativeInitialized;

        // Some notification might not report their job, Reset
        // `mNotificationInProcessing` after some time.
        if (mNotificationIntentStartTime - lastStartTime > getJobDuration(JobDuration.LONG)) {
            mNotificationInProcessing = 0;
        }
        mNotificationInProcessing++;
        RecordHistogram.recordCount100Histogram(
                "Notifications.Android.IntentProcessedInParallel", mNotificationInProcessing);
    }

    /**
     * Called to track a trampoline activity. If a activity is already tracked, return false.
     * Otherwise, start tracking the new activity.
     *
     * @return Whether the activity is being tracked.
     */
    public boolean tryTrackActivity(Activity activity) {
        long delayToFinish =
                mNativeInitialized
                        ? getJobDuration(JobDuration.NORMAL)
                        : TIMEOUT_PRIOR_NATIVE_INIT_VALUE.getValue();
        long estimatedFinishTime = TimeUtils.elapsedRealtimeMillis() + delayToFinish;
        // Extend the timeout if necessary.
        if (estimatedFinishTime > mActivityFinishTimeInMillis) {
            updateActivityFinishTime(estimatedFinishTime);
        }

        if (mTrackedActivity == null) {
            mTrackedActivity = activity;
            return true;
        }
        return false;
    }

    /** Called to finish the tracked activity due to timeout. */
    public void finishTrackedActivity() {
        if (mTrackedActivity != null) {
            mTrackedActivity.finish();
            mTrackedActivity = null;
        }
        mHandler.removeCallbacks(mRunnable);
        mEstimatedJobCompletionTimeMap.clear();
        mActivityFinishTimeInMillis = 0;
    }

    /**
     * Called to inform that a job started processing notification intent.
     *
     * @param jobId ID of the Job.
     * @param jobDuration Estimated time to complete the job.
     */
    public void startProcessingNewIntent(String jobId, @JobDuration int jobDuration) {
        if (mTrackedActivity == null || jobId == null) {
            return;
        }

        long estimatedFinishTime = TimeUtils.elapsedRealtimeMillis() + getJobDuration(jobDuration);

        // Extend the timeout if necessary.
        if (estimatedFinishTime > mActivityFinishTimeInMillis) {
            updateActivityFinishTime(estimatedFinishTime);
        }
        mEstimatedJobCompletionTimeMap.put(jobId, estimatedFinishTime);
    }

    /**
     * Called when a notification intent finished processing.
     *
     * @param jobId The ID of the Job.
     */
    public void onIntentCompleted(String jobId) {
        if (mNotificationIntentStartTime > 0) {
            if (mNotificationInProcessing > 0) {
                mNotificationInProcessing--;
            }
            String histogram =
                    "Notifications.Android.IntentStartToFinishDuration."
                            + (mIsNativeInitializedWhenIntentStarted
                                    ? "NativeInitialized"
                                    : "NativeUninitialized");
            RecordHistogram.recordTimesHistogram(
                    histogram, TimeUtils.elapsedRealtimeMillis() - mNotificationIntentStartTime);
        }

        if (jobId == null) return;

        mEstimatedJobCompletionTimeMap.remove(jobId);

        if (mEstimatedJobCompletionTimeMap.isEmpty()) {
            finishTrackedActivity();
            return;
        }

        long latestCompletionTime = Collections.max(mEstimatedJobCompletionTimeMap.values());
        updateActivityFinishTime(latestCompletionTime);
    }

    /** Called when native library is initialized. */
    public void onNativeInitialized() {
        mNativeInitialized = true;

        if (mNotificationIntentStartTime > 0) {
            assert !mIsNativeInitializedWhenIntentStarted;
            RecordHistogram.recordTimesHistogram(
                    "Android.Notification.Startup.NativeInitialized",
                    TimeUtils.elapsedRealtimeMillis() - mNotificationIntentStartTime);
        }
        if (mTrackedActivity == null) return;

        // There are no jobs yet, wait a small amount of time for jobs to be added. If there
        // are some jobs, just use the timeout from those jobs
        if (mEstimatedJobCompletionTimeMap.isEmpty()) {
            long estimatedFinishTime =
                    TimeUtils.elapsedRealtimeMillis() + getJobDuration(JobDuration.NORMAL);
            updateActivityFinishTime(estimatedFinishTime);
        }
    }

    /** Update the activity finish time. */
    void updateActivityFinishTime(long activityFinishTime) {
        long delayToFinish = activityFinishTime - TimeUtils.elapsedRealtimeMillis();
        // If the finish time has already passed, finish the activity.
        if (delayToFinish < 0) {
            finishTrackedActivity();
            return;
        }

        // Reschedule when the activity should be finished.
        mActivityFinishTimeInMillis = activityFinishTime;
        mHandler.removeCallbacks(mRunnable);
        mHandler.postDelayed(mRunnable, delayToFinish);
    }

    private int getJobDuration(@JobDuration int jobDuration) {
        switch (jobDuration) {
            case JobDuration.IMMEDIATE:
                return IMMEDIATE_JOB_DURATION_VALUE.getValue();
            case JobDuration.NORMAL:
                return NORMAL_JOB_DURATION_VALUE.getValue();
            case JobDuration.LONG:
                return LONG_JOB_DURATION_VALUE.getValue();
        }
        return 0;
    }

    /** Sets the handler for testing purpose. */
    void setHandlerForTesting(Handler handler) {
        mHandler = handler;
    }

    /** Sets whether native is initialized for testing. */
    static void destroy() {
        sInstance = null;
    }
}

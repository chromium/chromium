// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Handler;
import android.os.Looper;

import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

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
    private static final String TIMEOUT_POST_NATIVE_INIT_PARAM =
            "timeout_in_millis_post_native_init";
    private static final int TIMEOUT_PRIOR_NATIVE_INIT_IN_MILLISECONDS = 5 * 1000;
    private static final int TIMEOUT_POST_NATIVE_INIT_IN_MILLISECONDS = 1 * 1000;
    private static final int INVALID_JOB_HANDLE = -1;

    @SuppressLint("StaticFieldLeak")
    private static TrampolineActivityTracker sInstance;

    private final Runnable mRunnable;
    private final Map<Integer, Long> mEstimatedJobCompletionTimeMap = new HashMap<>();

    private boolean mNativeInitialized;
    private Activity mTrackedActivity;

    // Number of activities waiting for notification intent handling.
    private int mCurrentJobId;
    private Handler mHandler;

    // The elapsed real time in Milliseconds to finish the tracked activity.
    private long mActivityFinishTimeInMillis;

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
     * Called to track a trampoline activity. If a activity is already tracked, return false.
     * Otherwise, start tracking the new activity.
     *
     * @return Whether the activity is being tracked.
     */
    public boolean tryTrackActivity(Activity activity) {
        long delayToFinish =
                mNativeInitialized ? getTimeoutPostNativeInitMs() : getTimeoutPriorNativeInitMs();
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
     * @param estimatedTimeInMillis Estimated time to complete the job.
     * @return An integer representing the job Id.
     */
    public int startProcessingNewIntent(long estimatedTimeInMillis) {
        if (mTrackedActivity == null) {
            return INVALID_JOB_HANDLE;
        }

        long estimatedFinishTime = TimeUtils.elapsedRealtimeMillis() + estimatedTimeInMillis;

        // Extend the timeout if necessary.
        if (estimatedFinishTime > mActivityFinishTimeInMillis) {
            updateActivityFinishTime(estimatedFinishTime);
        }
        mEstimatedJobCompletionTimeMap.put(mCurrentJobId, estimatedFinishTime);
        return mCurrentJobId++;
    }

    /**
     * Called when a notification intent finished processing.
     *
     * @param jobId The ID of the job.
     */
    public void onIntentCompleted(int jobId) {
        if (jobId == INVALID_JOB_HANDLE) return;

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

        if (mTrackedActivity == null) return;

        // There are no jobs yet, wait a small amount of time for jobs to be added. If there
        // are some jobs, just use the timeout from those jobs
        if (mEstimatedJobCompletionTimeMap.isEmpty()) {
            long estimatedFinishTime =
                    TimeUtils.elapsedRealtimeMillis() + getTimeoutPostNativeInitMs();
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

    private int getTimeoutPriorNativeInitMs() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.NOTIFICATION_TRAMPOLINE,
                TIMEOUT_PRIOR_NATIVE_INIT_PARAM,
                TIMEOUT_PRIOR_NATIVE_INIT_IN_MILLISECONDS);
    }

    private int getTimeoutPostNativeInitMs() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.NOTIFICATION_TRAMPOLINE,
                TIMEOUT_POST_NATIVE_INIT_PARAM,
                TIMEOUT_POST_NATIVE_INIT_IN_MILLISECONDS);
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

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.PowerManager;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.customtabs.CustomTabsTimeoutOutcome.CustomTabsResetTimeoutOutcome;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.concurrent.TimeUnit;

/**
 * Handles the timeout logic for {@link CustomTabActivity}. When a timeout is specified in the
 * intent, this class will finish the activity if the user leaves the app and returns after the
 * timeout has elapsed.
 */
@NullMarked
class CustomTabActivityTimeoutHandler {

    private static final String TAG = "CustomTabTimeout";

    /**
     * An extra that can be used to provide a timeout in minutes. If the user leaves the app and
     * returns after the timeout has elapsed, the activity will be finished. This extra allows for
     * embedding apps to experiment with the timeout.
     */
    static final String EXTRA_TIMEOUT_MINUTES =
            "org.chromium.chrome.browser.customtabs.EXTRA_TIMEOUT_MINUTES_ENABLED";

    /**
     * An extra that can be used to provide a timeout in minutes. If the user leaves the app and
     * returns after the timeout has elapsed, the activity will be finished. Embedding apps should
     * pass this value to allow for Chrome experiments for the timeout.
     */
    static final String EXTRA_TIMEOUT_MINUTES_ALLOWED =
            "org.chromium.chrome.browser.customtabs.EXTRA_TIMEOUT_MINUTES_ALLOWED";

    /**
     * An extra that can be used to provide a pending intent to be sent to the embedder when the
     * timeout is elapsed. If the pending intent is not specified, the activity will be finished
     * directly.
     */
    static final String EXTRA_TIMEOUT_PENDING_INTENT =
            "org.chromium.chrome.browser.customtabs.EXTRA_TIMEOUT_PENDING_INTENT";

    static final String KEY_LEAVE_TIMESTAMP = "CustomTabActivity.leave_timestamp";

    private final Runnable mFinishActivityRunnable;
    private final boolean mIsTimeoutEnabled;
    private final int mTimeoutMinutes;
    private boolean mOutcomeRecorded;

    @Nullable private final PendingIntent mEmbedderClosingIntent;

    // Timestamp of when the user left the activity, used for timeout logic.
    private long mLeaveTimestamp = -1;
    // Whether the activity is launching an external activity.
    private boolean mIsLaunchingExternalActivity;

    CustomTabActivityTimeoutHandler(Runnable finishActivityRunnable, Intent intent) {
        mFinishActivityRunnable = finishActivityRunnable;
        if (IntentUtils.safeGetParcelableExtra(intent, EXTRA_TIMEOUT_PENDING_INTENT)
                instanceof PendingIntent pendingIntent) {
            mEmbedderClosingIntent = pendingIntent;
        } else {
            mEmbedderClosingIntent = null;
        }
        mIsTimeoutEnabled =
                isTimeoutEnabledForChromeExperiment(intent)
                        || isTimeoutEnabledForEmbedderExperiment(intent);

        if (mIsTimeoutEnabled) {
            // If the embedder experiment is enabled, use the timeout value from the embedder.
            // Otherwise, use the timeout value from the Chrome experiment.
            boolean isEmbedderExperiment = isTimeoutEnabledForEmbedderExperiment(intent);
            RecordHistogram.recordBooleanHistogram(
                    "CustomTabs.ResetTimeout.IsFromEmbedder", isEmbedderExperiment);
            if (isEmbedderExperiment) {
                Log.d(TAG, "Timeout enabled for embedder experiment.");
                mTimeoutMinutes = getTimeoutMinutesForEmbedderExperiment(intent);
            } else {
                Log.d(TAG, "Timeout enabled for Chrome experiment.");
                mTimeoutMinutes = getTimeoutMinutesForChromeExperiment(intent);
            }
        } else {
            mTimeoutMinutes = 0;
        }
    }

    /** To be called from {@link Activity#onStart()}. */
    void onStart() {
        if (!mIsTimeoutEnabled) return;

        // Reset the flag when the activity is started.
        mIsLaunchingExternalActivity = false;
    }

    /** To be called from {@link Activity#onStop()}. */
    void onStop(Context context) {
        if (!mIsTimeoutEnabled) return;

        // Reset the outcome recorded flag for a new leave cycle. This ensures that an outcome
        // can be recorded when the user returns or the activity is destroyed.
        mOutcomeRecorded = false;

        PowerManager powerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        boolean isLockingScreenAction = !powerManager.isInteractive();

        if (isLockingScreenAction) {
            mLeaveTimestamp = -1;
            Log.d(TAG, "onStop: Locking screen, not starting timeout.");
            return;
        }

        if (mIsLaunchingExternalActivity) {
            mLeaveTimestamp = -1;
            Log.d(TAG, "onStop: Intenting to new activity, not starting timeout.");
            return;
        }

        if (mLeaveTimestamp == -1) {
            mLeaveTimestamp = TimeUtils.elapsedRealtimeMillis();
            Log.d(TAG, "onStop: User leaving task, timeout timer started.");
        }
    }

    /** To be called from {@link Activity#onResume()}. */
    void onResume(Context context) {
        if (!mIsTimeoutEnabled) return;

        Log.d(TAG, "onResume: timeoutMinutes: %d", mTimeoutMinutes);
        handleTimeout(context);
    }

    /**
     * Sets a flag indicating whether the CustomTabActivity is launching an external activity. This
     * is used in {@link #onStop(Context)} to determine if the timeout timer should be started.
     */
    void setLaunchingExternalActivity(boolean isLaunchingExternalActivity) {
        if (!mIsTimeoutEnabled) return;

        Log.d(TAG, "setLaunchingExternalActivity: %s", isLaunchingExternalActivity);
        mIsLaunchingExternalActivity = isLaunchingExternalActivity;
    }

    /**
     * Checks if the timeout has elapsed since the user left the activity and finishes it if it has.
     */
    private void handleTimeout(Context context) {
        // If a timeout is specified and the user had previously left the activity.
        if (mLeaveTimestamp != -1) {
            long timeoutMillis = TimeUnit.MINUTES.toMillis(mTimeoutMinutes);
            long elapsedTime = TimeUtils.elapsedRealtimeMillis() - mLeaveTimestamp;

            long elapsedTimeInMinutes = TimeUnit.MILLISECONDS.toMinutes(elapsedTime);
            RecordHistogram.recordExactLinearHistogram(
                    "CustomTabs.ResetTimeout.ElapsedTimeInMinutesOnReturn",
                    (int) elapsedTimeInMinutes,
                    180);

            if (elapsedTime >= timeoutMillis) {
                // Finish the activity if the timeout has elapsed. If an embedder closing intent is
                // specified, send it, otherwise finish the activity.
                if (mEmbedderClosingIntent != null) {
                    try {
                        ActivityOptions options = ActivityOptions.makeBasic();
                        ApiCompatibilityUtils.setActivityOptionsBackgroundActivityStartAllowAlways(
                                options);
                        mEmbedderClosingIntent.send(
                                context,
                                /* code= */ 0,
                                /* intent= */ null,
                                /* onFinished= */ null,
                                /* handler= */ null,
                                /* requiredPermissions= */ null,
                                /* options= */ options.toBundle());
                        record(CustomTabsResetTimeoutOutcome.RESET_TRIGGERED_INTENT);
                    } catch (PendingIntent.CanceledException e) {
                        Log.e(TAG, "Failed to send embedder intent: %s", e);
                        mFinishActivityRunnable.run();
                        record(CustomTabsResetTimeoutOutcome.RESET_TRIGGERED_FALLBACK);
                    }
                    return;
                } else {
                    mFinishActivityRunnable.run();
                    record(CustomTabsResetTimeoutOutcome.RESET_TRIGGERED_FALLBACK);
                }
            } else {
                record(CustomTabsResetTimeoutOutcome.RETURNED_BEFORE_TIMEOUT);
            }
        }
        // Reset the timestamp after checking to ensure the timeout logic only runs once per leave.
        mLeaveTimestamp = -1;
    }

    /** To be called from {@link Activity#onSaveInstanceState(Bundle)}. */
    void onSaveInstanceState(Bundle outState) {
        if (mIsTimeoutEnabled && mLeaveTimestamp != -1) {
            outState.putLong(KEY_LEAVE_TIMESTAMP, mLeaveTimestamp);
        }
    }

    /** To be called from {@link Activity#onCreate(Bundle)} or similar. */
    void restoreInstanceState(@Nullable Bundle savedInstanceState) {
        if (mIsTimeoutEnabled && savedInstanceState != null) {
            mLeaveTimestamp = savedInstanceState.getLong(KEY_LEAVE_TIMESTAMP, -1);
        }
    }

    /** To be called from {@link Activity#onDestroy()}. */
    void onDestroy() {
        // If the activity is destroyed without the timeout being triggered, record it as a manual
        // close.
        if (mIsTimeoutEnabled) {
            record(CustomTabsResetTimeoutOutcome.SESSION_CLOSED_MANUALLY);
        }
    }

    private void record(@CustomTabsResetTimeoutOutcome int outcome) {
        if (mOutcomeRecorded) return;

        CustomTabsTimeoutOutcome.record(outcome);
        mOutcomeRecorded = true;
    }

    private boolean isTimeoutEnabledForChromeExperiment(Intent intent) {
        return IntentUtils.safeHasExtra(intent, EXTRA_TIMEOUT_MINUTES_ALLOWED)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_RESET_TIMEOUT_ENABLED);
    }

    private boolean isTimeoutEnabledForEmbedderExperiment(Intent intent) {
        return IntentUtils.safeHasExtra(intent, EXTRA_TIMEOUT_MINUTES)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_RESET_TIMEOUT_ALLOWED);
    }

    private int getTimeoutMinutesForChromeExperiment(Intent intent) {
        return Math.max(
                IntentUtils.safeGetIntExtra(intent, EXTRA_TIMEOUT_MINUTES_ALLOWED, 0),
                ChromeFeatureList.sCctResetMinimumTimeoutMinutes.getValue());
    }

    private int getTimeoutMinutesForEmbedderExperiment(Intent intent) {
        return Math.max(
                IntentUtils.safeGetIntExtra(intent, EXTRA_TIMEOUT_MINUTES, 0),
                ChromeFeatureList.sCctResetMinimumTimeoutMinutesAllowed.getValue());
    }
}

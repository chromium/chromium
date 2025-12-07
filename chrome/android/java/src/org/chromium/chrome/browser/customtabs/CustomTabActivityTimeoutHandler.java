// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
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
     * returns after the timeout has elapsed, the activity will be finished.
     */
    static final String EXTRA_TIMEOUT_MINUTES =
            "org.chromium.chrome.browser.customtabs.EXTRA_TIMEOUT_MINUTES";

    static final String EXTRA_TIMEOUT_PENDING_INTENT =
            "org.chromium.chrome.browser.customtabs.EXTRA_TIMEOUT_PENDING_INTENT";

    static final String KEY_LEAVE_TIMESTAMP = "CustomTabActivity.leave_timestamp";

    private final Runnable mFinishActivityRunnable;
    private final Intent mIntent;
    private final boolean mIsTimeoutEnabled;

    @Nullable private final PendingIntent mEmbedderClosingIntent;

    // Timestamp of when the user left the activity, used for timeout logic.
    private long mLeaveTimestamp = -1;

    CustomTabActivityTimeoutHandler(Runnable finishActivityRunnable, Intent intent) {
        mFinishActivityRunnable = finishActivityRunnable;
        mIntent = intent;
        if (IntentUtils.safeGetParcelableExtra(intent, EXTRA_TIMEOUT_PENDING_INTENT)
                instanceof PendingIntent pendingIntent) {
            mEmbedderClosingIntent = pendingIntent;
        } else {
            mEmbedderClosingIntent = null;
        }
        mIsTimeoutEnabled =
                IntentUtils.safeHasExtra(intent, EXTRA_TIMEOUT_MINUTES)
                        && ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_RESET_TIMEOUT_ENABLED);
    }

    /**
     * To be called from {@link Activity#onUserLeaveHint()}. If a timeout is specified, save the
     * timestamp when the user leaves.
     */
    void onUserLeaveHint() {
        if (mIsTimeoutEnabled) {
            mLeaveTimestamp = TimeUtils.elapsedRealtimeMillis();
        }
    }

    /** To be called from {@link Activity#onResume()}. */
    void onResume() {
        if (mIsTimeoutEnabled) {
            int timeoutMinutes =
                    Math.max(
                            IntentUtils.safeGetIntExtra(mIntent, EXTRA_TIMEOUT_MINUTES, 0),
                            ChromeFeatureList.sCctResetMinimumTimeoutMinutes.getValue());
            handleTimeout(timeoutMinutes);
        }
    }

    /**
     * Checks if the timeout has elapsed since the user left the activity and finishes it if it has.
     */
    private void handleTimeout(int timeoutMinutes) {
        // If a timeout is specified and the user had previously left the activity.
        if (mLeaveTimestamp != -1) {
            long timeoutMillis = TimeUnit.MINUTES.toMillis(timeoutMinutes);
            long elapsedTime = TimeUtils.elapsedRealtimeMillis() - mLeaveTimestamp;

            if (elapsedTime > timeoutMillis) {
                // Finish the activity if the timeout has elapsed. If an embedder closing intent is
                // specified, send it, otherwise finish the activity.
                if (mEmbedderClosingIntent != null) {
                    try {
                        mEmbedderClosingIntent.send();
                    } catch (PendingIntent.CanceledException e) {
                        Log.e(TAG, "Failed to send embedder intent: %s", e);
                    }
                    return;
                } else {
                    mFinishActivityRunnable.run();
                }
            }
        }
        // Reset the timestamp after checking to ensure the timeout logic only runs once per leave.
        mLeaveTimestamp = -1;
    }

    /** To be called from {@link Activity#onSaveInstanceState(Bundle)}. */
    void onSaveInstanceState(@NonNull Bundle outState) {
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
}

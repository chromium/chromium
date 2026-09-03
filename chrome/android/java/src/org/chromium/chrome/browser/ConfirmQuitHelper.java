// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifetime.ApplicationLifetime;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Handles "Warn Before Quit" on Android, requiring the user to hold the keyboard shortcut to quit
 * Chrome for 1.5 seconds or double-tap it quickly to terminate the application.
 */
@NullMarked
public class ConfirmQuitHelper {
    public static final long HOLD_DURATION_MS = 1500;
    public static final long DOUBLE_TAP_DELTA_MS = 320;

    // LINT.IfChange(AndroidConfirmQuitResult)
    @IntDef({
        ConfirmQuitResult.QUIT_HOLD,
        ConfirmQuitResult.QUIT_DOUBLE_TAP,
        ConfirmQuitResult.CANCELED,
        ConfirmQuitResult.NUM_ENTRIES,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ConfirmQuitResult {
        int QUIT_HOLD = 0;
        int QUIT_DOUBLE_TAP = 1;
        int CANCELED = 2;
        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidConfirmQuitResult)

    private static @Nullable ConfirmQuitHelper sInstance;

    private final Handler mHandler;
    private @Nullable Runnable mQuitRunnable;
    private @Nullable Runnable mPendingCanceledMetricRunnable;
    private @Nullable Toast mToast;
    private long mLastQuitAttemptTime;
    private boolean mIsQuitInProgress;

    /**
     * Returns the singleton instance of {@link ConfirmQuitHelper}.
     *
     * @return The singleton instance.
     */
    public static ConfirmQuitHelper getInstance() {
        if (sInstance == null) {
            sInstance = new ConfirmQuitHelper();
        }
        return sInstance;
    }

    private ConfirmQuitHelper() {
        mHandler = new Handler(Looper.getMainLooper());
    }

    /**
     * Handles a Ctrl+Q quit request on key down. Initiates a hold-to-quit timer and displays a
     * confirmation toast instructing the user to hold the shortcut to quit, or immediately
     * terminates the application if double-tapped within {@link #DOUBLE_TAP_DELTA_MS}.
     *
     * @param context The Android {@link Context} used to display toasts.
     * @return True if the quit request was handled, false otherwise.
     */
    public boolean handleQuitRequest(Context context) {
        long now = SystemClock.elapsedRealtime();

        // Check for double-tap quick exit path.
        if (mLastQuitAttemptTime > 0 && (now - mLastQuitAttemptTime) < DOUBLE_TAP_DELTA_MS) {
            cancelPendingCanceledMetricRunnable();
            mIsQuitInProgress = false;
            cancelPendingQuit(/* dismissToast= */ true);
            executeQuit(ConfirmQuitResult.QUIT_DOUBLE_TAP);
            return true;
        }

        if (mIsQuitInProgress) {
            // Ignore repeat keydown events while already holding down.
            return true;
        }

        cancelPendingCanceledMetricRunnable();
        mLastQuitAttemptTime = now;
        mIsQuitInProgress = true;

        cancelPendingQuit(/* dismissToast= */ true);

        String toastText = context.getString(R.string.confirm_to_quit_toast);
        mToast = Toast.makeText(context.getApplicationContext(), toastText, Toast.LENGTH_SHORT);
        mToast.show();

        mQuitRunnable =
                () -> {
                    mIsQuitInProgress = false;
                    cancelPendingQuit(/* dismissToast= */ true);
                    executeQuit(ConfirmQuitResult.QUIT_HOLD);
                };
        mHandler.postDelayed(mQuitRunnable, HOLD_DURATION_MS);
        return true;
    }

    /**
     * Returns whether a hold-to-quit request is currently in progress.
     *
     * @return True if a quit request is in progress, false otherwise.
     */
    public boolean isQuitInProgress() {
        return mIsQuitInProgress;
    }

    /**
     * Cancels any active hold-to-quit timer and optionally dismisses the confirmation toast.
     *
     * @param dismissToast If true, dismisses any existing confirmation toast immediately; otherwise
     *     leaves it visible for its normal duration.
     */
    public void cancel(boolean dismissToast) {
        if (mIsQuitInProgress) {
            mIsQuitInProgress = false;
            long now = SystemClock.elapsedRealtime();
            long elapsed = now - mLastQuitAttemptTime;
            if (!dismissToast && elapsed < DOUBLE_TAP_DELTA_MS) {
                // Delay recording CANCELED to avoid inflating the metric if the user double-taps.
                mPendingCanceledMetricRunnable =
                        () -> {
                            mPendingCanceledMetricRunnable = null;
                            recordConfirmQuitResult(ConfirmQuitResult.CANCELED);
                        };
                mHandler.postDelayed(mPendingCanceledMetricRunnable, DOUBLE_TAP_DELTA_MS - elapsed);
            } else {
                recordConfirmQuitResult(ConfirmQuitResult.CANCELED);
            }
        } else if (dismissToast && mPendingCanceledMetricRunnable != null) {
            // If an asynchronous CANCELED recording was pending and the toast is now dismissed
            // (e.g. activity paused), record CANCELED immediately.
            cancelPendingCanceledMetricRunnable();
            recordConfirmQuitResult(ConfirmQuitResult.CANCELED);
        }
        cancelPendingQuit(dismissToast);
    }

    private void cancelPendingCanceledMetricRunnable() {
        if (mPendingCanceledMetricRunnable != null) {
            mHandler.removeCallbacks(mPendingCanceledMetricRunnable);
            mPendingCanceledMetricRunnable = null;
        }
    }

    private void cancelPendingQuit(boolean dismissToast) {
        if (mQuitRunnable != null) {
            mHandler.removeCallbacks(mQuitRunnable);
            mQuitRunnable = null;
        }
        if (dismissToast && mToast != null) {
            mToast.cancel();
            mToast = null;
        }
    }

    private void executeQuit(@ConfirmQuitResult int result) {
        recordConfirmQuitResult(result);
        ApplicationLifetime.terminate(/* restart= */ false);
    }

    private static void recordConfirmQuitResult(@ConfirmQuitResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.ConfirmQuit.Result", result, ConfirmQuitResult.NUM_ENTRIES);
    }

    /* package */ static void setInstanceForTesting(@Nullable ConfirmQuitHelper instance) {
        if (sInstance != null) {
            sInstance.cancelPendingCanceledMetricRunnable();
        }
        sInstance = instance;
    }

    /* package */ @Nullable Toast getToastForTesting() {
        return mToast;
    }
}

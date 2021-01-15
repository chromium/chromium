// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.app.Activity;
import android.view.Display;

import androidx.annotation.CallSuper;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CheckDiscard;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.ui.display.DisplayAndroidManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Computes and records metrics for what caused Chrome to be launched.
 */
public abstract class LaunchCauseMetrics implements ApplicationStatus.ApplicationStateListener {
    private static final boolean DEBUG = false;
    private static final String TAG = "LaunchCauseMetrics";

    // Static to avoid recording launch metrics when transitioning between Activities without
    // Chrome leaving the foreground.
    private static boolean sRecordedLaunchCause;

    @VisibleForTesting
    public static final String LAUNCH_CAUSE_HISTOGRAM = "MobileStartup.Experimental.LaunchCause";

    private PerLaunchState mPerLaunchState = new PerLaunchState();
    private BetweenLaunchState mBetweenLaunchState = new BetweenLaunchState();

    // State pertaining to the current launch, reset when Chrome is backgrounded.
    private static class PerLaunchState {
        boolean mReceivedIntent;
        boolean mLaunchedFromRecents;
    }

    // State that persists through Chrome being backgrounded (but not destroyed), reset after
    // computing LaunchCause.
    private static class BetweenLaunchState {
        boolean mReceivedLeaveHint;
        boolean mScreenOffWhenPaused;
    }

    // These values are persisted in histograms. Please do not renumber. Append only.
    @IntDef({LaunchCause.OTHER, LaunchCause.CUSTOM_TAB, LaunchCause.TWA, LaunchCause.RECENTS,
            LaunchCause.RECENTS_OR_BACK, LaunchCause.FOREGROUND_WHEN_LOCKED,
            LaunchCause.MAIN_LAUNCHER_ICON})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LaunchCause {
        int OTHER = 0;
        int CUSTOM_TAB = 1;
        int TWA = 2;
        int RECENTS = 3;
        int RECENTS_OR_BACK = 4;
        int FOREGROUND_WHEN_LOCKED = 5;
        int MAIN_LAUNCHER_ICON = 6;

        int NUM_ENTRIES = 7;
    }

    /**
     * @param activity The Activity context to compute LaunchCause for, used for getting the correct
     *         Display, etc.
     */
    public LaunchCauseMetrics(final Activity activity) {
        ApplicationStatus.registerApplicationStateListener(this);
        ApplicationStatus.registerStateListenerForActivity((a, newState) -> {
            if (newState == ActivityState.PAUSED) {
                mBetweenLaunchState.mScreenOffWhenPaused = isDisplayOff(a);
            }
            if (newState == ActivityState.DESTROYED) {
                ApplicationStatus.unregisterApplicationStateListener(this);
            }
        }, activity);
    }

    @Override
    public void onApplicationStateChange(@ApplicationState int newState) {
        if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES) {
            resetPerLaunchState();
        }
    }

    /**
     * Resets state used to compute launch cause when Chrome is backgrounded.
     */
    @CallSuper
    protected void resetPerLaunchState() {
        sRecordedLaunchCause = false;
        mPerLaunchState = new PerLaunchState();
    }

    private void resetBetweenLaunchState() {
        mBetweenLaunchState = new BetweenLaunchState();
    }

    /**
     * Computes and returns what the cause of the Chrome launch was.
     */
    protected abstract @LaunchCause int computeLaunchCause();

    /**
     * Called after Chrome has launched and all information necessary to compute why Chrome was
     * launched is available.
     *
     * Records UMA metrics for what caused Chrome to launch.
     */
    public void recordLaunchCause() {
        if (!sRecordedLaunchCause) {
            sRecordedLaunchCause = true;

            @LaunchCause
            int cause = LaunchCause.OTHER;

            if (mPerLaunchState.mReceivedIntent) {
                cause = computeLaunchCause();
            } else {
                cause = computeNonIntentLaunchCause();
            }

            if (DEBUG) logLaunchCause(cause);

            RecordHistogram.recordEnumeratedHistogram(
                    LAUNCH_CAUSE_HISTOGRAM, cause, LaunchCause.NUM_ENTRIES);
        }
        resetBetweenLaunchState();
    }

    // If Chrome wasn't launched via an intent, it was either launched from Recents, Back button,
    // or through Screen ON.
    //
    // For posterity: If you're testing this by switching between the Android Settings app, and
    // Chrome, with Chrome set as the debug app, it won't work because Android clears app state and
    // resuming through Recents will instead send a MAIN intent.
    private @LaunchCause int computeNonIntentLaunchCause() {
        if (mPerLaunchState.mLaunchedFromRecents) {
            return LaunchCause.RECENTS;
        }
        if (mBetweenLaunchState.mScreenOffWhenPaused) {
            // It's possible we got here through Recents, if the user tapped a non-Chrome
            // notification after locking their screen with Chrome in the foreground, then
            // returned to Chrome through Recents, and there's no reliable way to detect this.
            // The most likely explanation for arriving here is Chrome was resumed through
            // unlocking their phone.
            return LaunchCause.FOREGROUND_WHEN_LOCKED;
        }
        // If we don't get a UserLeaveHint when leaving Chrome, then back can't return us to Chrome.
        if (!mBetweenLaunchState.mReceivedLeaveHint) {
            return LaunchCause.RECENTS;
        }
        // There's no way to distinguish between Recents and Back when we've received a
        // UserLeaveHint.
        return LaunchCause.RECENTS_OR_BACK;
    }

    /**
     * Called when Chrome receives a new Intent (including both when Chrome is launched, or
     * resumed, through an intent). The Intent may be any Intent, including MAIN, VIEW, and
     * arbitrary explicit intents targeting Chrome.
     */
    public void onReceivedIntent() {
        mPerLaunchState.mReceivedIntent = true;
    }

    /** See {@link Activity#onUserLeaveHint()} */
    public void onUserLeaveHint() {
        mBetweenLaunchState.mReceivedLeaveHint = true;
    }

    /** Called when the Activity is launched from Android Recets (aka App Overview) */
    public void onLaunchFromRecents() {
        mPerLaunchState.mLaunchedFromRecents = true;
    }

    @VisibleForTesting
    protected boolean isDisplayOff(Activity activity) {
        final Display display = DisplayAndroidManager.getDefaultDisplayForContext(activity);
        return display.getState() != Display.STATE_ON;
    }

    @VisibleForTesting
    public static void resetForTests() {
        ThreadUtils.assertOnUiThread();
        sRecordedLaunchCause = false;
    }

    @CheckDiscard("")
    private static void logLaunchCause(@LaunchCause int cause) {
        String launchCause = "";
        switch (cause) {
            case LaunchCause.OTHER:
                launchCause = "OTHER";
                break;
            case LaunchCause.CUSTOM_TAB:
                launchCause = "CUSTOM_TAB";
                break;
            case LaunchCause.TWA:
                launchCause = "TWA";
                break;
            case LaunchCause.RECENTS:
                launchCause = "RECENTS";
                break;
            case LaunchCause.RECENTS_OR_BACK:
                launchCause = "RECENTS_OR_BACK";
                break;
            case LaunchCause.FOREGROUND_WHEN_LOCKED:
                launchCause = "FOREGROUND_WHEN_LOCKED";
                break;
            case LaunchCause.MAIN_LAUNCHER_ICON:
                launchCause = "MAIN_LAUNCHER_ICON";
                break;
        }
        Log.d(TAG, "Launch Cause: " + launchCause);
    }
}

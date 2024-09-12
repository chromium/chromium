// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.metrics;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.view.Display;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.CheckDiscard;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.ui.display.DisplayAndroidManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Computes and records metrics for what caused Chrome to be launched. */
public abstract class LaunchCauseMetrics
        implements ApplicationStatus.ApplicationStateListener,
                ApplicationStatus.ActivityStateListener {
    private static final boolean DEBUG = false;
    private static final String TAG = "LaunchCauseMetrics";

    // Static to avoid recording launch metrics when transitioning between Activities without
    // Chrome leaving the foreground.
    private static boolean sRecordedLaunchCause;

    @VisibleForTesting
    public static final String LAUNCH_CAUSE_HISTOGRAM = "MobileStartup.LaunchCause";

    private PerLaunchState mPerLaunchState = new PerLaunchState();
    private BetweenLaunchState mBetweenLaunchState = new BetweenLaunchState();
    private final Activity mActivity;
    private long mActivityId;

    @SuppressLint("StaticFieldLeak")
    private static Activity sLastResumedActivity;

    private static ApplicationStatus.ActivityStateListener sAppActivityListener;

    static {
        doStaticInit();
    }

    private static void doStaticInit() {
        sAppActivityListener =
                new ApplicationStatus.ActivityStateListener() {
                    @Override
                    public void onActivityStateChange(Activity activity, int newState) {
                        if (newState == ActivityState.RESUMED) sLastResumedActivity = activity;
                        if (newState == ActivityState.DESTROYED) {
                            if (activity == sLastResumedActivity) sLastResumedActivity = null;
                        }
                    }
                };
        ApplicationStatus.registerStateListenerForAllActivities(sAppActivityListener);
        if (ApplicationStatus.getStateForApplication() == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            sLastResumedActivity = ApplicationStatus.getLastTrackedFocusedActivity();
        }
    }

    // State pertaining to the current launch, reset when Chrome is backgrounded,
    // and after computing LaunchCause.
    private static class PerLaunchState {
        boolean mReceivedIntent;
        // Whether a ChromeActivity other than |mActivity| was last focused, used to track
        // intentional transitions between different types of ChromeActivity.
        boolean mOtherChromeActivityLastFocused;
        boolean mLaunchedFromRecents;
    }

    // State that persists through Chrome being backgrounded (but not destroyed), reset after
    // computing LaunchCause.
    private static class BetweenLaunchState {
        boolean mReceivedLeaveHint;
        boolean mScreenOffWhenPaused;
    }

    // These values are persisted in histograms. Please do not renumber. Append only.
    // These values are also recorded in chrome_track_event.proto in Startup.LaunchCauseType.
    // Keep values in sync between the two files.
    @IntDef({
        LaunchCause.OTHER,
        LaunchCause.CUSTOM_TAB,
        LaunchCause.TWA,
        LaunchCause.RECENTS,
        LaunchCause.RECENTS_OR_BACK,
        LaunchCause.FOREGROUND_WHEN_LOCKED,
        LaunchCause.MAIN_LAUNCHER_ICON,
        LaunchCause.MAIN_LAUNCHER_ICON_SHORTCUT,
        LaunchCause.HOME_SCREEN_WIDGET,
        LaunchCause.OPEN_IN_BROWSER_FROM_MENU,
        LaunchCause.EXTERNAL_SEARCH_ACTION_INTENT,
        LaunchCause.NOTIFICATION,
        LaunchCause.EXTERNAL_VIEW_INTENT,
        LaunchCause.OTHER_CHROME,
        LaunchCause.WEBAPK_CHROME_DISTRIBUTOR,
        LaunchCause.WEBAPK_OTHER_DISTRIBUTOR,
        LaunchCause.HOME_SCREEN_SHORTCUT,
        LaunchCause.SHARE_INTENT,
        LaunchCause.NFC,
        LaunchCause.AUTH_TAB,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface LaunchCause {
        int OTHER = 0;
        int CUSTOM_TAB = 1;
        int TWA = 2;
        int RECENTS = 3;
        int RECENTS_OR_BACK = 4;
        int FOREGROUND_WHEN_LOCKED = 5;
        int MAIN_LAUNCHER_ICON = 6;
        int MAIN_LAUNCHER_ICON_SHORTCUT = 7;
        int HOME_SCREEN_WIDGET = 8;
        int OPEN_IN_BROWSER_FROM_MENU = 9;
        int EXTERNAL_SEARCH_ACTION_INTENT = 10;
        int NOTIFICATION = 11;
        int EXTERNAL_VIEW_INTENT = 12;
        int OTHER_CHROME = 13;
        int WEBAPK_CHROME_DISTRIBUTOR = 14;
        int WEBAPK_OTHER_DISTRIBUTOR = 15;
        int HOME_SCREEN_SHORTCUT = 16;
        int SHARE_INTENT = 17;
        int NFC = 18;
        int AUTH_TAB = 19;

        int NUM_ENTRIES = 20;
    }

    /**
     * @param activity The Activity context to compute LaunchCause for, used for getting the correct
     *     Display, etc.
     */
    public LaunchCauseMetrics(final Activity activity) {
        mActivity = activity;
        ApplicationStatus.registerApplicationStateListener(this);
        ApplicationStatus.registerStateListenerForActivity(this, activity);
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        assert activity == mActivity;
        if (newState == ActivityState.DESTROYED) {
            ApplicationStatus.unregisterApplicationStateListener(this);
            ApplicationStatus.unregisterActivityStateListener(this);
        }
        if (newState == ActivityState.PAUSED) {
            mBetweenLaunchState.mScreenOffWhenPaused = isDisplayOff(mActivity);
        }
    }

    @Override
    public void onApplicationStateChange(@ApplicationState int newState) {
        if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES) {
            sRecordedLaunchCause = false;
            resetPerLaunchState();
        }
    }

    private void resetPerLaunchState() {
        mPerLaunchState = new PerLaunchState();
    }

    private void resetBetweenLaunchState() {
        mBetweenLaunchState = new BetweenLaunchState();
    }

    /** Computes and returns what the cause of the Chrome launch was. */
    protected abstract @LaunchCause int computeIntentLaunchCause();

    /**
     * Computes and returns the cause of an Intentional transition between Chrome Activity
     * types, or other if the transition wasn't Intentional.
     *
     * Intentional here means that the user knew they were transitioning between Chrome Activities,
     * and made an explicit choice to do so.
     */
    protected @LaunchCause int getIntentionalTransitionCauseOrOther() {
        return LaunchCause.OTHER;
    }

    /** Returns true if an intent has been received since the last launch of Chrome. */
    protected boolean didReceiveIntent() {
        return mPerLaunchState.mReceivedIntent;
    }

    public void setActivityId(long activityId) {
        mActivityId = activityId;
    }

    /**
     * Called after Chrome has launched and all information necessary to compute why Chrome was
     * launched is available.
     *
     * <p>Records UMA metrics for what caused Chrome to launch, and returns the launch cause.
     */
    public @LaunchCause int recordLaunchCause() {
        @LaunchCause int launchCause = LaunchCause.OTHER;
        if (!sRecordedLaunchCause) {
            sRecordedLaunchCause = true;

            if (mPerLaunchState.mReceivedIntent) {
                launchCause = computeIntentLaunchCause();
            } else {
                launchCause = computeNonIntentLaunchCause();
            }

            if (DEBUG) logLaunchCause(launchCause);

            RecordHistogram.recordEnumeratedHistogram(
                    LAUNCH_CAUSE_HISTOGRAM, launchCause, LaunchCause.NUM_ENTRIES);
            TraceEvent.startupLaunchCause(mActivityId, launchCause);
        } else if (mPerLaunchState.mOtherChromeActivityLastFocused) {
            // Handle the case where we're intentionally transitioning between two Chrome
            // Activities while Chrome is in the foreground, and want to count that as a Launch.
            launchCause = getIntentionalTransitionCauseOrOther();
            if (launchCause != LaunchCause.OTHER) {
                if (DEBUG) logLaunchCause(launchCause);
                RecordHistogram.recordEnumeratedHistogram(
                        LAUNCH_CAUSE_HISTOGRAM, launchCause, LaunchCause.NUM_ENTRIES);
                TraceEvent.startupLaunchCause(mActivityId, launchCause);
            }
        }
        resetPerLaunchState();
        resetBetweenLaunchState();
        return launchCause;
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
        mPerLaunchState.mOtherChromeActivityLastFocused =
                sLastResumedActivity != mActivity && sLastResumedActivity instanceof ChromeActivity;
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

    public static void resetForTests() {
        ThreadUtils.assertOnUiThread();
        sRecordedLaunchCause = false;
        if (sAppActivityListener != null) {
            ApplicationStatus.unregisterActivityStateListener(sAppActivityListener);
            sAppActivityListener = null;
        }
        sLastResumedActivity = null;
        doStaticInit();
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
            case LaunchCause.MAIN_LAUNCHER_ICON_SHORTCUT:
                launchCause = "MAIN_LAUNCHER_ICON_SHORTCUT";
                break;
            case LaunchCause.HOME_SCREEN_WIDGET:
                launchCause = "HOME_SCREEN_WIDGET";
                break;
            case LaunchCause.OPEN_IN_BROWSER_FROM_MENU:
                launchCause = "OPEN_IN_BROWSER_FROM_MENU";
                break;
            case LaunchCause.EXTERNAL_SEARCH_ACTION_INTENT:
                launchCause = "EXTERNAL_SEARCH_ACTION_INTENT";
                break;
            case LaunchCause.NOTIFICATION:
                launchCause = "NOTIFICATION";
                break;
            case LaunchCause.EXTERNAL_VIEW_INTENT:
                launchCause = "EXTERNAL_VIEW_INTENT";
                break;
            case LaunchCause.OTHER_CHROME:
                launchCause = "OTHER_CHROME";
                break;
            case LaunchCause.WEBAPK_CHROME_DISTRIBUTOR:
                launchCause = "WEBAPK_CHROME_DISTRIBUTOR";
                break;
            case LaunchCause.WEBAPK_OTHER_DISTRIBUTOR:
                launchCause = "WEBAPK_OTHER_DISTRIBUTOR";
                break;
            case LaunchCause.HOME_SCREEN_SHORTCUT:
                launchCause = "HOME_SCREEN_SHORTCUT";
                break;
        }
        Log.d(TAG, "Launch Cause: " + launchCause);
    }
}

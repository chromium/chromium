// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Handler;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/**
 * Records the behavior metrics after an ACTION_MAIN intent is received.
 */
public class MainIntentBehaviorMetrics implements ApplicationStatus.ActivityStateListener {
    static final long TIMEOUT_DURATION_MS = 10000;

    @IntDef({MainIntentActionType.CONTINUATION, MainIntentActionType.FOCUS_OMNIBOX,
            MainIntentActionType.SWITCH_TABS, MainIntentActionType.NTP_CREATED,
            MainIntentActionType.BACKGROUNDED})
    @Retention(RetentionPolicy.SOURCE)
    @interface MainIntentActionType {
        int CONTINUATION = 0;
        int FOCUS_OMNIBOX = 1;
        int SWITCH_TABS = 2;
        int NTP_CREATED = 3;
        int BACKGROUNDED = 4;
    }

    // Min and max values (in minutes) for the buckets in the duration histograms.
    private static final int DURATION_HISTOGRAM_MIN = 5;
    private static final int DURATION_HISTOGRAM_MAX = 48 * 60;
    private static final int DURATION_HISTOGRAM_BUCKET_COUNT = 50;

    private static long sTimeoutDurationMs = TIMEOUT_DURATION_MS;
    private static boolean sShouldTrackBehaviorSource;
    private static boolean sLoggedLaunchBehavior;
    static {
        ApplicationStatus.registerApplicationStateListener(newState -> {
            if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES) {
                sLoggedLaunchBehavior = false;
            }
        });
    }

    @VisibleForTesting
    static final String LAUNCH_TIMESTAMP_PREF = "MainIntent.LaunchTimestamp";
    @VisibleForTesting
    static final String LAUNCH_COUNT_PREF = "MainIntent.LaunchCount";

    // Constants used to log UMA "enum" histogram about launch type.
    private static final int LAUNCH_FROM_ICON = 0;
    private static final int LAUNCH_NOT_FROM_ICON = 1;
    private static final int LAUNCH_BOUNDARY = 2;

    private final ChromeActivity mActivity;
    private final Handler mHandler;
    private final Runnable mTimeoutRunnable;
    private final Runnable mLogLaunchRunnable;

    private boolean mPendingActionRecordForMainIntent;
    private long mBackgroundDurationMs;
    private TabModelSelectorTabModelObserver mTabModelObserver;
    private boolean mIgnoreEvents;

    @MainIntentActionType
    private Integer mLastMainIntentBehavior;
    private StackTraceElement[] mMainIntentBehaviorSource;

    /**
     * Constructs a metrics handler for ACTION_MAIN intents received for the specified activity.
     */
    public MainIntentBehaviorMetrics(ChromeActivity activity) {
        mActivity = activity;
        mHandler = new Handler();
        mTimeoutRunnable = new Runnable() {
            @Override
            public void run() {
                recordUserBehavior(MainIntentActionType.CONTINUATION);
            }
        };
        mLogLaunchRunnable = () -> logLaunchBehavior(false);
    }

    /**
     * Signal that an intent with ACTION_MAIN was received.
     *
     * This must only be called after the native libraries have been initialized.
     */
    public void onMainIntentWithNative(long backgroundDurationMs) {
        mLastMainIntentBehavior = null;

        RecordUserAction.record("MobileStartup.MainIntentReceived");

        if (backgroundDurationMs >= DateUtils.HOUR_IN_MILLIS * 24) {
            RecordUserAction.record("MobileStartup.MainIntentReceived.After24Hours");
        } else if (backgroundDurationMs >= DateUtils.HOUR_IN_MILLIS * 12) {
            RecordUserAction.record("MobileStartup.MainIntentReceived.After12Hours");
        } else if (backgroundDurationMs >= DateUtils.HOUR_IN_MILLIS * 6) {
            RecordUserAction.record("MobileStartup.MainIntentReceived.After6Hours");
        } else if (backgroundDurationMs >= DateUtils.HOUR_IN_MILLIS) {
            RecordUserAction.record("MobileStartup.MainIntentReceived.After1Hour");
        }

        if (mPendingActionRecordForMainIntent) return;
        mBackgroundDurationMs = backgroundDurationMs;

        ApplicationStatus.registerStateListenerForActivity(this, mActivity);
        mPendingActionRecordForMainIntent = true;

        mHandler.postDelayed(mTimeoutRunnable, sTimeoutDurationMs);

        mTabModelObserver = new TabModelSelectorTabModelObserver(mActivity.getTabModelSelector()) {
            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type) {
                if (type == TabLaunchType.FROM_RESTORE) return;
                if (NewTabPage.isNTPUrl(tab.getUrl())) {
                    recordUserBehavior(MainIntentActionType.NTP_CREATED);
                }
            }

            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                recordUserBehavior(MainIntentActionType.SWITCH_TABS);
            }
        };

        logLaunchBehavior(true);
    }

    /**
     * Signal that any events should be ignored as a signal of MAIN intent behavior.
     *
     * @param shouldIgnore Whether events should be ignored.
     */
    public void setIgnoreEvents(boolean shouldIgnore) {
        mIgnoreEvents = shouldIgnore;
    }

    /**
     * Signal that the omnibox received focus.
     */
    public void onOmniboxFocused() {
        recordUserBehavior(MainIntentActionType.FOCUS_OMNIBOX);
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.STOPPED || newState == ActivityState.DESTROYED) {
            recordUserBehavior(MainIntentActionType.BACKGROUNDED);
        }
    }

    /**
     * @return The last main intent behavior recorded, which can be null if no MAIN intent has been
     *         received or if the event has not yet occurred.
     */
    @MainIntentActionType
    public Integer getLastMainIntentBehaviorForTesting() {
        return mLastMainIntentBehavior;
    }

    /**
     * @return The stack trace that triggered the last logged main intent.
     */
    @Nullable
    public StackTraceElement[] getMainIntentBehaviorSourceForTesting() {
        assert sShouldTrackBehaviorSource;
        return mMainIntentBehaviorSource;
    }

    /**
     * Allows test to override the timeout duration.
     */
    public static void setTimeoutDurationMsForTesting(long duration) {
        sTimeoutDurationMs = duration;
    }

    /**
     * Specifies whether to track the source stack frame for main intent behavior.
     * @param shouldTrack Whether to track the trigger source of the main intent behavior.
     */
    public static void setShouldTrackBehaviorSourceForTesting(boolean shouldTrack) {
        sShouldTrackBehaviorSource = shouldTrack;
    }

    /**
     * @return Whether we are pending action for a received main intent.
     */
    public boolean getPendingActionRecordForMainIntent() {
        return mPendingActionRecordForMainIntent;
    }

    /**
     * Log how many times user intentionally (from launcher or recents) launch Chrome per day,
     * and the type of each launch.
     */
    public void logLaunchBehavior() {
        if (sLoggedLaunchBehavior) return;
        ThreadUtils.getUiThreadHandler().postDelayed(mLogLaunchRunnable, sTimeoutDurationMs);
    }

    private String getHistogramNameForBehavior(@MainIntentActionType int behavior) {
        switch (behavior) {
            case MainIntentActionType.CONTINUATION:
                return "FirstUserAction.BackgroundTime.MainIntent.Continuation";
            case MainIntentActionType.FOCUS_OMNIBOX:
                return "FirstUserAction.BackgroundTime.MainIntent.Omnibox";
            case MainIntentActionType.SWITCH_TABS:
                return "FirstUserAction.BackgroundTime.MainIntent.SwitchTabs";
            case MainIntentActionType.NTP_CREATED:
                return "FirstUserAction.BackgroundTime.MainIntent.NtpCreated";
            case MainIntentActionType.BACKGROUNDED:
                return "FirstUserAction.BackgroundTime.MainIntent.Backgrounded";
            default:
                return null;
        }
    }

    private void recordUserBehavior(@MainIntentActionType int behavior) {
        if (!mPendingActionRecordForMainIntent || mIgnoreEvents) return;
        mPendingActionRecordForMainIntent = false;

        if (sShouldTrackBehaviorSource) {
            mMainIntentBehaviorSource = Thread.currentThread().getStackTrace();
        }
        mLastMainIntentBehavior = behavior;
        String histogramName = getHistogramNameForBehavior(behavior);
        if (histogramName != null) {
            RecordHistogram.recordCustomCountHistogram(histogramName,
                    (int) (mBackgroundDurationMs / DateUtils.MINUTE_IN_MILLIS),
                    DURATION_HISTOGRAM_MIN, DURATION_HISTOGRAM_MAX,
                    DURATION_HISTOGRAM_BUCKET_COUNT);
        } else {
            assert false : String.format(Locale.getDefault(), "Invalid behavior: %d", behavior);
        }

        ApplicationStatus.unregisterActivityStateListener(this);

        mHandler.removeCallbacksAndMessages(null);

        mTabModelObserver.destroy();
        mTabModelObserver = null;
    }

    private void logLaunchBehavior(boolean isLaunchFromIcon) {
        if (sLoggedLaunchBehavior) return;
        sLoggedLaunchBehavior = true;

        SharedPreferences pref = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = pref.edit();
        long current = System.currentTimeMillis();
        long timestamp = pref.getLong(LAUNCH_TIMESTAMP_PREF, 0);
        int count = pref.getInt(LAUNCH_COUNT_PREF, 0);

        if (current - timestamp > DateUtils.DAY_IN_MILLIS) {
            // Log count if it's not first launch of Chrome.
            if (timestamp != 0) {
                RecordHistogram.recordCountHistogram("MobileStartup.DailyLaunchCount", count);
            }
            count = 0;
            editor.putLong(LAUNCH_TIMESTAMP_PREF, current);
        }

        count++;
        editor.putInt(LAUNCH_COUNT_PREF, count).apply();
        RecordHistogram.recordEnumeratedHistogram("MobileStartup.LaunchType",
                isLaunchFromIcon ? LAUNCH_FROM_ICON : LAUNCH_NOT_FROM_ICON, LAUNCH_BOUNDARY);

        ThreadUtils.getUiThreadHandler().removeCallbacks(mLogLaunchRunnable);
    }
}

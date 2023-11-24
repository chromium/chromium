// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.text.format.DateUtils;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;

/** Records the behavior metrics after an ACTION_MAIN intent is received. */
public class MainIntentBehaviorMetrics {
    static final long TIMEOUT_DURATION_MS = 10000;

    private static long sTimeoutDurationMs = TIMEOUT_DURATION_MS;
    private static boolean sLoggedLaunchBehavior;
    private static boolean sHasRegisteredApplicationStateListener;

    private final Runnable mLogLaunchRunnable;

    /** Constructs a metrics handler for ACTION_MAIN intents received for an activity. */
    public MainIntentBehaviorMetrics() {
        mLogLaunchRunnable = () -> logLaunchBehaviorInternal();
    }

    private void ensureApplicationStateListenerRegistered() {
        if (sHasRegisteredApplicationStateListener) return;
        sHasRegisteredApplicationStateListener = true;
        ApplicationStatus.registerApplicationStateListener(
                newState -> {
                    if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES) {
                        sLoggedLaunchBehavior = false;
                    }
                });
    }

    /**
     * Signal that an intent with ACTION_MAIN was received.
     *
     * This must only be called after the native libraries have been initialized.
     */
    public void onMainIntentWithNative(long backgroundDurationMs) {
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

        logLaunchBehaviorInternal();
    }

    /** Allows test to override the timeout duration. */
    public static void setTimeoutDurationMsForTesting(long duration) {
        var oldValue = sTimeoutDurationMs;
        sTimeoutDurationMs = duration;
        ResettersForTesting.register(() -> sTimeoutDurationMs = oldValue);
    }

    /**
     * Log how many times user intentionally (from launcher or recents) launch Chrome per day,
     * and the type of each launch.
     */
    public void logLaunchBehavior() {
        ensureApplicationStateListenerRegistered();
        if (sLoggedLaunchBehavior) return;
        ThreadUtils.getUiThreadHandler().postDelayed(mLogLaunchRunnable, sTimeoutDurationMs);
    }

    private void logLaunchBehaviorInternal() {
        ensureApplicationStateListenerRegistered();
        if (sLoggedLaunchBehavior) return;
        sLoggedLaunchBehavior = true;

        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        long current = System.currentTimeMillis();
        long timestamp =
                prefs.readLong(ChromePreferenceKeys.METRICS_MAIN_INTENT_LAUNCH_TIMESTAMP, 0);
        int count = prefs.readInt(ChromePreferenceKeys.METRICS_MAIN_INTENT_LAUNCH_COUNT, 0);

        if (current - timestamp > DateUtils.DAY_IN_MILLIS) {
            // Log count if it's not first launch of Chrome.
            if (timestamp != 0) {
                RecordHistogram.recordCount1MHistogram("MobileStartup.DailyLaunchCount", count);
            }
            count = 0;
            prefs.writeLong(ChromePreferenceKeys.METRICS_MAIN_INTENT_LAUNCH_TIMESTAMP, current);
        }

        count++;
        prefs.writeInt(ChromePreferenceKeys.METRICS_MAIN_INTENT_LAUNCH_COUNT, count);

        DefaultBrowserPromoUtils.incrementSessionCount();

        ThreadUtils.getUiThreadHandler().removeCallbacks(mLogLaunchRunnable);
    }
}

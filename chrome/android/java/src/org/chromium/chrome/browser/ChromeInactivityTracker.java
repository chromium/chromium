// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/**
 * Manages pref that can track the delay since the last stop of the tracked activity.
 * TODO(crbug.com/40691343): Split ChromeInactivityTracker out from ChromeTabbedActivity.
 */
public class ChromeInactivityTracker
        implements StartStopWithNativeObserver, PauseResumeWithNativeObserver, DestroyObserver {
    private static final String TAG = "InactivityTracker";
    private static final String UMA_DURATION_SINCE_LAST_BACKGROUND_TIME =
            "Startup.Android.DurationSinceLastBackgroundTime";
    private static final String UMA_IS_LAST_BACKGROUND_TIME_LOGGED =
            "Startup.Android.IsLastBackgroundTimeLogged";

    @VisibleForTesting
    public static final String UMA_IS_LAST_VISIBLE_TIME_LOGGED =
            "Startup.Android.IsLastVisibleTimeLogged";

    private static final long UNKNOWN_LAST_BACKGROUNDED_TIME = -1;

    private final String mPrefName;
    private ActivityLifecycleDispatcher mLifecycleDispatcher;

    /**
     * Creates an inactivity tracker without a timeout callback. This is useful if clients only
     * want to query the inactivity state manually.
     * @param prefName the location in shared preferences that the timestamp is stored.
     */
    public ChromeInactivityTracker(String prefName) {
        mPrefName = prefName;
    }

    /**
     * Registers to the given lifecycle dispatcher.
     * @param lifecycleDispatcher tracks the lifecycle of the Activity of interest, and calls
     *     observer methods on ChromeInactivityTracker.
     */
    public void register(ActivityLifecycleDispatcher lifecycleDispatcher) {
        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
    }

    /**
     * Updates the shared preferences to contain the given time. Used internally and for tests.
     * @param timeInMillis the time to record.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void setLastBackgroundedTimeInPrefs(long timeInMillis) {
        ChromeSharedPreferences.getInstance().writeLong(mPrefName, timeInMillis);
    }

    /**
     * @return The last backgrounded time in millis.
     */
    public long getLastBackgroundedTimeMs() {
        return ChromeSharedPreferences.getInstance()
                .readLong(mPrefName, UNKNOWN_LAST_BACKGROUNDED_TIME);
    }

    /**
     * @return the time interval in millis since the last backgrounded time.
     */
    public long getTimeSinceLastBackgroundedMs() {
        long lastBackgroundedTimeMs = getLastBackgroundedTimeMs();
        if (lastBackgroundedTimeMs == UNKNOWN_LAST_BACKGROUNDED_TIME) {
            return UNKNOWN_LAST_BACKGROUNDED_TIME;
        }
        return System.currentTimeMillis() - lastBackgroundedTimeMs;
    }

    /**
     * Sets the last time when Chrome was launching or moving to foreground and became visible to
     * users and record histogram.
     */
    public void setLastVisibleTimeMsAndRecord(long timeInMillis) {
        // We log the last visible time here to prevent losing the time stamp during the shutdown.
        long lastVisibleTime = getLastVisibleTimeMs();
        ChromeSharedPreferences.getInstance()
                .writeLong(ChromePreferenceKeys.TABBED_ACTIVITY_LAST_VISIBLE_TIME_MS, timeInMillis);

        Log.i(TAG, "Last visible time read from the SharedPreference is:" + lastVisibleTime + ".");
        RecordHistogram.recordBooleanHistogram(
                UMA_IS_LAST_VISIBLE_TIME_LOGGED, lastVisibleTime != UNKNOWN_LAST_BACKGROUNDED_TIME);
    }

    /**
     * Gets the last time when Chrome was launching or moving to foreground and became visible to
     * users.
     */
    public long getLastVisibleTimeMs() {
        return ChromeSharedPreferences.getInstance()
                .readLong(
                        ChromePreferenceKeys.TABBED_ACTIVITY_LAST_VISIBLE_TIME_MS,
                        UNKNOWN_LAST_BACKGROUNDED_TIME);
    }

    @Override
    public void onStartWithNative() {}

    @Override
    public void onResumeWithNative() {
        // We clear the backgrounded time here, rather than in #onStartWithNative, to give
        // handlers the chance to respond to inactivity during any onStartWithNative handler
        // regardless of ordering. onResume is always called after onStart, and it should be fine to
        // consider Chrome active if it reaches onResume.
        long lastBackgroundTime =
                ChromeSharedPreferences.getInstance()
                        .readLong(mPrefName, UNKNOWN_LAST_BACKGROUNDED_TIME);
        setLastBackgroundedTimeInPrefs(UNKNOWN_LAST_BACKGROUNDED_TIME);

        Log.i(
                TAG,
                "Last background time read from the SharedPreference is:"
                        + lastBackgroundTime
                        + ".");
        RecordHistogram.recordBooleanHistogram(
                UMA_IS_LAST_BACKGROUND_TIME_LOGGED,
                lastBackgroundTime != UNKNOWN_LAST_BACKGROUNDED_TIME);

        if (lastBackgroundTime != UNKNOWN_LAST_BACKGROUNDED_TIME) {
            RecordHistogram.recordLongTimesHistogram100(
                    UMA_DURATION_SINCE_LAST_BACKGROUND_TIME,
                    System.currentTimeMillis() - lastBackgroundTime);
        }
    }

    @Override
    public void onPauseWithNative() {}

    @Override
    public void onStopWithNative() {}

    @Override
    public void onDestroy() {
        mLifecycleDispatcher.unregister(this);
    }
}

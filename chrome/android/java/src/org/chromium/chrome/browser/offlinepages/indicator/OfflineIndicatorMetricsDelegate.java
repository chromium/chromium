// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Tracks metrics about how long the Offline Indicator is shown. All state related to the metrics
 * are persisted to prefs, so if Chrome is ever killed while the Offline Indicator is shown, we can
 * continue tracking accurate metrics the next time Chrome is started up.
 */
public class OfflineIndicatorMetricsDelegate {
    /** Clock to use so we can mock the time in tests. */
    public interface Clock {
        long currentTimeMillis();
    }
    private static Clock sClock = System::currentTimeMillis;

    // UMA Histograms.
    public static final String OFFLINE_INDICATOR_SHOWN_DURATION_V2 =
            "OfflineIndicator.ShownDurationV2";

    /** Whether or not we are tracking a shown duration of the offline indicator. */
    private boolean mIsTrackingShownDuration;

    /**
     * The wall time in milliseconds of the most recent time the offline indicator began being
     * shown. This value is persisted in prefs if |mIsTrackingShownDuration| is true.
     */
    private long mIndicatorShownWallTimeMs;

    public OfflineIndicatorMetricsDelegate() {
        // Read stored state from Prefs
        if (SharedPreferencesManager.getInstance().contains(
                    ChromePreferenceKeys.OFFLINE_INDICATOR_V2_WALL_TIME_SHOWN_MS)) {
            mIndicatorShownWallTimeMs = SharedPreferencesManager.getInstance().readLong(
                    ChromePreferenceKeys.OFFLINE_INDICATOR_V2_WALL_TIME_SHOWN_MS);
            mIsTrackingShownDuration = true;
        }
    }

    /**
     * Returns whether we are currently tracking a shown duration of the offline indicator or not.
     */
    public boolean isTrackingShownDuration() {
        return mIsTrackingShownDuration;
    }

    /**
     * When we are tracking a persisted shown duration and the offline state is initialized to
     * online, then we treat this as if the offline indicator was just hidden.
     * @param isOffline The offline state at initialization.
     */
    public void onOfflineStateInitialized(boolean isOffline) {
        if (mIsTrackingShownDuration && !isOffline) {
            onIndicatorHidden();
        }
    }

    /**
     * When the Offline Indicator is shown, then we begin tracking a shown duration if not already
     * tracking one. This function can be called when already tracking a shown duration, if there
     * was state stored in Prefs when Chrome started up.
     */
    public void onIndicatorShown() {
        if (mIsTrackingShownDuration) return;

        mIsTrackingShownDuration = true;
        mIndicatorShownWallTimeMs = sClock.currentTimeMillis();
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.OFFLINE_INDICATOR_V2_WALL_TIME_SHOWN_MS,
                mIndicatorShownWallTimeMs);
    }

    /**
     * When the Offline Indicator is hidden, then we record the shown duration. If the indicator is
     * not shown when Chrome starts up and it was shown the last time Chrome was running in the
     * foreground, then we count the indicator as being hidden.
     */
    public void onIndicatorHidden() {
        if (!mIsTrackingShownDuration) return;

        final long shownDurationWallTimeMs = sClock.currentTimeMillis() - mIndicatorShownWallTimeMs;

        // shownDurationWallTimeMs can be negative in cases where the system time is changed, so
        // we want to avoid recording metrics in cases where we know this happened.
        if (shownDurationWallTimeMs >= 0) {
            recordShownDurationV2Histogram(shownDurationWallTimeMs);
        }

        clearStateFromPrefs();
        mIsTrackingShownDuration = false;
    }

    /**
     * When the application is foregrounded, we update the state used to track how long the
     * indicator is shown while in the foreground or in the background.
     */
    public void onAppForegrounded() {
        // TODO(curranmax) Track the amount of time that the application is foreground or background
        // and the offline indicator is shown.
    }

    /**
     * When the application is backgrounded, we update the state used to track how long the
     * indicator is shown while in the foreground or in the background.
     */
    public void onAppBackgrounded() {
        // TOOD(curranmax) Same as |onAppForegrounded|.
    }

    private void recordShownDurationV2Histogram(long shownDurationMs) {
        RecordHistogram.recordLongTimesHistogram100(
                OFFLINE_INDICATOR_SHOWN_DURATION_V2, shownDurationMs);
    }

    private void clearStateFromPrefs() {
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.OFFLINE_INDICATOR_V2_WALL_TIME_SHOWN_MS);
    }

    @VisibleForTesting
    static void setClockForTesting(Clock clock) {
        sClock = clock;
    }
}

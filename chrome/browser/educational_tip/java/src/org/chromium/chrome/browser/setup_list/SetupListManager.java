// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.setup_list;

import static org.chromium.chrome.browser.firstrun.FirstRunStatus.isFirstRunTriggered;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.concurrent.TimeUnit;

/**
 * Manages the state and logic for the Setup List feature. This class is responsible for determining
 * whether the Setup List is active and caching this state for the duration of the app session.
 */
@NullMarked
public class SetupListManager {
    private static class LazyHolder {
        private static final SetupListManager sInstance = new SetupListManager();
    }

    @Nullable private static SetupListManager sInstanceForTesting;

    private final boolean mIsSetupListActive;
    private final boolean mShouldShowTwoCellLayout;

    @VisibleForTesting
    static final long SETUP_LIST_ACTIVE_WINDOW_MILLIS = TimeUnit.DAYS.toMillis(14);

    @VisibleForTesting
    static final long TWO_CELL_LAYOUT_ACTIVE_WINDOW_MILLIS = TimeUnit.DAYS.toMillis(3);

    @VisibleForTesting
    SetupListManager() {
        if (!ChromeFeatureList.sAndroidSetupList.isEnabled() || isFirstRunTriggered()) {
            // Only enabled from the second run onwards.
            mIsSetupListActive = false;
            mShouldShowTwoCellLayout = false;
            return;
        }
        long setupListFirstShownTimestamp =
                ChromeSharedPreferences.getInstance()
                        .readLong(ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP, -1L);

        if (setupListFirstShownTimestamp != -1L) {
            // If the setup list has been shown before, check if it's within the active window.
            long timeSinceFirstStart = TimeUtils.currentTimeMillis() - setupListFirstShownTimestamp;
            mIsSetupListActive = timeSinceFirstStart < SETUP_LIST_ACTIVE_WINDOW_MILLIS;
            mShouldShowTwoCellLayout =
                    mIsSetupListActive
                            && timeSinceFirstStart >= TWO_CELL_LAYOUT_ACTIVE_WINDOW_MILLIS;
        } else {
            // If the timestamp is not set, this is the first time SetupListManager is
            // instantiated after the first run. Mark the list as active and record the
            // current time as the start of the 14-day window.
            mIsSetupListActive = true;
            mShouldShowTwoCellLayout = false;
            ChromeSharedPreferences.getInstance()
                    .writeLong(
                            ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP,
                            TimeUtils.currentTimeMillis());
        }
    }

    /** Returns the singleton instance of SetupListManager. */
    public static SetupListManager getInstance() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return LazyHolder.sInstance;
    }

    /** Returns whether the setup list is active based on the 14-day window. */
    public boolean isSetupListActive() {
        return mIsSetupListActive;
    }

    /** Returns whether the two-cell layout should be shown. This is cached for the session. */
    public boolean shouldShowTwoCellLayout() {
        return mShouldShowTwoCellLayout;
    }

    public static void setInstanceForTesting(@Nullable SetupListManager instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }
}

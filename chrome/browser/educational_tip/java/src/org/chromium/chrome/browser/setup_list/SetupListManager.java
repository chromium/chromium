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
    public static final long SETUP_LIST_ACTIVE_WINDOW_MILLIS = TimeUnit.DAYS.toMillis(14);

    @VisibleForTesting
    SetupListManager() {
        if (!ChromeFeatureList.sAndroidSetupList.isEnabled() || isFirstRunTriggered()) {
            // Only enabled from the second run onwards.
            mIsSetupListActive = false;
            return;
        }
        long firstCtaStartTimestamp =
                ChromeSharedPreferences.getInstance()
                        .readLong(ChromePreferenceKeys.FIRST_CTA_START_TIMESTAMP, -1L);
        if (firstCtaStartTimestamp == -1L) {
            mIsSetupListActive = false;
            return;
        }
        mIsSetupListActive =
                (TimeUtils.currentTimeMillis() - firstCtaStartTimestamp)
                        < SETUP_LIST_ACTIVE_WINDOW_MILLIS;
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

    public static void setInstanceForTesting(@Nullable SetupListManager instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.shared;

import org.chromium.base.Log;
import org.chromium.chrome.browser.feed.FeedV1;
import org.chromium.chrome.browser.feed.FeedV2;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Helper methods covering more complex Feed related feature checks and states.
 */
public final class FeedFeatures {
    private static final String TAG = "FeedFeatures";
    /**
     * Flag that tracks whether we've ever been disabled via enterprise policy. Should only be
     * accessed through isFeedProcessScopeEnabled().
     */
    private static boolean sEverDisabledForPolicy;

    private static PrefChangeRegistrar sPrefChangeRegistrar;

    /**
     * @return Whether implicit Feed user actions are being reported based on feature states. Can be
     *         used for both Feed v1 and v2.
     */
    public static boolean isReportingUserActions() {
        return isV2Enabled()
                || ChromeFeatureList.isEnabled(ChromeFeatureList.REPORT_FEED_USER_ACTIONS);
    }

    /**
     * Identical to {@link isReportingUserActions} but uses {@link CachedFeatureFlags} for checking
     * feature states.
     */
    public static boolean cachedIsReportingUserActions() {
        return cachedIsV2Enabled()
                || CachedFeatureFlags.isEnabled(ChromeFeatureList.REPORT_FEED_USER_ACTIONS);
    }

    public static boolean isV2Enabled() {
        if (!FeedV1.IS_AVAILABLE) return true;
        if (!FeedV2.IS_AVAILABLE) return false;
        return ChromeFeatureList.isEnabled(ChromeFeatureList.INTEREST_FEED_V2);
    }

    public static boolean cachedIsV2Enabled() {
        if (!FeedV1.IS_AVAILABLE) return true;
        if (!FeedV2.IS_AVAILABLE) return false;
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.INTEREST_FEED_V2);
    }

    /**
     * @return Whether the feed is allowed to be used. The feed is disabled if supervised user or
     * enterprise policy has once been added within the current session. The value returned by
     * this function can change from true to false over the life of the application.
     */
    public static boolean isFeedEnabled() {
        // Once true, sEverDisabledForPolicy will remain true. If it isn't true yet, we need to
        // check the pref every time. Two reasons for this. 1) We want to notice when we start in a
        // disabled state, shouldn't allow Feed to enabled until a restart. 2) A different
        // subscriber to this pref change event might check in with this method, and we cannot
        // assume who will be called first. See https://crbug.com/896468.
        if (sEverDisabledForPolicy) return false;

        if (sPrefChangeRegistrar == null) {
            sPrefChangeRegistrar = new PrefChangeRegistrar();
            sPrefChangeRegistrar.addObserver(
                    Pref.ENABLE_SNIPPETS, FeedFeatures::articlesEnabledPrefChange);
        }

        if (!sEverDisabledForPolicy) {
            sEverDisabledForPolicy = !getPrefService().getBoolean(Pref.ENABLE_SNIPPETS);
        }

        return !sEverDisabledForPolicy;
    }

    private static void articlesEnabledPrefChange() {
        // Cannot assume this is called because of an actual change. May be going from true to true.
        if (!getPrefService().getBoolean(Pref.ENABLE_SNIPPETS)) {
            // There have been quite a few crashes/bugs that happen when code does not correctly
            // handle the scenario where Feed suddenly becomes disabled and the above getters start
            // returning nulls. Having this log a warning helps diagnose this pattern from the
            // logcat.
            Log.w(TAG, "Disabling Feed because of policy.");
            sEverDisabledForPolicy = true;
            if (FeedV1.IS_AVAILABLE) {
                FeedV1.destroy();
            }
        }
    }

    private static PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }
}

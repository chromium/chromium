// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.shared;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Helper methods covering more complex Feed related feature checks and states.
 */
public final class FeedFeatures {
    private static final String TAG = "FeedFeatures";
    private static PrefService sFakePrefServiceForTest;

    /**
     * @return Whether the feed is allowed to be used. Returns false if the feed is disabled due to
     *         enterprise policy. The value returned should not be cached as it may change.
     */
    public static boolean isFeedEnabled() {
        return getPrefService().getBoolean(Pref.ENABLE_SNIPPETS);
    }

    /**
     * @return Whether the WebFeed UI should be enabled. Checks for both the WEB_FEED flag and if
     *         the user is signed in.
     */
    public static boolean isWebFeedUIEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_FEED)
                && IdentityServicesProvider.get()
                           .getSigninManager(Profile.getLastUsedRegularProfile())
                           .getIdentityManager()
                           .hasPrimaryAccount();
    }

    /**
     * @return Whether the feed should automatically scroll down when it first loads so that the
     *         first card is at the top of the screen. This is for use with screenshot utilities.
     */
    public static boolean isAutoScrollToTopEnabled() {
        CommandLine commandLine = CommandLine.getInstance();
        if (commandLine == null) return false;
        return commandLine.hasSwitch("feed-screenshot-mode");
    }

    private static PrefService getPrefService() {
        if (sFakePrefServiceForTest != null) {
            return sFakePrefServiceForTest;
        }
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }

    @VisibleForTesting
    public static void setFakePrefsForTest(PrefService fakePref) {
        sFakePrefServiceForTest = fakePref;
    }
}

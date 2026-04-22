// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.base.CommandLine;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator.StreamTabId;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Helper methods covering more complex Feed related feature checks and states. */
@NullMarked
public final class FeedFeatures {

    private static @Nullable PrefService sFakePrefServiceForTest;

    /**
     * @param profile the profile of the current user.
     * @return Whether the feed is allowed to be used. Returns false if the feed is disabled due to
     *     enterprise policy, or by flag. The value returned should not be cached as it may change.
     */
    public static boolean isFeedEnabled(Profile profile) {
        return (!ChromeFeatureList.sNtpSimplification.isEnabled() || !DeviceInfo.isDesktop())
                && FeedServiceBridge.isEnabled()
                && isFeedEnabledByDse(profile);
    }

    private static boolean isFeedEnabledByDse(Profile profile) {
        return getPrefService(profile).getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE);
    }

    /**
     * @return Whether the feed should automatically scroll down when it first loads so that the
     *     first card is at the top of the screen. This is for use with screenshot utilities.
     */
    public static boolean isAutoScrollToTopEnabled() {
        CommandLine commandLine = CommandLine.getInstance();
        if (commandLine == null) return false;
        return commandLine.hasSwitch("feed-screenshot-mode");
    }

    public static PrefService getPrefService(Profile profile) {
        if (sFakePrefServiceForTest != null) {
            return sFakePrefServiceForTest;
        }
        return UserPrefs.get(profile);
    }

    public static void setFakePrefsForTest(PrefService fakePref) {
        sFakePrefServiceForTest = fakePref;
        ResettersForTesting.register(() -> sFakePrefServiceForTest = null);
    }

    /**
     * Returns the feed tab ID to restore depending on the configured logic controlling the
     * "stickiness" of the selected feed tab.
     */
    public static @StreamTabId int getFeedTabIdToRestore(Profile profile) {
        // Default behavior (reset_for_every_new_ntp).
        setLastSeenFeedTabId(profile, StreamTabId.FOR_YOU);
        return StreamTabId.FOR_YOU;
    }

    public static void setLastSeenFeedTabId(Profile profile, @StreamTabId int tabId) {
        // Note: the "first check" flag is updated here to make sure that if setLastSeenFeedTabId is
        // called before getFeedTabIdToRestore, the value set here is taken into account in by the
        // latter at least for some of the restore logic atlernatives.
        getPrefService(profile).setInteger(Pref.LAST_SEEN_FEED_TYPE, tabId);
    }
}

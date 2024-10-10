// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import androidx.annotation.NonNull;

import org.chromium.base.CommandLine;
import org.chromium.base.LocaleUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator.StreamTabId;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.concurrent.TimeUnit;

/** Helper methods covering more complex Feed related feature checks and states. */
public final class FeedFeatures {
    private static final long ONE_DAY_DELTA_MILLIS = TimeUnit.DAYS.toMillis(1L);

    private static PrefService sFakePrefServiceForTest;

    /**
     * @param profile the profile of the current user.
     * @return Whether the feed is allowed to be used. Returns false if the feed is disabled due to
     *     enterprise policy, or by flag. The value returned should not be cached as it may change.
     */
    public static boolean isFeedEnabled(Profile profile) {
        return FeedServiceBridge.isEnabled() && isFeedEnabledByDSE(profile);
    }

    /**
     * @param profile the profile of the current user.
     * @return Whether the WebFeed UI should be enabled. Checks for the WEB_FEED flag, if the user
     *     is signed in and confirms it's not a child profile.
     */
    public static boolean isWebFeedUIEnabled(@NonNull Profile profile) {
        // TODO(b/197354832, b/188188861): change consent check to SIGNIN.
        boolean isPrimaryAccountSignedIn = false;
        if (IdentityServicesProvider.get().getSigninManager(profile) != null) {
            isPrimaryAccountSignedIn =
                    IdentityServicesProvider.get()
                            .getSigninManager(profile)
                            .getIdentityManager()
                            .hasPrimaryAccount(ConsentLevel.SIGNIN);
        }
        return WebFeedBridge.isWebFeedEnabled()
                && isPrimaryAccountSignedIn
                && !profile.isChild()
                && isFeedEnabledByDSE(profile);
    }

    private static boolean isFeedEnabledByDSE(Profile profile) {
        return getPrefService(profile).getBoolean(Pref.ENABLE_SNIPPETS_BY_DSE);
    }

    public static boolean shouldUseWebFeedAwarenessIPH() {
        String awarenessStyleParam =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.WEB_FEED_AWARENESS, "awareness_style");
        return WebFeedBridge.isWebFeedEnabled()
                && (awarenessStyleParam.equals("IPH") || awarenessStyleParam.isEmpty());
    }

    public static boolean shouldUseNewIndicator(Profile profile) {
        // Return true if we are not rate limited.
        if (ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.WEB_FEED_AWARENESS, "awareness_style")
                .equals("new_animation_no_limit")) {
            return true;
        }
        // Otherwise, the rate limit is:
        // 1. We have never seen the web feed.
        // 2. It's been > 1 day since we last seen the new indicator.
        if (ChromeFeatureList.getFieldTrialParamByFeature(
                                ChromeFeatureList.WEB_FEED_AWARENESS, "awareness_style")
                        .equals("new_animation")
                && !getPrefService(profile).getBoolean(Pref.HAS_SEEN_WEB_FEED)) {
            String timestamp = getPrefService(profile).getString(Pref.LAST_BADGE_ANIMATION_TIME);
            long currentTime = System.currentTimeMillis();
            long parsedTime;
            try {
                parsedTime = Long.parseLong(timestamp);
            } catch (NumberFormatException e) {
                parsedTime = 0L;
            }
            // Ignore parsed timestamps in the future.
            return currentTime < parsedTime || currentTime - parsedTime > ONE_DAY_DELTA_MILLIS;
        }
        return false;
    }

    public static boolean isFeedFollowUiUpdateEnabled() {
        if (LocaleUtils.getDefaultCountryCode().equals("US")) {
            return true;
        }
        return ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_FOLLOW_UI_UPDATE);
    }

    /** Updates the timestamp for the last time the new indicator was seen to now. */
    public static void updateNewIndicatorTimestamp(Profile profile) {
        getPrefService(profile)
                .setString(Pref.LAST_BADGE_ANIMATION_TIME, "" + System.currentTimeMillis());
    }

    /** Updates that the following feed has been seen. */
    public static void updateFollowingFeedSeen(Profile profile) {
        getPrefService(profile).setBoolean(Pref.HAS_SEEN_WEB_FEED, true);
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

    private static @StreamTabId int getLastSeenFeedTabId(Profile profile) {
        return getPrefService(profile).getInteger(Pref.LAST_SEEN_FEED_TYPE);
    }
}

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator.StreamTabId;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeUnit;

/**
 * Helper methods covering more complex Feed related feature checks and states.
 */
public final class FeedFeatures {
    // Finch param constants for controlling the feed tab stickiness logic to use.
    private static final String FEED_TAB_STICKYNESS_LOGIC_PARAM = "feed_tab_stickiness_logic";
    private static final String RESET_UPON_CHROME_RESTART = "reset_upon_chrome_restart";
    private static final String INDEFINITELY_PERSISTED = "indefinitely_persisted";
    private static final long ONE_DAY_DELTA_MILLIS = TimeUnit.DAYS.toMillis(1L);

    private static PrefService sFakePrefServiceForTest;
    private static boolean sIsFirstFeedTabStickinessCheckSinceLaunch = true;

    /**
     * @return Whether the feed is allowed to be used. Returns false if the feed is disabled due to
     *         enterprise policy, or by flag. The value returned should not be cached as it may
     * change.
     */
    public static boolean isFeedEnabled() {
        return FeedServiceBridge.isEnabled();
    }

    /**
     * @return Whether the WebFeed UI should be enabled. Checks for the WEB_FEED flag, if
     *         the user is signed in and confirms it's not a child profile.
     */
    public static boolean isWebFeedUIEnabled() {
        // TODO(b/197354832, b/188188861): change consent check to SIGNIN.
        return ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_FEED)
                && IdentityServicesProvider.get()
                           .getSigninManager(Profile.getLastUsedRegularProfile())
                           .getIdentityManager()
                           .hasPrimaryAccount(ConsentLevel.SIGNIN)
                && !Profile.getLastUsedRegularProfile().isChild();
    }

    public static boolean shouldUseWebFeedAwarenessIPH() {
        return ChromeFeatureList
                .getFieldTrialParamByFeature(
                        ChromeFeatureList.WEB_FEED_AWARENESS, "awareness_style")
                .equals("IPH");
    }

    public static boolean shouldUseNewIndicator() {
        // Return true if we are not rate limited.
        if (ChromeFeatureList
                        .getFieldTrialParamByFeature(
                                ChromeFeatureList.WEB_FEED_AWARENESS, "awareness_style")
                        .equals("new_animation_no_limit")) {
            return true;
        }
        // Otherwise, the rate limit is:
        // 1. We have never seen the web feed.
        // 2. It's been > 1 day since we last seen the new indicator.
        if (ChromeFeatureList
                        .getFieldTrialParamByFeature(
                                ChromeFeatureList.WEB_FEED_AWARENESS, "awareness_style")
                        .equals("new_animation")
                && !getPrefService().getBoolean(Pref.HAS_SEEN_WEB_FEED)) {
            String timestamp = getPrefService().getString(Pref.LAST_BADGE_ANIMATION_TIME);
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

    /**
     * Updates the timestamp for the last time the new indicator was seen to now.
     */
    public static void updateNewIndicatorTimestamp() {
        getPrefService().setString(Pref.LAST_BADGE_ANIMATION_TIME, "" + System.currentTimeMillis());
    }

    /**
     * Updates that the following feed has been seen.
     */
    public static void updateFollowingFeedSeen() {
        getPrefService().setBoolean(Pref.HAS_SEEN_WEB_FEED, true);
    }

    public static boolean isMultiColumnFeedEnabled(Context context) {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_MULTI_COLUMN);
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

    public static PrefService getPrefService() {
        if (sFakePrefServiceForTest != null) {
            return sFakePrefServiceForTest;
        }
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }

    @VisibleForTesting
    public static void setFakePrefsForTest(PrefService fakePref) {
        sFakePrefServiceForTest = fakePref;
    }

    /**
     * Returns the feed tab ID to restore depending on the configured logic controlling the
     * "stickiness" of the selected feed tab. These are the available options:
     * - reset_for_every_new_ntp: tab choice is reset for each newly opened NTP (default behavior;
     *   not an actual Finch param).
     * - indefinitely_persisted: tab choice is kept forever.
     * - reset_upon_chrome_restart: tab choice is reset upon Chrome relaunch.
     */
    public static @StreamTabId int getFeedTabIdToRestore() {
        String stickinessLogic = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.WEB_FEED, FEED_TAB_STICKYNESS_LOGIC_PARAM);

        if (RESET_UPON_CHROME_RESTART.equals(stickinessLogic)) {
            if (sIsFirstFeedTabStickinessCheckSinceLaunch) {
                sIsFirstFeedTabStickinessCheckSinceLaunch = false;
                setLastSeenFeedTabId(StreamTabId.FOR_YOU);
                return StreamTabId.FOR_YOU;
            }
            return getLastSeenFeedTabId();
        }
        if (INDEFINITELY_PERSISTED.equals(stickinessLogic)) {
            return getLastSeenFeedTabId();
        }

        // Default behavior (reset_for_every_new_ntp).
        setLastSeenFeedTabId(StreamTabId.FOR_YOU);
        return StreamTabId.FOR_YOU;
    }

    public static void setLastSeenFeedTabId(@StreamTabId int tabId) {
        // Note: the "first check" flag is updated here to make sure that if setLastSeenFeedTabId is
        // called before getFeedTabIdToRestore, the value set here is taken into account in by the
        // latter at least for some of the restore logic atlernatives.
        sIsFirstFeedTabStickinessCheckSinceLaunch = false;
        getPrefService().setInteger(Pref.LAST_SEEN_FEED_TYPE, tabId);
    }

    private static @StreamTabId int getLastSeenFeedTabId() {
        return getPrefService().getInteger(Pref.LAST_SEEN_FEED_TYPE);
    }

    @VisibleForTesting
    static void resetInternalStateForTesting() {
        sIsFirstFeedTabStickinessCheckSinceLaunch = true;
    }
}

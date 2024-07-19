// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.content.Context;
import android.content.Intent;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.PrefNames;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.Set;

/**
 * A utility class for Privacy Guide that fetches the current state of {@link
 * PrivacyGuideFragment.FragmentType}s.
 */
class PrivacyGuideUtils {
    static boolean isMsbbEnabled(Profile profile) {
        return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(profile);
    }

    static boolean isHistorySyncEnabled(Profile profile) {
        Set<Integer> syncTypes = SyncServiceFactory.getForProfile(profile).getSelectedTypes();

        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            return syncTypes.contains(UserSelectableType.HISTORY);
        }
        // The toggle represents both History and Tabs.
        // History and Tabs should usually have the same value, but in some
        // cases they may not, e.g. if one of them is disabled by policy. In that
        // case, show the toggle as on if at least one of them is enabled. The
        // toggle should reflect the value of the non-disabled type.
        return syncTypes.contains(UserSelectableType.HISTORY)
                || syncTypes.contains(UserSelectableType.TABS);
    }

    static boolean isUserSignedIn(Profile profile) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        return identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN);
    }

    static boolean isSearchSuggestionsEnabled(Profile profile) {
        return UserPrefs.get(profile).getBoolean(Pref.SEARCH_SUGGEST_ENABLED);
    }

    static boolean isAdTopicsEnabled(Profile profile) {
        return UserPrefs.get(profile).getBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED);
    }

    static @SafeBrowsingState int getSafeBrowsingState(Profile profile) {
        return new SafeBrowsingBridge(profile).getSafeBrowsingState();
    }

    static @CookieControlsMode int getCookieControlsMode(Profile profile) {
        return UserPrefs.get(profile).getInteger(PrefNames.COOKIE_CONTROLS_MODE);
    }

    /**
     * Functional interface to start a Chrome Custom Tab for the given intent, e.g. by using {@link
     * org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent}.
     * TODO(crbug.com/40751023): Update when LaunchIntentDispatcher is (partially-)modularized.
     */
    public interface CustomTabIntentHelper {
        /**
         * @see org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent
         */
        Intent createCustomTabActivityIntent(Context context, Intent intent);
    }
}

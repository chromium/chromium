// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.content.Context;
import android.content.Intent;

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
    static boolean isMsbbEnabled() {
        return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                Profile.getLastUsedRegularProfile());
    }

    static boolean isHistorySyncEnabled() {
        Set<Integer> syncTypes = SyncServiceFactory.get().getSelectedTypes();
        return syncTypes.contains(UserSelectableType.HISTORY);
    }

    static boolean isUserSignedIn() {
        IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        return identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN);
    }

    static @SafeBrowsingState int getSafeBrowsingState() {
        return SafeBrowsingBridge.getSafeBrowsingState();
    }

    static @CookieControlsMode int getCookieControlsMode() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile())
                .getInteger(PrefNames.COOKIE_CONTROLS_MODE);
    }

    /**
     * Functional interface to start a Chrome Custom Tab for the given intent, e.g. by using
     * {@link org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent}.
     * TODO(crbug.com/1181700): Update when LaunchIntentDispatcher is (partially-)modularized.
     */
    public interface CustomTabIntentHelper {
        /**
         * @see org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent
         */
        Intent createCustomTabActivityIntent(Context context, Intent intent);
    }
}

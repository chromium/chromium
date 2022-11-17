// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.MutableFlagWithSafeDefault;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.user_prefs.UserPrefs;

/** Self-documenting feature class for shopping.  */
public class ShoppingFeatures {
    private static final MutableFlagWithSafeDefault sShoppingListFlag =
            new MutableFlagWithSafeDefault(ChromeFeatureList.SHOPPING_LIST, false);
    /** Returns whether shopping is enabled. */
    public static boolean isShoppingListEnabled() {
        return sShoppingListFlag.isEnabled() && isSignedIn()
                && isAnonymizedUrlDataCollectionEnabled() && isWebAndAppActivityEnabled();
    }

    private static boolean isSignedIn() {
        return IdentityServicesProvider.get()
                .getIdentityManager(Profile.getLastUsedRegularProfile())
                .hasPrimaryAccount(ConsentLevel.SYNC);
    }

    private static boolean isAnonymizedUrlDataCollectionEnabled() {
        return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                Profile.getLastUsedRegularProfile());
    }

    private static boolean isWebAndAppActivityEnabled() {
        PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
        return prefService != null
                && prefService.getBoolean(Pref.WEB_AND_APP_ACTIVITY_ENABLED_FOR_SHOPPING);
    }
}
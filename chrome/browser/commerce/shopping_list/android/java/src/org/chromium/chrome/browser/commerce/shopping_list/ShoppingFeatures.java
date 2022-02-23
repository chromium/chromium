// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce.shopping_list;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.components.signin.identitymanager.ConsentLevel;

/** Self-documenting feature class for shopping.  */
public class ShoppingFeatures {
    /** Returns whether shopping is enabled. */
    public static boolean isShoppingListEnabled() {
        return FeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.SHOPPING_LIST) && isSignedIn()
                && isAnonymizedUrlDataCollectionEnabled();
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
}
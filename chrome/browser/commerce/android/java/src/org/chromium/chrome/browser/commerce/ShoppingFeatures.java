// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.commerce.core.ShoppingService;

/** Self-documenting feature class for shopping. */
public class ShoppingFeatures {
    /** Wrapper function for ShoppingService.isShoppingListEligibile(). */
    public static boolean isShoppingListEligible(Profile profile) {
        if (ShoppingService.isShoppingListEligibleForTesting() != null) {
            return ShoppingService.isShoppingListEligibleForTesting();
        }

        if (profile == null) return false;
        ShoppingService service = ShoppingServiceFactory.getForProfile(profile);
        if (service == null) return false;
        return service.isShoppingListEligible();
    }

    public static void setShoppingListEligibleForTesting(Boolean eligible) {
        ShoppingService.setShoppingListEligibleForTesting(eligible);
    }
}

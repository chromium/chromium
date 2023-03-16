// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/**
 * {@link CommerceSubscriptionsService} cached by {@link Profile}.
 */
public class CommerceSubscriptionsServiceFactory {
    private static ProfileKeyedMap<CommerceSubscriptionsService> sProfileToSubscriptionsService;
    private static CommerceSubscriptionsService sSubscriptionsServiceForTesting;

    /** Creates new instance. */
    public CommerceSubscriptionsServiceFactory() {
        if (sProfileToSubscriptionsService == null) {
            sProfileToSubscriptionsService = ProfileKeyedMap.createMapOfDestroyables();
        }
    }

    /**
     * Creates a new instance or reuses an existing one based on the current {@link Profile}.
     *
     * Note: Don't hold a reference to the returned value. Always use this method to access {@link
     * CommerceSubscriptionsService} instead.
     * @return {@link CommerceSubscriptionsService} instance for the current regular
     *         profile.
     */
    public CommerceSubscriptionsService getForLastUsedProfile() {
        if (sSubscriptionsServiceForTesting != null) return sSubscriptionsServiceForTesting;
        Profile profile = Profile.getLastUsedRegularProfile();
        return sProfileToSubscriptionsService.getForProfile(profile, () -> {
            PriceDropNotificationManager priceDropNotificationManager =
                    PriceDropNotificationManagerFactory.create();
            return new CommerceSubscriptionsService(
                    ShoppingServiceFactory.getForProfile(profile), priceDropNotificationManager);
        });
    }

    /** Sets the CommerceSubscriptionsService for testing. */
    @VisibleForTesting
    public static void setSubscriptionsServiceForTesting(
            CommerceSubscriptionsService subscriptionsService) {
        sSubscriptionsServiceForTesting = subscriptionsService;
    }
}

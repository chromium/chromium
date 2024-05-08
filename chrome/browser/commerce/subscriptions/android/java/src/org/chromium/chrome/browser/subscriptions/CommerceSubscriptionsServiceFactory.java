// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/** {@link CommerceSubscriptionsService} cached by {@link Profile}. */
public class CommerceSubscriptionsServiceFactory {
    private static CommerceSubscriptionsServiceFactory sInstance;
    private static CommerceSubscriptionsService sSubscriptionsServiceForTesting;

    private final ProfileKeyedMap<CommerceSubscriptionsService> mProfileToSubscriptionsService;

    /** Return the singleton instance of the CommerceSubscriptionsServiceFactory. */
    public static CommerceSubscriptionsServiceFactory getInstance() {
        if (sInstance == null) sInstance = new CommerceSubscriptionsServiceFactory();
        return sInstance;
    }

    /** Creates new instance. */
    private CommerceSubscriptionsServiceFactory() {
        mProfileToSubscriptionsService =
                ProfileKeyedMap.createMapOfDestroyables(
                        ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL);
    }

    /**
     * Creates a new instance or reuses an existing one based on the {@link Profile}.
     *
     * <p>Note: Don't hold a reference to the returned value. Always use this method to access
     * {@link CommerceSubscriptionsService} instead.
     *
     * @return {@link CommerceSubscriptionsService} instance for the regular profile.
     */
    public CommerceSubscriptionsService getForProfile(Profile profile) {
        if (sSubscriptionsServiceForTesting != null) return sSubscriptionsServiceForTesting;
        return mProfileToSubscriptionsService.getForProfile(
                profile, CommerceSubscriptionsServiceFactory::buildForProfile);
    }

    private static CommerceSubscriptionsService buildForProfile(Profile profile) {
        PriceDropNotificationManager priceDropNotificationManager =
                PriceDropNotificationManagerFactory.create(profile);
        return new CommerceSubscriptionsService(
                ShoppingServiceFactory.getForProfile(profile), priceDropNotificationManager);
    }

    /** Sets the CommerceSubscriptionsService for testing. */
    public static void setSubscriptionsServiceForTesting(
            CommerceSubscriptionsService subscriptionsService) {
        sSubscriptionsServiceForTesting = subscriptionsService;
        ResettersForTesting.register(() -> sSubscriptionsServiceForTesting = null);
    }
}

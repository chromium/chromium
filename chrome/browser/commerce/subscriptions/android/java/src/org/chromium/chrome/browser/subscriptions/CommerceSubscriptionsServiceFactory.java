// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;

import java.util.HashMap;
import java.util.Map;

/**
 * {@link CommerceSubscriptionsService} cached by {@link Profile}.
 */
public class CommerceSubscriptionsServiceFactory {
    @VisibleForTesting
    protected static final Map<Profile, CommerceSubscriptionsService>
            sProfileToSubscriptionsService = new HashMap<>();
    private static ProfileManager.Observer sProfileManagerObserver;
    private static CommerceSubscriptionsService sSubscriptionsServiceForTesting;

    /** Creates new instance. */
    public CommerceSubscriptionsServiceFactory() {
        if (sProfileManagerObserver == null) {
            sProfileManagerObserver = new ProfileManager.Observer() {
                @Override
                public void onProfileAdded(Profile profile) {}

                @Override
                public void onProfileDestroyed(Profile destroyedProfile) {
                    CommerceSubscriptionsService serviceToDestroy =
                            sProfileToSubscriptionsService.get(destroyedProfile);
                    if (serviceToDestroy != null) {
                        serviceToDestroy.destroy();
                        sProfileToSubscriptionsService.remove(destroyedProfile);
                    }

                    if (sProfileToSubscriptionsService.isEmpty()) {
                        ProfileManager.removeObserver(sProfileManagerObserver);
                        sProfileManagerObserver = null;
                    }
                }
            };
            ProfileManager.addObserver(sProfileManagerObserver);
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
        CommerceSubscriptionsService service = sProfileToSubscriptionsService.get(profile);
        if (service == null) {
            PriceDropNotificationManager priceDropNotificationManager =
                    PriceDropNotificationManagerFactory.create();
            service = new CommerceSubscriptionsService(
                    ShoppingServiceFactory.getForProfile(profile), priceDropNotificationManager);
            sProfileToSubscriptionsService.put(profile, service);
        }
        return service;
    }

    /** Sets the CommerceSubscriptionsService for testing. */
    @VisibleForTesting
    public static void setSubscriptionsServiceForTesting(
            CommerceSubscriptionsService subscriptionsService) {
        sSubscriptionsServiceForTesting = subscriptionsService;
    }
}

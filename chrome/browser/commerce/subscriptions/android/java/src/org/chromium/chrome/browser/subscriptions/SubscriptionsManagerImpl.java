// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.List;

/**
 * Implementation of {@link SubscriptionsManager} to manage price drop related subscriptions.
 * TODO(crbug.com/1186450): Pull subscription type specific code into respective handlers to
 * simplify this class.
 */
public class SubscriptionsManagerImpl implements SubscriptionsManager {
    private final CommerceSubscriptionsStorage mStorage;
    private static List<CommerceSubscription> sRemoteSubscriptionsForTesting;

    public SubscriptionsManagerImpl() {
        mStorage = new CommerceSubscriptionsStorage(Profile.getLastUsedRegularProfile());
    }

    /**
     * Creates a new subscription on the server-side and refreshes the local storage of
     * subscriptions.
     * @param subscription The {@link CommerceSubscription} to add.
     */
    @Override
    public void subscribe(CommerceSubscription subscription) {
        String type = subscription.getType();
        if (type.equals(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK)) {
            // TODO(crbug.com/1186450): Replace getSubscriptions with callback from subscription
            // request.
            getSubscriptions(type, true,
                    remoteSubscriptions
                    -> updateStorageWithSubscriptions(type, remoteSubscriptions));
        }
    }

    /**
     * Creates new subscriptions in batch if needed.
     * @param subscriptions The list of {@link CommerceSubscription} to add.
     */
    @Override
    public void subscribe(List<CommerceSubscription> subscriptions) {
        if (subscriptions.size() == 0) return;

        String type = subscriptions.get(0).getType();
        if (type.equals(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK)) {
            // TODO(crbug.com/1186450): Replace getSubscriptions with callback from subscription
            // request.
            getSubscriptions(type, true,
                    remoteSubscriptions
                    -> updateStorageWithSubscriptions(type, remoteSubscriptions));
        }
    }

    /**
     * Destroys a subscription on the server-side and refreshes the local storage of subscriptions.
     * @param subscription The {@link CommerceSubscription} to destroy.
     */
    @Override
    public void unsubscribe(CommerceSubscription subscription) {
        String type = subscription.getType();
        if (type.equals(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK)) {
            // TODO(crbug.com/1186450): Replace getSubscriptions with callback from unsubscription
            // request.
            getSubscriptions(type, true,
                    remoteSubscriptions
                    -> updateStorageWithSubscriptions(type, remoteSubscriptions));
        }
    }

    /**
     * Returns all subscriptions that match the provided type.
     * @param type The {@link CommerceSubscription.CommerceSubscriptionType} to query.
     * @param forceFetch Whether to fetch from server. If no, fetch from local storage.
     */
    @Override
    public void getSubscriptions(@CommerceSubscription.CommerceSubscriptionType String type,
            boolean forceFetch, Callback<List<CommerceSubscription>> callback) {
        if (sRemoteSubscriptionsForTesting != null) {
            callback.onResult(sRemoteSubscriptionsForTesting);
            return;
        }
        if (!forceFetch) {
            mStorage.loadWithPrefix(String.valueOf(type),
                    localSubscriptions -> callback.onResult(localSubscriptions));
        }
    }

    private void updateStorageWithSubscriptions(
            @CommerceSubscription.CommerceSubscriptionType String type,
            List<CommerceSubscription> remoteSubscriptions) {
        mStorage.loadWithPrefix(String.valueOf(type), localSubscriptions -> {
            for (CommerceSubscription subscription : localSubscriptions) {
                if (!remoteSubscriptions.contains(subscription)) {
                    mStorage.delete(subscription);
                }
            }
            for (CommerceSubscription subscription : remoteSubscriptions) {
                if (!localSubscriptions.contains(subscription)) {
                    mStorage.save(subscription);
                }
            }
        });
    }

    @VisibleForTesting
    public void setRemoteSubscriptionsForTesting(List<CommerceSubscription> subscriptions) {
        sRemoteSubscriptionsForTesting = subscriptions;
    }
}
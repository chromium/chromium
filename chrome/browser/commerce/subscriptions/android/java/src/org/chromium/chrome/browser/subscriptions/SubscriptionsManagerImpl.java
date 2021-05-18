// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/**
 * Implementation of {@link SubscriptionsManager} to manage price drop related subscriptions.
 * TODO(crbug.com/1186450): Pull subscription type specific code into respective handlers to
 * simplify this class.
 */
public class SubscriptionsManagerImpl implements SubscriptionsManager {
    private final CommerceSubscriptionsStorage mStorage;
    private final CommerceSubscriptionsServiceProxy mServiceProxy;
    private static List<CommerceSubscription> sRemoteSubscriptionsForTesting;

    public SubscriptionsManagerImpl(Profile profile) {
        mStorage = new CommerceSubscriptionsStorage(profile);
        mServiceProxy = new CommerceSubscriptionsServiceProxy(profile);
    }

    /**
     * Creates a new subscription on the server-side and refreshes the local storage of
     * subscriptions.
     * @param subscription The {@link CommerceSubscription} to add.
     * @param callback indicates whether or not the operation was successful.
     */
    @Override
    public void subscribe(CommerceSubscription subscription, Callback<Boolean> callback) {
        if (subscription == null || !isSubscriptionTypeSupported(subscription.getType())) {
            callback.onResult(false);
            return;
        }

        mServiceProxy.create(new ArrayList<CommerceSubscription>() {
            { add(subscription); };
        }, (didSucceed) -> handleUpdateSubscriptionsResponse(didSucceed, subscription.getType()));
    }

    /**
     * Creates new subscriptions in batch if needed.
     * @param subscriptions The list of {@link CommerceSubscription} to add.
     * @param callback indicates whether or not the operation was successful.
     */
    @Override
    public void subscribe(List<CommerceSubscription> subscriptions, Callback<Boolean> callback) {
        if (subscriptions.size() == 0) {
            callback.onResult(false);
            return;
        }

        String type = subscriptions.get(0).getType();
        if (isSubscriptionTypeSupported(type)) {
            mServiceProxy.create(subscriptions,
                    (didSucceed) -> handleUpdateSubscriptionsResponse(didSucceed, type));
        } else {
            callback.onResult(false);
        }
    }

    /**
     * Destroys a subscription on the server-side and refreshes the local storage of subscriptions.
     * @param subscription The {@link CommerceSubscription} to destroy.
     * @param callback indicates whether or not the operation was successful.
     */
    @Override
    public void unsubscribe(CommerceSubscription subscription, Callback<Boolean> callback) {
        String type = subscription.getType();
        if (subscription == null || !isSubscriptionTypeSupported(type)) {
            callback.onResult(false);
            return;
        }

        mServiceProxy.delete(new ArrayList<CommerceSubscription>() {
            { add(subscription); };
        }, (didSucceed) -> handleUpdateSubscriptionsResponse(didSucceed, type));
    }

    /**
     * Returns all subscriptions that match the provided type.
     * @param type The {@link CommerceSubscription.CommerceSubscriptionType} to query.
     * @param forceFetch Whether to fetch from server. If no, fetch from local storage.
     * @param callback returns the list of subscriptions.
     */
    @Override
    public void getSubscriptions(@CommerceSubscription.CommerceSubscriptionType String type,
            boolean forceFetch, Callback<List<CommerceSubscription>> callback) {
        if (sRemoteSubscriptionsForTesting != null) {
            callback.onResult(sRemoteSubscriptionsForTesting);
            return;
        }
        if (forceFetch) {
            mServiceProxy.get(type, callback);
        } else {
            mStorage.loadWithPrefix(String.valueOf(type),
                    localSubscriptions -> callback.onResult(localSubscriptions));
        }
    }

    private boolean isSubscriptionTypeSupported(
            @CommerceSubscription.CommerceSubscriptionType String type) {
        return CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK.equals(type);
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

    private void handleUpdateSubscriptionsResponse(
            Boolean didSucceed, @CommerceSubscription.CommerceSubscriptionType String type) {
        assert didSucceed : "Failed to handle update subscriptions response";
        if (didSucceed) {
            getSubscriptions(type, true,
                    remoteSubscriptions
                    -> updateStorageWithSubscriptions(type, remoteSubscriptions));
        }
    }

    @VisibleForTesting
    public void setRemoteSubscriptionsForTesting(List<CommerceSubscription> subscriptions) {
        sRemoteSubscriptionsForTesting = subscriptions;
    }
}
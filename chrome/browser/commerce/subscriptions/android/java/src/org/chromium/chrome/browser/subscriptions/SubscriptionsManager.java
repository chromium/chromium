// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Interface for exposing {@link CommerceSubscription} management.
 */
public interface SubscriptionsManager {
    @IntDef({StatusCode.OK, StatusCode.NETWORK_ERROR, StatusCode.INTERNAL_ERROR,
            StatusCode.INVALID_ARGUMENT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface StatusCode {
        int OK = 0;
        int NETWORK_ERROR = 2;
        int INTERNAL_ERROR = 3;
        int INVALID_ARGUMENT = 4;
    }

    /**
     * Creates a new subscription on the server if needed.
     * @param subscription The {@link CommerceSubscription} to add.
     * @param callback indicates whether or not the operation was successful.
     */
    void subscribe(CommerceSubscription subscription, Callback<Integer> callback);

    /**
     * Creates new subscriptions in batch if needed.
     * @param subscriptions The list of {@link CommerceSubscription} to add.
     * @param callback indicates whether or not the operation was successful.
     */
    void subscribe(List<CommerceSubscription> subscriptions, Callback<Integer> callback);

    /**
     * Destroys a subscription on the server if needed.
     * @param subscription The {@link CommerceSubscription} to destroy.
     * @param callback indicates whether or not the operation was successful.
     */
    void unsubscribe(CommerceSubscription subscription, Callback<Integer> callback);

    /**
     * Returns all subscriptions that match the provided type.
     * @param type The {@link CommerceSubscription.CommerceSubscriptionType} to query.
     * @param forceFetch Whether to fetch from server.
     * @param callback returns the list of subscriptions.
     */
    void getSubscriptions(@CommerceSubscription.CommerceSubscriptionType String type,
            boolean forceFetch, Callback<List<CommerceSubscription>> callback);

    /**
     * Checks if the given subscription matches any subscriptions in local storage.
     *
     * @param subscription The subscription to check.
     * @param callback The callback to receive the result.
     */
    void isSubscribed(CommerceSubscription subscription, Callback<Boolean> callback);
}

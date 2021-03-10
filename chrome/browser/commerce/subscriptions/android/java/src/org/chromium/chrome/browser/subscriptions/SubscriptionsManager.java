// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

/**
 * Interface for exposing {@link CommerceSubscription} management.
 */
public interface SubscriptionsManager {
    /**
     * Creates a new subscription on the server if needed.
     * @param subscription The {@link CommerceSubscription} to add.
     */
    void subscribe(CommerceSubscription subscription);

    /**
     * Destroys a subscription on the server if needed.
     * @param subscription The {@link CommerceSubscription} to destroy.
     */
    void unsubscribe(CommerceSubscription subscription);

    /**
     * Returns all subscriptions that match the provided type.
     * @param type The {@link CommerceSubscription.CommerceSubscriptionType} to query.
     * @param forceFetch Whether to fetch from server.
     */
    void getSubscriptions(CommerceSubscription.CommerceSubscriptionType type, boolean forceFetch);
}
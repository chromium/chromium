// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

/**
 * Implementation of {@link SubscriptionsManager} to manage price drop related subscriptions.
 */
public class SubscriptionsManagerImpl implements SubscriptionsManager {
    public SubscriptionsManagerImpl() {}

    /**
     * Creates a new subscription on the server-side and refreshes the local storage of
     * subscriptions.
     * @param subscription The {@link CommerceSubscription} to add.
     */
    @Override
    public void subscribe(CommerceSubscription subscription) {}

    /**
     * Destroys a subscription on the server-side and refreshes the local storage of subscriptions.
     * @param subscription The {@link CommerceSubscription} to destroy.
     */
    @Override
    public void unsubscribe(CommerceSubscription subscription) {}

    /**
     * Returns all subscriptions that match the provided type.
     * @param type The {@link CommerceSubscription.CommerceSubscriptionType} to query.
     * @param forceFetch Whether to fetch from server. If no, fetch from local storage.
     */
    @Override
    public void getSubscriptions(
            CommerceSubscription.CommerceSubscriptionType type, boolean forceFetch) {}
}
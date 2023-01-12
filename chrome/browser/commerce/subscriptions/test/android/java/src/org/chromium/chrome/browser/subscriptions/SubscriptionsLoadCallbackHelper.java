// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import org.chromium.base.test.util.CallbackHelper;

import java.util.List;

/**
 * Helper class for load operations to get load results from {@link CommerceSubscriptionsStorage}.
 */
public class SubscriptionsLoadCallbackHelper extends CallbackHelper {
    private CommerceSubscription mSingleResult;
    private List<CommerceSubscription> mResultList;

    /**
     * Notifies that the callback has returned with single subscription and cache the result.
     * @param subscription The {@link CommerceSubscription} returned in callback.
     */
    void notifyCalled(CommerceSubscription subscription) {
        mSingleResult = subscription;
        notifyCalled();
    }

    /**
     * Notifies that the callback has returned with a list of subscriptions and cache the result.
     * @param subscriptions The list of {@link CommerceSubscription} returned in callback.
     */
    void notifyCalled(List<CommerceSubscription> subscriptions) {
        mResultList = subscriptions;
        notifyCalled();
    }

    /**
     * Gets the single {@link CommerceSubscription} from callback.
     * @return The single {@link CommerceSubscription} in callback.
     */
    CommerceSubscription getSingleResult() {
        return mSingleResult;
    }

    /**
     * Gets the list of {@link CommerceSubscription} from callback.
     * @return The list of {@link CommerceSubscription} in callback.
     */
    List<CommerceSubscription> getResultList() {
        return mResultList;
    }
}

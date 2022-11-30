// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import org.chromium.base.test.util.CallbackHelper;

import java.util.List;

/**
 * Helper class for load operations to get load results from {@link
 * MerchantTrustSignalsEventStorage}.
 */
public class MerchantTrustSignalsEventLoadCallbackHelper extends CallbackHelper {
    private MerchantTrustSignalsEvent mSingleResult;
    private List<MerchantTrustSignalsEvent> mResultList;

    /**
     * Notifies that the callback has returned with single event and cache the result.
     * @param event The {@link MerchantTrustSignalsEvent} returned in callback.
     */
    void notifyCalled(MerchantTrustSignalsEvent event) {
        mSingleResult = event;
        notifyCalled();
    }

    /**
     * Notifies that the callback has returned with a list of events and cache the result.
     * @param events The list of {@link MerchantTrustSignalsEvent} returned in callback.
     */
    void notifyCalled(List<MerchantTrustSignalsEvent> events) {
        mResultList = events;
        notifyCalled();
    }

    /**
     * Gets the single {@link MerchantTrustSignalsEvent} from callback.
     * @return The single {@link MerchantTrustSignalsEvent} in callback.
     */
    MerchantTrustSignalsEvent getSingleResult() {
        return mSingleResult;
    }

    /**
     * Gets the list of {@link MerchantTrustSignalsEvent} from callback.
     * @return The list of {@link MerchantTrustSignalsEvent} in callback.
     */
    List<MerchantTrustSignalsEvent> getResultList() {
        return mResultList;
    }
}

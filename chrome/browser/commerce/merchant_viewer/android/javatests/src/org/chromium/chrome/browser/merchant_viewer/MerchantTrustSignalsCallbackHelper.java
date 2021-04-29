// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.merchant_viewer.proto.MerchantTrustSignalsOuterClass.MerchantTrustSignals;

/**
 * Extends {@link CallbackHelper} to offer a set of convenience methods for handling merchant trust
 * callbacks.
 */
class MerchantTrustSignalsCallbackHelper extends CallbackHelper {
    private MerchantTrustMessageContext mResult;
    private MerchantTrustSignals mMerchantTrustSignalsResult;

    /** Handles callbacks with type {@link MerchantTrustMessageContext}. */
    void notifyCalled(MerchantTrustMessageContext context) {
        mResult = context;
        notifyCalled();
    }

    /** Handles callbacks with type {@link MerchantTrustSignals}. */
    void notifyCalled(MerchantTrustSignals signals) {
        mMerchantTrustSignalsResult = signals;
        notifyCalled();
    }

    /** Returns the cached {@link MerchantTrustSignals} result. */

    MerchantTrustSignals getMerchantTrustSignalsResult() {
        return mMerchantTrustSignalsResult;
    }

    /** Returns the cached {@link MerchantTrustMessageContext} result. */

    MerchantTrustMessageContext getResult() {
        return mResult;
    }
}

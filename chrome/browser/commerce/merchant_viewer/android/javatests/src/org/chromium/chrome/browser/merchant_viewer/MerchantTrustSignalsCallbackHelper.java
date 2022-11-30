// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.commerce.core.ShoppingService.MerchantInfo;

/**
 * Extends {@link CallbackHelper} to offer a set of convenience methods for handling merchant trust
 * callbacks.
 */
class MerchantTrustSignalsCallbackHelper extends CallbackHelper {
    private MerchantTrustMessageContext mResult;
    private MerchantInfo mMerchantInfoResult;

    /** Handles callbacks with type {@link MerchantTrustMessageContext}. */
    void notifyCalled(MerchantTrustMessageContext context) {
        mResult = context;
        notifyCalled();
    }

    /** Handles callbacks with type {@link MerchantInfo}. */
    void notifyCalled(MerchantInfo info) {
        mMerchantInfoResult = info;
        notifyCalled();
    }

    /** Returns the cached {@link MerchantInfo} result. */
    MerchantInfo getMerchantTrustSignalsResult() {
        return mMerchantInfoResult;
    }

    /** Returns the cached {@link MerchantTrustMessageContext} result. */
    MerchantTrustMessageContext getResult() {
        return mResult;
    }
}

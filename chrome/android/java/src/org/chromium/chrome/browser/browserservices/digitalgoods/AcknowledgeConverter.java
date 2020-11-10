// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.convertResponseCode;

import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.trusted.TrustedWebActivityCallback;

import org.chromium.base.Log;
import org.chromium.payments.mojom.BillingResponseCode;
import org.chromium.payments.mojom.DigitalGoods.AcknowledgeResponse;

/**
 * A converter that deals with the parameters and result for Acknowledge calls.
 */
class AcknowledgeConverter {
    private static final String TAG = "DigitalGoods";

    static final String PARAM_ACKNOWLEDGE_PURCHASE_TOKEN = "acknowledge.purchaseToken";
    static final String PARAM_ACKNOWLEDGE_MAKE_AVAILABLE_AGAIN = "acknowledge.makeAvailableAgain";
    static final String RESPONSE_ACKNOWLEDGE = "acknowledge.response";
    static final String RESPONSE_ACKNOWLEDGE_RESPONSE_CODE = "acknowledge.responseCode";

    private AcknowledgeConverter() {}

    static Bundle convertParams(String purchaseToken, boolean makeAvailableAgain) {
        Bundle bundle = new Bundle();
        bundle.putString(PARAM_ACKNOWLEDGE_PURCHASE_TOKEN, purchaseToken);
        bundle.putBoolean(PARAM_ACKNOWLEDGE_MAKE_AVAILABLE_AGAIN, makeAvailableAgain);
        return bundle;
    }

    static TrustedWebActivityCallback convertCallback(AcknowledgeResponse callback) {
        return new TrustedWebActivityCallback() {
            @Override
            public void onExtraCallback(@NonNull String callbackName, @Nullable Bundle args) {
                if (!RESPONSE_ACKNOWLEDGE.equals(callbackName)) {
                    Log.w(TAG, "Wrong callback name given: " + callbackName + ".");
                    returnClientAppError(callback);
                    return;
                }

                if (args == null) {
                    Log.w(TAG, "No args provided.");
                    returnClientAppError(callback);
                    return;
                }

                if (!(args.get(RESPONSE_ACKNOWLEDGE_RESPONSE_CODE) instanceof Integer)) {
                    Log.w(TAG, "Poorly formed args provided.");
                    returnClientAppError(callback);
                    return;
                }

                int code = args.getInt(RESPONSE_ACKNOWLEDGE_RESPONSE_CODE);
                callback.call(convertResponseCode(code, args));
            }
        };
    }

    public static void returnClientAppUnavailable(AcknowledgeResponse callback) {
        callback.call(BillingResponseCode.CLIENT_APP_UNAVAILABLE);
    }

    public static void returnClientAppError(AcknowledgeResponse callback) {
        callback.call(BillingResponseCode.CLIENT_APP_ERROR);
    }

    /**
     * Creates a {@link Bundle} that represents the result of an acknowledge call. This would be
     * carried out by the client app and is only here to help testing.
     */
    @VisibleForTesting
    public static Bundle createResponseBundle(int responseCode) {
        Bundle bundle = new Bundle();

        bundle.putInt(RESPONSE_ACKNOWLEDGE_RESPONSE_CODE, responseCode);

        return bundle;
    }
}

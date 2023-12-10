// Copyright 2022 The Chromium Authors
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
import org.chromium.payments.mojom.DigitalGoods.Consume_Response;

/** A converter that deals with the parameters and result for Consume calls. */
class ConsumeConverter {
    private static final String TAG = "DigitalGoods";

    static final String PARAM_CONSUME_PURCHASE_TOKEN = "consume.purchaseToken";
    static final String RESPONSE_CONSUME = "consume.response";
    static final String RESPONSE_CONSUME_RESPONSE_CODE = "consume.responseCode";

    private ConsumeConverter() {}

    static Bundle convertParams(String purchaseToken) {
        Bundle bundle = new Bundle();
        bundle.putString(PARAM_CONSUME_PURCHASE_TOKEN, purchaseToken);
        return bundle;
    }

    static TrustedWebActivityCallback convertCallback(Consume_Response callback) {
        return new TrustedWebActivityCallback() {
            @Override
            public void onExtraCallback(@NonNull String callbackName, @Nullable Bundle args) {
                if (!RESPONSE_CONSUME.equals(callbackName)) {
                    Log.w(TAG, "Wrong callback name given: " + callbackName + ".");
                    returnClientAppError(callback);
                    return;
                }

                if (args == null) {
                    Log.w(TAG, "No args provided.");
                    returnClientAppError(callback);
                    return;
                }

                if (!(args.get(RESPONSE_CONSUME_RESPONSE_CODE) instanceof Integer)) {
                    Log.w(TAG, "Poorly formed args provided.");
                    returnClientAppError(callback);
                    return;
                }

                int code = args.getInt(RESPONSE_CONSUME_RESPONSE_CODE);
                callback.call(convertResponseCode(code, args));
            }
        };
    }

    public static void returnClientAppUnavailable(Consume_Response callback) {
        callback.call(BillingResponseCode.CLIENT_APP_UNAVAILABLE);
    }

    public static void returnClientAppError(Consume_Response callback) {
        callback.call(BillingResponseCode.CLIENT_APP_ERROR);
    }

    /**
     * Creates a {@link Bundle} that represents the result of an consume call. This would be
     * carried out by the client app and is only here to help testing.
     */
    @VisibleForTesting
    public static Bundle createResponseBundle(int responseCode) {
        Bundle bundle = new Bundle();

        bundle.putInt(RESPONSE_CONSUME_RESPONSE_CODE, responseCode);

        return bundle;
    }
}

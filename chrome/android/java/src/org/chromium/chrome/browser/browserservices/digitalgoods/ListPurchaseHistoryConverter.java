// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.checkField;
import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.convertParcelableArray;
import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.convertResponseCode;

import android.os.Bundle;
import android.os.Parcelable;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.trusted.TrustedWebActivityCallback;

import org.chromium.base.Log;
import org.chromium.payments.mojom.BillingResponseCode;
import org.chromium.payments.mojom.DigitalGoods.ListPurchaseHistory_Response;
import org.chromium.payments.mojom.PurchaseReference;

/** A converter that deals with the results of ListPurchaseHistory calls. */
class ListPurchaseHistoryConverter {
    private static final String TAG = "DigitalGoods";

    static final String RESPONSE_COMMAND = "listPurchaseHistory.response";
    static final String KEY_PURCHASES_LIST = "listPurchaseHistory.purchasesList";
    static final String KEY_RESPONSE_CODE = "listPurchaseHistory.responseCode";

    private ListPurchaseHistoryConverter() {}

    /**
     * Produces a {@link TrustedWebActivityCallback} that calls the given
     * {@link ListPurchaseHistoryResponse}.
     */
    static TrustedWebActivityCallback convertCallback(ListPurchaseHistory_Response callback) {
        return new TrustedWebActivityCallback() {
            @Override
            public void onExtraCallback(@NonNull String callbackName, @Nullable Bundle args) {
                if (!RESPONSE_COMMAND.equals(callbackName)) {
                    Log.w(TAG, "Wrong callback name given: " + callbackName + ".");
                    returnClientAppError(callback);
                    return;
                }

                if (args == null) {
                    Log.w(TAG, "No args provided.");
                    returnClientAppError(callback);
                    return;
                }

                if (!checkField(args, KEY_RESPONSE_CODE, Integer.class)
                        || !checkField(args, KEY_PURCHASES_LIST, Parcelable[].class)) {
                    returnClientAppError(callback);
                    return;
                }

                int code = args.getInt(KEY_RESPONSE_CODE);
                Parcelable[] array = args.getParcelableArray(KEY_PURCHASES_LIST);

                PurchaseReference[] reference =
                        convertParcelableArray(
                                        array, ListPurchasesConverter::convertPurchaseReference)
                                .toArray(new PurchaseReference[0]);
                callback.call(convertResponseCode(code, args), reference);
            }
        };
    }

    static void returnClientAppUnavailable(ListPurchaseHistory_Response callback) {
        callback.call(BillingResponseCode.CLIENT_APP_UNAVAILABLE, new PurchaseReference[0]);
    }

    static void returnClientAppError(ListPurchaseHistory_Response callback) {
        callback.call(BillingResponseCode.CLIENT_APP_ERROR, new PurchaseReference[0]);
    }

    @VisibleForTesting
    static Bundle createResponseBundle(int responseCode, Bundle... purchaseDetails) {
        Bundle bundle = new Bundle();

        bundle.putInt(KEY_RESPONSE_CODE, responseCode);
        bundle.putParcelableArray(KEY_PURCHASES_LIST, purchaseDetails);

        return bundle;
    }
}

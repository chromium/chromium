// Copyright 2020 The Chromium Authors
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
import org.chromium.payments.mojom.DigitalGoods.ListPurchases_Response;
import org.chromium.payments.mojom.PurchaseReference;

/** A converter that deals with the results of ListPurchases calls. */
class ListPurchasesConverter {
    private static final String TAG = "DigitalGoods";

    static final String RESPONSE_COMMAND = "listPurchases.response";
    static final String KEY_PURCHASES_LIST = "listPurchases.purchasesList";
    static final String KEY_RESPONSE_CODE = "listPurchases.responseCode";

    static final String KEY_ITEM_ID = "purchaseDetails.itemId";
    static final String KEY_PURCHASE_TOKEN = "purchaseDetails.purchaseToken";

    private ListPurchasesConverter() {}

    /**
     * Produces a {@link TrustedWebActivityCallback} that calls the given
     * {@link ListPurchasesResponse}.
     */
    static TrustedWebActivityCallback convertCallback(ListPurchases_Response callback) {
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

    /** Convert a Bundle into a PurchaseReference object. */
    static PurchaseReference convertPurchaseReference(Bundle purchase) {
        if (!checkField(purchase, KEY_ITEM_ID, String.class)) return null;
        if (!checkField(purchase, KEY_PURCHASE_TOKEN, String.class)) return null;

        PurchaseReference result = new PurchaseReference();

        result.itemId = purchase.getString(KEY_ITEM_ID);
        result.purchaseToken = purchase.getString(KEY_PURCHASE_TOKEN);

        return result;
    }

    static void returnClientAppUnavailable(ListPurchases_Response callback) {
        callback.call(BillingResponseCode.CLIENT_APP_UNAVAILABLE, new PurchaseReference[0]);
    }

    static void returnClientAppError(ListPurchases_Response callback) {
        callback.call(BillingResponseCode.CLIENT_APP_ERROR, new PurchaseReference[0]);
    }

    @VisibleForTesting
    static Bundle createPurchaseReferenceBundle(String itemId, String purchaseToken) {
        Bundle bundle = new Bundle();

        bundle.putString(KEY_ITEM_ID, itemId);
        bundle.putString(KEY_PURCHASE_TOKEN, purchaseToken);

        return bundle;
    }

    @VisibleForTesting
    static Bundle createResponseBundle(int responseCode, Bundle... purchaseDetails) {
        Bundle bundle = new Bundle();

        bundle.putInt(KEY_RESPONSE_CODE, responseCode);
        bundle.putParcelableArray(KEY_PURCHASES_LIST, purchaseDetails);

        return bundle;
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
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
import org.chromium.mojo_base.mojom.TimeDelta;
import org.chromium.payments.mojom.BillingResponseCode;
import org.chromium.payments.mojom.DigitalGoods.ListPurchasesResponse;
import org.chromium.payments.mojom.PurchaseDetails;
import org.chromium.payments.mojom.PurchaseState;

/**
 * A converter that deals with the results of ListPurchases calls.
 */
class ListPurchasesConverter {
    private static final String TAG = "DigitalGoods";

    static final String RESPONSE_COMMAND = "listPurchases.response";
    static final String KEY_PURCHASES_LIST = "listPurchases.purchasesList";
    static final String KEY_RESPONSE_CODE = "listPurchases.responseCode";

    static final String KEY_ITEM_ID = "purchaseDetails.itemId";
    static final String KEY_PURCHASE_TOKEN = "purchaseDetails.purchaseToken";
    static final String KEY_ACKNOWLEDGED = "purchaseDetails.acknowledged";
    static final String KEY_PURCHASE_STATE = "purchaseDetails.purchaseState";
    static final String KEY_PURCHASE_TIME_MICROSECONDS_PAST_UNIX_EPOCH =
            "purchaseDetails.purchaseTimeMicrosecondsPastUnixEpoch";
    static final String KEY_WILL_AUTO_RENEW = "purchaseDetails.willAutoRenew";

    // These values are copied from the Play Billing library since Chrome cannot depend on it.
    // https://developer.android.com/reference/com/android/billingclient/api/Purchase.PurchaseState
    static final int PLAY_BILLING_PURCHASE_STATE_PENDING = 2;
    static final int PLAY_BILLING_PURCHASE_STATE_PURCHASED = 1;
    static final int PLAY_BILLING_PURCHASE_STATE_UNSPECIFIED = 0;

    /**
     * Produces a {@link TrustedWebActivityCallback} that calls the given
     * {@link ListPurchasesResponse}.
     */
    static TrustedWebActivityCallback convertCallback(ListPurchasesResponse callback) {
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

                PurchaseDetails[] details = convertParcelableArray(
                        array, ListPurchasesConverter::convertPurchaseDetails)
                                                    .toArray(new PurchaseDetails[0]);
                callback.call(convertResponseCode(code, args), details);
            }
        };
    }

    static PurchaseDetails convertPurchaseDetails(Bundle purchase) {
        if (!checkField(purchase, KEY_ITEM_ID, String.class)) return null;
        if (!checkField(purchase, KEY_PURCHASE_TOKEN, String.class)) return null;
        if (!checkField(purchase, KEY_ACKNOWLEDGED, Boolean.class)) return null;
        if (!checkField(purchase, KEY_PURCHASE_STATE, Integer.class)) return null;
        if (!checkField(purchase, KEY_PURCHASE_TIME_MICROSECONDS_PAST_UNIX_EPOCH, Long.class)) {
            return null;
        }
        if (!checkField(purchase, KEY_WILL_AUTO_RENEW, Boolean.class)) return null;

        TimeDelta purchaseTime = new TimeDelta();
        purchaseTime.microseconds =
                purchase.getLong(KEY_PURCHASE_TIME_MICROSECONDS_PAST_UNIX_EPOCH);

        PurchaseDetails result = new PurchaseDetails();

        result.itemId = purchase.getString(KEY_ITEM_ID);
        result.purchaseToken = purchase.getString(KEY_PURCHASE_TOKEN);
        result.acknowledged = purchase.getBoolean(KEY_ACKNOWLEDGED);
        result.purchaseState = convertPurchaseState(purchase.getInt(KEY_PURCHASE_STATE));
        result.purchaseTime = purchaseTime;
        result.willAutoRenew = purchase.getBoolean(KEY_WILL_AUTO_RENEW);

        return result;
    }

    static int convertPurchaseState(int purchaseState) {
        switch (purchaseState) {
            case PLAY_BILLING_PURCHASE_STATE_PENDING:
                return PurchaseState.PENDING;
            case PLAY_BILLING_PURCHASE_STATE_PURCHASED:
                return PurchaseState.PURCHASED;
            default:
                return PurchaseState.UNKNOWN;
        }
    }

    static void returnClientAppUnavailable(ListPurchasesResponse callback) {
        callback.call(BillingResponseCode.CLIENT_APP_UNAVAILABLE, new PurchaseDetails[0]);
    }

    static void returnClientAppError(ListPurchasesResponse callback) {
        callback.call(BillingResponseCode.CLIENT_APP_ERROR, new PurchaseDetails[0]);
    }

    @VisibleForTesting
    static Bundle createPurchaseDetailsBundle(String itemId, String purchaseToken,
            boolean acknowledged, int purchaseState, long purchaseTimeMicrosecondsPastUnixEpoch,
            boolean willAutoRenew) {
        Bundle bundle = new Bundle();

        bundle.putString(KEY_ITEM_ID, itemId);
        bundle.putString(KEY_PURCHASE_TOKEN, purchaseToken);
        bundle.putBoolean(KEY_ACKNOWLEDGED, acknowledged);
        bundle.putInt(KEY_PURCHASE_STATE, purchaseState);
        bundle.putLong(KEY_PURCHASE_TIME_MICROSECONDS_PAST_UNIX_EPOCH,
                purchaseTimeMicrosecondsPastUnixEpoch);
        bundle.putBoolean(KEY_WILL_AUTO_RENEW, willAutoRenew);

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

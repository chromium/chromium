// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import android.os.Bundle;
import android.os.Parcelable;

import org.chromium.base.Log;
import org.chromium.payments.mojom.BillingResponseCode;
import org.chromium.payments.mojom.DigitalGoods.AcknowledgeResponse;
import org.chromium.payments.mojom.DigitalGoods.GetDetailsResponse;
import org.chromium.payments.mojom.ItemDetails;
import org.chromium.payments.mojom.PaymentCurrencyAmount;

import java.util.ArrayList;
import java.util.List;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.trusted.TrustedWebActivityCallback;

/**
 * This class takes care of converting between the mojo types that {@link DigitalGoodsImpl} deals
 * with and the Android types that {@link TrustedWebActivityClient} details with.
 *
 * Ideally this class would have no Chromium dependencies that are not from Mojo (in a *.mojom.*
 * package) to allow it to be more easily reused in ARC++.
 */
public class DigitalGoodsConverter {
    private static final String TAG = "DigitalGoods";

    static final String PARAM_GET_DETAILS_ITEM_IDS = "getDetails.itemIds";
    public static final String RESPONSE_GET_DETAILS = "getDetails.response";
    static final String RESPONSE_GET_DETAILS_RESPONSE_CODE = "getDetails.responseCode";
    static final String RESPONSE_GET_DETAILS_DETAILS_LIST = "getDetails.detailsList";

    static final String ITEM_DETAILS_ID = "itemDetails.id";
    static final String ITEM_DETAILS_TITLE = "itemDetails.title";
    static final String ITEM_DETAILS_DESC = "itemDetails.description";
    static final String ITEM_DETAILS_CURRENCY = "itemDetails.currency";
    static final String ITEM_DETAILS_VALUE = "itemDetails.value";
    private static final String[] ITEM_DETAILS_ALL_FIELDS = { ITEM_DETAILS_ID, ITEM_DETAILS_TITLE,
            ITEM_DETAILS_DESC, ITEM_DETAILS_CURRENCY, ITEM_DETAILS_VALUE };

    static final String PARAM_ACKNOWLEDGE_PURCHASE_TOKEN = "acknowledge.purchaseToken";
    static final String PARAM_ACKNOWLEDGE_MAKE_AVAILABLE_AGAIN = "acknowledge.makeAvailableAgain";
    static final String RESPONSE_ACKNOWLEDGE = "acknowledge.response";
    static final String RESPONSE_ACKNOWLEDGE_RESPONSE_CODE = "acknowledge.responseCode";

    // These values are copied from the Play Billing library since Chrome cannot depend on it.
    // https://developer.android.com/reference/com/android/billingclient/api/BillingClient.BillingResponseCode
    static final int PLAY_BILLING_OK = 0;
    static final int PLAY_BILLING_ITEM_ALREADY_OWNED = 7;
    static final int PLAY_BILLING_ITEM_NOT_OWNED = 8;
    static final int PLAY_BILLING_ITEM_UNAVAILABLE = 4;

    private DigitalGoodsConverter() {}

    /**
     * Converts the parameters to the getDetails.
     */
    static Bundle convertGetDetailsParams(String[] itemIds) {
        Bundle args = new Bundle();
        args.putStringArray(PARAM_GET_DETAILS_ITEM_IDS, itemIds);
        return args;
    }

    /**
     * Produces a {@link TrustedWebActivityCallback} that calls the given
     * {@link GetDetailsResponse}.
     */
    static TrustedWebActivityCallback convertGetDetailsCallback(GetDetailsResponse callback) {
        return new TrustedWebActivityCallback() {
            @Override
            public void onExtraCallback(@NonNull String callbackName, @Nullable Bundle args) {
                if (!RESPONSE_GET_DETAILS.equals(callbackName)) {
                    Log.w(TAG, "Wrong callback name given: " + callbackName + ".");
                    returnClientAppError(callback);
                    return;
                }

                if (args == null) {
                    Log.w(TAG, "No args provided.");
                    returnClientAppError(callback);
                    return;
                }

                if (!(args.get(RESPONSE_GET_DETAILS_RESPONSE_CODE) instanceof Integer)
                        || !(args.get(RESPONSE_GET_DETAILS_DETAILS_LIST) instanceof Parcelable[])) {
                    Log.w(TAG, "Poorly formed args provided.");
                    returnClientAppError(callback);
                    return;
                }

                int code = args.getInt(RESPONSE_GET_DETAILS_RESPONSE_CODE);
                ItemDetails[] details = convertItemDetailsList(
                        args.getParcelableArray(RESPONSE_GET_DETAILS_DETAILS_LIST));
                callback.call(convertResponseCodes(code), details);
            }
        };
    }

    private static ItemDetails[] convertItemDetailsList(Parcelable[] list) {
        List<ItemDetails> details = new ArrayList<>();
        for (Parcelable item: list) {
            details.add(convertItemDetails(item));
        }
        return details.toArray(new ItemDetails[0]);
    }

    /** Extracts an {@link ItemDetails} from a {@link Parcelable}. */
    @Nullable
    static ItemDetails convertItemDetails(Parcelable itemAsParcelable) {
        if (!(itemAsParcelable instanceof Bundle)) {
            Log.w(TAG, "Item is not a Bundle.");
            return null;
        }

        Bundle item = (Bundle) itemAsParcelable;

        for (String field : ITEM_DETAILS_ALL_FIELDS) {
            if (item.containsKey(field) && (item.get(field) instanceof String)) continue;
            Log.w(TAG, "Item does not contain field String " + field + ".");
            return null;
        }

        PaymentCurrencyAmount amount = new PaymentCurrencyAmount();
        amount.currency = item.getString(ITEM_DETAILS_CURRENCY);
        amount.value = item.getString(ITEM_DETAILS_VALUE);

        ItemDetails res = new ItemDetails();
        res.itemId = item.getString(ITEM_DETAILS_ID);
        res.title = item.getString(ITEM_DETAILS_TITLE);
        res.description = item.getString(ITEM_DETAILS_DESC);
        res.price = amount;

        return res;
    }

    static int convertResponseCodes(int responseCode) {
        switch (responseCode) {
            case PLAY_BILLING_OK:
                return BillingResponseCode.OK;
            case PLAY_BILLING_ITEM_ALREADY_OWNED:
                return BillingResponseCode.ITEM_ALREADY_OWNED;
            case PLAY_BILLING_ITEM_NOT_OWNED:
                return BillingResponseCode.ITEM_NOT_OWNED;
            case PLAY_BILLING_ITEM_UNAVAILABLE:
                return BillingResponseCode.ITEM_UNAVAILABLE;
            default:
                Log.w(TAG, "Unexpected response code: " + responseCode);
                return BillingResponseCode.ERROR;
        }
    }

    static Bundle convertAcknowledgeParams(String purchaseToken, boolean makeAvailableAgain) {
        Bundle bundle = new Bundle();
        bundle.putString(PARAM_ACKNOWLEDGE_PURCHASE_TOKEN, purchaseToken);
        bundle.putBoolean(PARAM_ACKNOWLEDGE_MAKE_AVAILABLE_AGAIN, makeAvailableAgain);
        return bundle;
    }

    static TrustedWebActivityCallback convertAcknowledgeCallback(AcknowledgeResponse callback) {
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
                callback.call(convertResponseCodes(code));
            }
        };
    }

    public static void returnClientAppUnavailable(GetDetailsResponse callback) {
        callback.call(BillingResponseCode.CLIENT_APP_UNAVAILABLE,
                new ItemDetails[0]);
    }

    public static void returnClientAppError(GetDetailsResponse callback) {
        callback.call(BillingResponseCode.CLIENT_APP_ERROR,
                new ItemDetails[0]);
    }

    public static void returnClientAppUnavailable(AcknowledgeResponse callback) {
        callback.call(BillingResponseCode.CLIENT_APP_UNAVAILABLE);
    }

    public static void returnClientAppError(AcknowledgeResponse callback) {
        callback.call(BillingResponseCode.CLIENT_APP_ERROR);
    }

    /**
     * Creates a {@link Bundle} that represents an {@link ItemDetails} with the given values.
     * This would be used by the client app and is here only to help testing.
     */
    @VisibleForTesting
    public static Bundle createItemDetailsBundle(String id, String title, String desc,
            String currency, String value) {
        Bundle bundle = new Bundle();

        bundle.putString(ITEM_DETAILS_ID, id);
        bundle.putString(ITEM_DETAILS_TITLE, title);
        bundle.putString(ITEM_DETAILS_DESC, desc);
        bundle.putString(ITEM_DETAILS_CURRENCY, currency);
        bundle.putString(ITEM_DETAILS_VALUE, value);

        return bundle;
    }

    /**
     * Creates a {@link Bundle} that represents the result of a getDetails call. This would be
     * carried out by the client app and is only here to help testing.
     */
    @VisibleForTesting
    public static Bundle createGetDetailsResponseBundle(int responseCode, Bundle... itemDetails) {
        Bundle bundle = new Bundle();

        bundle.putInt(RESPONSE_GET_DETAILS_RESPONSE_CODE, responseCode);
        bundle.putParcelableArray(RESPONSE_GET_DETAILS_DETAILS_LIST, itemDetails);

        return bundle;
    }

    /**
     * Creates a {@link Bundle} that represents the result of an acknowledge call. This would be
     * carried out by the client app and is only here to help testing.
     */
    @VisibleForTesting
    public static Bundle createAcknowledgeResponseBundle(int responseCode) {
        Bundle bundle = new Bundle();

        bundle.putInt(RESPONSE_ACKNOWLEDGE_RESPONSE_CODE, responseCode);

        return bundle;
    }
}

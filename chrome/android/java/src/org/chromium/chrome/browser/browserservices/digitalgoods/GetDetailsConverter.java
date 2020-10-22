// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import static org.chromium.chrome.browser.browserservices.digitalgoods.DigitalGoodsConverter.convertResponseCodes;

import android.os.Bundle;
import android.os.Parcelable;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.trusted.TrustedWebActivityCallback;

import org.chromium.base.Log;
import org.chromium.payments.mojom.BillingResponseCode;
import org.chromium.payments.mojom.DigitalGoods.GetDetailsResponse;
import org.chromium.payments.mojom.ItemDetails;
import org.chromium.payments.mojom.PaymentCurrencyAmount;

import java.util.ArrayList;
import java.util.List;

/**
 * A converter that deals with the parameters and result for GetDetails calls.
 */
public class GetDetailsConverter {
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
    static final String[] ITEM_DETAILS_ALL_FIELDS = {ITEM_DETAILS_ID, ITEM_DETAILS_TITLE,
            ITEM_DETAILS_DESC, ITEM_DETAILS_CURRENCY, ITEM_DETAILS_VALUE};

    private GetDetailsConverter() {}

    /**
     * Converts the parameters to the getDetails.
     */
    static Bundle convertParams(String[] itemIds) {
        Bundle args = new Bundle();
        args.putStringArray(GetDetailsConverter.PARAM_GET_DETAILS_ITEM_IDS, itemIds);
        return args;
    }

    /**
     * Produces a {@link TrustedWebActivityCallback} that calls the given
     * {@link GetDetailsResponse}.
     */
    static TrustedWebActivityCallback convertCallback(GetDetailsResponse callback) {
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
        for (Parcelable item : list) {
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

    public static void returnClientAppUnavailable(GetDetailsResponse callback) {
        callback.call(BillingResponseCode.CLIENT_APP_UNAVAILABLE, new ItemDetails[0]);
    }

    public static void returnClientAppError(GetDetailsResponse callback) {
        callback.call(BillingResponseCode.CLIENT_APP_ERROR, new ItemDetails[0]);
    }

    /**
     * Creates a {@link Bundle} that represents an {@link ItemDetails} with the given values.
     * This would be used by the client app and is here only to help testing.
     */
    @VisibleForTesting
    public static Bundle createItemDetailsBundle(
            String id, String title, String desc, String currency, String value) {
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
    public static Bundle createResponseBundle(int responseCode, Bundle... itemDetails) {
        Bundle bundle = new Bundle();

        bundle.putInt(RESPONSE_GET_DETAILS_RESPONSE_CODE, responseCode);
        bundle.putParcelableArray(RESPONSE_GET_DETAILS_DETAILS_LIST, itemDetails);

        return bundle;
    }
}

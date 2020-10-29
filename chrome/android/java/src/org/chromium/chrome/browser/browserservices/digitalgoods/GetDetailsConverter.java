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

    static final String KEY_ID = "itemDetails.id";
    static final String KEY_TITLE = "itemDetails.title";
    static final String KEY_DESC = "itemDetails.description";
    static final String KEY_CURRENCY = "itemDetails.currency";
    static final String KEY_VALUE = "itemDetails.value";
    static final String[] REQUIRED_FIELDS = {KEY_ID, KEY_TITLE, KEY_DESC, KEY_CURRENCY, KEY_VALUE};

    static final String KEY_SUBS_PERIOD = "itemDetails.subsPeriod";
    static final String KEY_FREE_TRIAL_PERIOD = "itemDetails.freeTrialPeriod";
    static final String KEY_INTRO_CURRENCY = "itemDetails.introPriceCurrency";
    static final String KEY_INTRO_VALUE = "itemDetails.introPriceValue";
    static final String KEY_INTRO_PERIOD = "itemDetails.introPricePeriod";

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

        for (String field : REQUIRED_FIELDS) {
            if (item.containsKey(field) && (item.get(field) instanceof String)) continue;
            Log.w(TAG, "Item does not contain field String " + field + ".");
            return null;
        }

        // Mandatory fields.
        PaymentCurrencyAmount price = new PaymentCurrencyAmount();
        price.currency = item.getString(KEY_CURRENCY);
        price.value = item.getString(KEY_VALUE);

        ItemDetails result = new ItemDetails();
        result.itemId = item.getString(KEY_ID);
        result.title = item.getString(KEY_TITLE);
        result.description = item.getString(KEY_DESC);
        result.price = price;

        // Optional fields.
        result.subscriptionPeriod = item.getString(KEY_SUBS_PERIOD);
        result.freeTrialPeriod = item.getString(KEY_FREE_TRIAL_PERIOD);
        result.introductoryPricePeriod = item.getString(KEY_INTRO_PERIOD);

        String introPriceCurrency = item.getString(KEY_INTRO_CURRENCY);
        String introPriceValue = item.getString(KEY_INTRO_VALUE);

        if (introPriceCurrency != null && introPriceValue != null) {
            PaymentCurrencyAmount introPrice = new PaymentCurrencyAmount();
            introPrice.currency = introPriceCurrency;
            introPrice.value = introPriceValue;
            result.introductoryPrice = introPrice;
        }

        return result;
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
    public static Bundle createItemDetailsBundle(String id, String title, String desc,
            String currency, String value, @Nullable String subsPeriod,
            @Nullable String freeTrialPeriod, @Nullable String introPriceCurrency,
            @Nullable String introPriceValue, @Nullable String intoPricePeriod) {
        Bundle bundle = createItemDetailsBundle(id, title, desc, currency, value);

        bundle.putString(KEY_SUBS_PERIOD, subsPeriod);
        bundle.putString(KEY_FREE_TRIAL_PERIOD, freeTrialPeriod);
        bundle.putString(KEY_INTRO_CURRENCY, introPriceCurrency);
        bundle.putString(KEY_INTRO_VALUE, introPriceValue);
        bundle.putString(KEY_INTRO_PERIOD, intoPricePeriod);

        return bundle;
    }

    /**
     * Like the above method, but provides {@code null} for all optional parameters.
     */
    @VisibleForTesting
    public static Bundle createItemDetailsBundle(
            String id, String title, String desc, String currency, String value) {
        Bundle bundle = new Bundle();

        bundle.putString(KEY_ID, id);
        bundle.putString(KEY_TITLE, title);
        bundle.putString(KEY_DESC, desc);
        bundle.putString(KEY_CURRENCY, currency);
        bundle.putString(KEY_VALUE, value);

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

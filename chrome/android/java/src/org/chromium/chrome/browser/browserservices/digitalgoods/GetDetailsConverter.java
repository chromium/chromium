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
import org.chromium.payments.mojom.DigitalGoods.GetDetails_Response;
import org.chromium.payments.mojom.ItemDetails;
import org.chromium.payments.mojom.ItemType;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.url.mojom.Url;

/** A converter that deals with the parameters and result for GetDetails calls. */
public class GetDetailsConverter {
    private static final String TAG = "DigitalGoods";

    static final String PARAM_GET_DETAILS_ITEM_IDS = "getDetails.itemIds";
    public static final String RESPONSE_COMMAND = "getDetails.response";
    static final String KEY_RESPONSE_CODE = "getDetails.responseCode";
    static final String KEY_DETAILS_LIST = "getDetails.detailsList";

    static final String KEY_ID = "itemDetails.id";
    static final String KEY_TITLE = "itemDetails.title";
    static final String KEY_DESC = "itemDetails.description";
    static final String KEY_CURRENCY = "itemDetails.currency";
    static final String KEY_VALUE = "itemDetails.value";
    static final String[] REQUIRED_FIELDS = {KEY_ID, KEY_TITLE, KEY_DESC, KEY_CURRENCY, KEY_VALUE};

    static final String KEY_TYPE = "itemDetails.type";
    static final String KEY_ICON_URL = "itemDetails.url";

    static final String KEY_SUBS_PERIOD = "itemDetails.subsPeriod";
    static final String KEY_FREE_TRIAL_PERIOD = "itemDetails.freeTrialPeriod";
    static final String KEY_INTRO_CURRENCY = "itemDetails.introPriceCurrency";
    static final String KEY_INTRO_VALUE = "itemDetails.introPriceValue";
    static final String KEY_INTRO_PERIOD = "itemDetails.introPricePeriod";
    static final String KEY_INTRO_CYCLES = "itemDetails.introPriceCycles";

    static final String ITEM_TYPE_SUBS = "subs";
    static final String ITEM_TYPE_INAPP = "inapp";

    private GetDetailsConverter() {}

    /** Converts the parameters to the getDetails. */
    static Bundle convertParams(String[] itemIds) {
        Bundle args = new Bundle();
        args.putStringArray(GetDetailsConverter.PARAM_GET_DETAILS_ITEM_IDS, itemIds);
        return args;
    }

    /**
     * Produces a {@link TrustedWebActivityCallback} that calls the given
     * {@link GetDetailsResponse}.
     */
    static TrustedWebActivityCallback convertCallback(GetDetails_Response callback) {
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
                        || !checkField(args, KEY_DETAILS_LIST, Parcelable[].class)) {
                    returnClientAppError(callback);
                    return;
                }

                int code = args.getInt(KEY_RESPONSE_CODE);
                Parcelable[] array = args.getParcelableArray(KEY_DETAILS_LIST);

                ItemDetails[] details =
                        convertParcelableArray(array, GetDetailsConverter::convertItemDetails)
                                .toArray(new ItemDetails[0]);
                callback.call(convertResponseCode(code, args), details);
            }
        };
    }

    /** Extracts an {@link ItemDetails} from a {@link Parcelable}. */
    static @Nullable ItemDetails convertItemDetails(Bundle item) {
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
        result.type = convertItemType(item.getString(KEY_TYPE));
        String iconUrl = item.getString(KEY_ICON_URL);
        if (iconUrl != null) {
            org.chromium.url.mojom.Url url = new Url();
            url.url = iconUrl;
            result.iconUrls = new Url[] {url};
        } else {
            result.iconUrls = new Url[0];
        }

        result.subscriptionPeriod = item.getString(KEY_SUBS_PERIOD);
        result.freeTrialPeriod = item.getString(KEY_FREE_TRIAL_PERIOD);
        result.introductoryPricePeriod = item.getString(KEY_INTRO_PERIOD);
        result.introductoryPriceCycles = item.getInt(KEY_INTRO_CYCLES, 0);

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

    static int convertItemType(@Nullable String itemType) {
        // TODO: introduce our own API here.
        if (ITEM_TYPE_SUBS.equals(itemType)) {
            return ItemType.SUBSCRIPTION;
        } else if (ITEM_TYPE_INAPP.equals(itemType)) {
            return ItemType.PRODUCT;
        }

        return ItemType.UNKNOWN;
    }

    static void returnClientAppUnavailable(GetDetails_Response callback) {
        callback.call(BillingResponseCode.CLIENT_APP_UNAVAILABLE, new ItemDetails[0]);
    }

    static void returnClientAppError(GetDetails_Response callback) {
        callback.call(BillingResponseCode.CLIENT_APP_ERROR, new ItemDetails[0]);
    }

    /**
     * Creates a {@link Bundle} that represents an {@link ItemDetails} with the given values.
     * This would be used by the client app and is here only to help testing.
     */
    @VisibleForTesting
    static Bundle createItemDetailsBundle(
            String id,
            String title,
            String desc,
            String currency,
            String value,
            String type,
            String iconUrl,
            @Nullable String subsPeriod,
            @Nullable String freeTrialPeriod,
            @Nullable String introPriceCurrency,
            @Nullable String introPriceValue,
            @Nullable String intoPricePeriod,
            int introPriceCycles) {
        Bundle bundle = createItemDetailsBundle(id, title, desc, currency, value);

        bundle.putString(KEY_TYPE, type);
        bundle.putString(KEY_ICON_URL, iconUrl);

        bundle.putString(KEY_SUBS_PERIOD, subsPeriod);
        bundle.putString(KEY_FREE_TRIAL_PERIOD, freeTrialPeriod);
        bundle.putString(KEY_INTRO_CURRENCY, introPriceCurrency);
        bundle.putString(KEY_INTRO_VALUE, introPriceValue);
        bundle.putString(KEY_INTRO_PERIOD, intoPricePeriod);
        bundle.putInt(KEY_INTRO_CYCLES, introPriceCycles);

        return bundle;
    }

    /** Like the above method, but provides {@code null} for all optional parameters. */
    @VisibleForTesting
    static Bundle createItemDetailsBundle(
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
    static Bundle createResponseBundle(int responseCode, Bundle... itemDetails) {
        Bundle bundle = new Bundle();

        bundle.putInt(KEY_RESPONSE_CODE, responseCode);
        bundle.putParcelableArray(KEY_DETAILS_LIST, itemDetails);

        return bundle;
    }
}

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_annotations;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.build.annotations.DoNotClassMerge;

import java.util.Locale;

/**
 * {@link PageAnnotation} for products in a page.
 *
 * This class should not be merged because it is being used as a key in a Map
 * in PageAnnotationUtils.java.
 */
@DoNotClassMerge
public class BuyableProductPageAnnotation extends PageAnnotation {
    private static final String TAG = "BPPA";
    private static final String BUYABLE_PRODUCT_KEY = "buyableProduct";
    private static final String CURRENT_PRICE_KEY = "currentPrice";
    private static final String CURRENCY_CODE_KEY = "currencyCode";
    private static final String AMOUNT_MICROS_KEY = "amountMicros";
    private static final String OFFER_ID_KEY = "offerId";

    private final long mPriceMicros;
    private final String mCurrencyCode;
    private final String mOfferId;

    /** Creates a new instance. */
    public BuyableProductPageAnnotation(long priceMicros, String currencyCode, String offerId) {
        super(PageAnnotationType.BUYABLE_PRODUCT);
        mPriceMicros = priceMicros;
        mCurrencyCode = currencyCode;
        mOfferId = offerId;
    }

    /** Gets the current price amount in micros. */
    public long getCurrentPriceMicros() {
        return mPriceMicros;
    }

    /** Gets the currency code used for the price. */
    public String getCurrencyCode() {
        return mCurrencyCode;
    }

    /** Gets the offer id. */
    public String getOfferId() {
        return mOfferId;
    }

    /** Creates a new {@link BuyableProductPageAnnotation} from a {@link JSONObject}. */
    public static BuyableProductPageAnnotation fromJson(JSONObject object) {
        try {
            JSONObject metadata = object.getJSONObject(BUYABLE_PRODUCT_KEY);
            JSONObject priceMetadata = metadata.getJSONObject(CURRENT_PRICE_KEY);
            if (priceMetadata == null || !priceMetadata.has(AMOUNT_MICROS_KEY)
                    || priceMetadata.isNull(AMOUNT_MICROS_KEY)) {
                Log.i(TAG, String.format(Locale.US, "Invalid price info."));
                return null;
            }

            Long priceAmountMicros =
                    PageAnnotationUtils.safeParseLong(priceMetadata.getString(AMOUNT_MICROS_KEY));

            if (priceAmountMicros == null) {
                Log.i(TAG, String.format(Locale.US, "Invalid price micros."));
                return null;
            }

            return new BuyableProductPageAnnotation(priceAmountMicros,
                    priceMetadata.getString(CURRENCY_CODE_KEY), metadata.getString(OFFER_ID_KEY));
        } catch (JSONException e) {
            Log.i(TAG,
                    String.format(Locale.US,
                            "There was a problem parsing "
                                    + "BuyableProductPageAnnotation "
                                    + "Details: %s",
                            e.toString()));
        }

        return null;
    }
}

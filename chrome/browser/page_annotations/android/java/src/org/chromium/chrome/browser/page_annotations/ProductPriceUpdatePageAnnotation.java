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
 * {@link PageAnnotation} for product price updates in a page.
 *
 * This class should not be merged because it is being used as a key in a Map
 * in PageAnnotationUtils.java.
 */
@DoNotClassMerge
public class ProductPriceUpdatePageAnnotation extends PageAnnotation {
    private static final String TAG = "PPUPA";
    private static final String PRICE_UPDATE_KEY = "priceUpdate";
    private static final String OLD_PRICE_KEY = "oldPrice";
    private static final String NEW_PRICE_KEY = "newPrice";
    private static final String CURRENCY_CODE_KEY = "currencyCode";
    private static final String AMOUNT_MICROS_KEY = "amountMicros";

    private final long mOldPriceMicros;
    private final long mNewPriceMicros;
    private final String mCurrencyCode;

    /** Creates a new instance. */
    public ProductPriceUpdatePageAnnotation(
            long oldPriceMicros, long newPriceMicros, String currencyCode) {
        super(PageAnnotationType.PRODUCT_PRICE_UPDATE);
        mOldPriceMicros = oldPriceMicros;
        mNewPriceMicros = newPriceMicros;
        mCurrencyCode = currencyCode;
    }

    /** Gets the old price amount in micros. */
    public long getOldPriceMicros() {
        return mOldPriceMicros;
    }

    /** Gets the new price amount in micros. */
    public long getNewPriceMicros() {
        return mNewPriceMicros;
    }

    /** Gets the currency code for the price update. */
    public String getCurrencyCode() {
        return mCurrencyCode;
    }

    /** Creates a new {@link ProductPriceUpdatePageAnnotation} from a {@link JSONObject}. */
    public static ProductPriceUpdatePageAnnotation fromJson(JSONObject object) {
        try {
            JSONObject priceUpdateData = object.getJSONObject(PRICE_UPDATE_KEY);
            JSONObject oldPrice = priceUpdateData.getJSONObject(OLD_PRICE_KEY);
            JSONObject newPrice = priceUpdateData.getJSONObject(NEW_PRICE_KEY);

            if (!isValidPriceJsonObject(oldPrice) || !isValidPriceJsonObject(newPrice)) {
                Log.i(TAG, String.format(Locale.US, "Invalid price update."));
                return null;
            }

            String oldCurrencyCode = oldPrice.getString(CURRENCY_CODE_KEY);
            String newCurrencyCode = newPrice.getString(CURRENCY_CODE_KEY);

            if (oldCurrencyCode == null || !oldCurrencyCode.equals(newCurrencyCode)) {
                Log.i(TAG,
                        String.format(
                                Locale.US, "There was currency code mismatch in price update."));
                return null;
            }

            Long oldAmountMicros =
                    PageAnnotationUtils.safeParseLong(oldPrice.getString(AMOUNT_MICROS_KEY));
            Long newAmountMicros =
                    PageAnnotationUtils.safeParseLong(newPrice.getString(AMOUNT_MICROS_KEY));

            if (oldAmountMicros == null || newAmountMicros == null) {
                Log.i(TAG, String.format(Locale.US, "Invalid amount micros."));
                return null;
            }

            return new ProductPriceUpdatePageAnnotation(
                    oldAmountMicros, newAmountMicros, oldCurrencyCode);

        } catch (JSONException e) {
            Log.i(TAG,
                    String.format(Locale.US,
                            "There was a problem parsing "
                                    + "ProductPriceUpdatePageAnnotation "
                                    + "Details: %s",
                            e.toString()));
        }

        return null;
    }

    private static boolean isValidPriceJsonObject(JSONObject json) {
        return json != null && json.has(CURRENCY_CODE_KEY) && !json.isNull(CURRENCY_CODE_KEY)
                && json.has(AMOUNT_MICROS_KEY) && !json.isNull(AMOUNT_MICROS_KEY);
    }
}

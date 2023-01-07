// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_annotations;

import androidx.annotation.Nullable;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.chrome.browser.page_annotations.PageAnnotation.PageAnnotationType;

/** Utility functions used in page annotations unit tests. */
class PageAnnotationsTestUtils {
    private static final String BUYABLE_PRODUCT_KEY = "buyableProduct";
    private static final String CURRENT_PRICE_KEY = "currentPrice";
    private static final String CURRENCY_CODE_KEY = "currencyCode";
    private static final String AMOUNT_MICROS_KEY = "amountMicros";
    private static final String TYPE_KEY = "type";
    private static final String PRICE_UPDATE_KEY = "priceUpdate";
    private static final String OLD_PRICE_KEY = "oldPrice";
    private static final String NEW_PRICE_KEY = "newPrice";
    private static final String OFFER_ID_KEY = "offerId";

    static JSONObject createEmptyBuyableProduct() throws JSONException {
        return createFakeBuyableProductJson(false, null, null, null);
    }

    static JSONObject createFakeBuyableProductJson(boolean includePriceMetadata,
            @Nullable String priceInMicros, @Nullable String currencyCode, @Nullable String offerId)
            throws JSONException {
        JSONObject root = new JSONObject();
        JSONObject buyableProductJson = new JSONObject();

        if (includePriceMetadata) {
            JSONObject priceMetadata = new JSONObject();
            if (priceInMicros != null) {
                priceMetadata.put(AMOUNT_MICROS_KEY, priceInMicros);
            }

            if (currencyCode != null) {
                priceMetadata.put(CURRENCY_CODE_KEY, currencyCode);
            }

            buyableProductJson.put(CURRENT_PRICE_KEY, priceMetadata);
        }

        if (offerId != null) {
            buyableProductJson.put(OFFER_ID_KEY, offerId);
        }

        root.put(BUYABLE_PRODUCT_KEY, buyableProductJson);
        root.put(TYPE_KEY, PageAnnotationType.BUYABLE_PRODUCT);
        return root;
    }

    static JSONObject createEmptyProductPriceUpdate() throws JSONException {
        return createFakeProductPriceUpdate(null, null, null, null);
    }

    static JSONObject createFakeProductPriceUpdate(@Nullable String oldPriceMicros,
            @Nullable String oldCurrencyCode, @Nullable String newPriceMicros,
            @Nullable String newCurrencyCode) throws JSONException {
        JSONObject root = new JSONObject();
        JSONObject productPriceUpdate = new JSONObject();

        JSONObject oldPrice = new JSONObject();
        if (oldPriceMicros != null) {
            oldPrice.put(AMOUNT_MICROS_KEY, oldPriceMicros);
        }

        if (oldCurrencyCode != null) {
            oldPrice.put(CURRENCY_CODE_KEY, oldCurrencyCode);
        }

        JSONObject newPrice = new JSONObject();
        if (newPriceMicros != null) {
            newPrice.put(AMOUNT_MICROS_KEY, newPriceMicros);
        }

        if (newCurrencyCode != null) {
            newPrice.put(CURRENCY_CODE_KEY, newCurrencyCode);
        }

        productPriceUpdate.put(OLD_PRICE_KEY, oldPrice);
        productPriceUpdate.put(NEW_PRICE_KEY, newPrice);

        root.put(PRICE_UPDATE_KEY, productPriceUpdate);
        root.put(TYPE_KEY, PageAnnotationType.PRODUCT_PRICE_UPDATE);
        return root;
    }

    static JSONObject createDummyPageAnnotationJson(String type) throws JSONException {
        JSONObject root = new JSONObject();
        root.put("type", type);
        return root;
    }
}

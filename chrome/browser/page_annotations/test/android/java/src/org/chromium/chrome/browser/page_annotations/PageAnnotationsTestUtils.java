// Copyright 2021 The Chromium Authors. All rights reserved.
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

    static JSONObject createFakeBuyableProductJson() throws JSONException {
        return createFakeBuyableProductJson(false, null, null);
    }

    static JSONObject createFakeBuyableProductJson(boolean includePriceMetadata,
            @Nullable Long priceInMicros, @Nullable String currencyCode) throws JSONException {
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

        root.put(BUYABLE_PRODUCT_KEY, buyableProductJson);
        root.put(TYPE_KEY, PageAnnotationType.BUYABLE_PRODUCT);
        return root;
    }

    static JSONObject createDummyPageAnnotationJson(String type) throws JSONException {
        JSONObject root = new JSONObject();
        root.put("type", type);
        return root;
    }
}

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_annotations;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.chrome.browser.page_annotations.PageAnnotation.PageAnnotationType;

import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * A collection of helper functions for dealing with {@link PageAnnotation} instances.
 */
public class PageAnnotationUtils {
    private static final String TAG = "PAU";
    private static final String TYPE_KEY = "type";

    private static final Map<Class<? extends PageAnnotation>, String> CLASS_TO_TYPE_MAP =
            new LinkedHashMap<Class<? extends PageAnnotation>, String>() {
                { put(BuyableProductPageAnnotation.class, PageAnnotationType.BUYABLE_PRODUCT); }
                {
                    put(ProductPriceUpdatePageAnnotation.class,
                            PageAnnotationType.PRODUCT_PRICE_UPDATE);
                }
            };

    /**
     * Creates a {@link PageAnnotation} object from the provided {@link JSONObject}.
     */
    public static PageAnnotation createPageAnnotationFromJson(JSONObject json) {
        try {
            @PageAnnotationType
            String type = json.getString(TYPE_KEY);
            if (type == null) {
                type = PageAnnotationType.UNKNOWN;
            }

            switch (type) {
                case PageAnnotationType.BUYABLE_PRODUCT: {
                    return BuyableProductPageAnnotation.fromJson(json);
                }

                case PageAnnotationType.PRODUCT_PRICE_UPDATE: {
                    return ProductPriceUpdatePageAnnotation.fromJson(json);
                }

                case PageAnnotationType.UNKNOWN:
                default:
                    break;
            }

        } catch (JSONException e) {
            Log.i(TAG,
                    String.format(Locale.US,
                            "Failed to parse PageAnnotation."
                                    + "Details: %s",
                            e.toString()));
        }

        return null;
    }

    /**
     * @return the first {@link PageAnnotation} object that matches the provided
     * type from a list of generic {@link PageAnnotation} objects
     */
    public static <T extends PageAnnotation> T getAnnotation(
            List<PageAnnotation> annotations, Class<T> clazz) {
        if (annotations == null || annotations.size() == 0) {
            return null;
        }

        @PageAnnotationType
        String targetType = CLASS_TO_TYPE_MAP.get(clazz);
        if (targetType == null) {
            return null;
        }

        for (PageAnnotation annotation : annotations) {
            if (targetType.equals(annotation.getType())) {
                return (T) annotation;
            }
        }

        return null;
    }

    /** @return a {@link Long} from the provided string or null in case of an unprasable input. */
    static Long safeParseLong(String number) {
        try {
            return Long.parseLong(number);
        } catch (NumberFormatException e) {
            return null;
        }
    }
}

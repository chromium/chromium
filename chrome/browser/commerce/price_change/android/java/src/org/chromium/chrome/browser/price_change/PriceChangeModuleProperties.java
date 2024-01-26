// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_change;

import android.graphics.Bitmap;
import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties for the price change module. */
interface PriceChangeModuleProperties {
    PropertyModel.WritableObjectPropertyKey<String> MODULE_TITLE =
            new PropertyModel.WritableObjectPropertyKey<>();
    PropertyModel.WritableObjectPropertyKey<Bitmap> MODULE_FAVICON_BITMAP =
            new PropertyModel.WritableObjectPropertyKey<>();
    PropertyModel.WritableObjectPropertyKey<String> MODULE_PRODUCT_NAME_STRING =
            new PropertyModel.WritableObjectPropertyKey<>();
    PropertyModel.WritableObjectPropertyKey<String> MODULE_DOMAIN_STRING =
            new PropertyModel.WritableObjectPropertyKey<>();
    PropertyModel.WritableObjectPropertyKey<String> MODULE_PREVIOUS_PRICE_STRING =
            new PropertyModel.WritableObjectPropertyKey<>();
    PropertyModel.WritableObjectPropertyKey<String> MODULE_CURRENT_PRICE_STRING =
            new PropertyModel.WritableObjectPropertyKey<>();
    PropertyModel.WritableObjectPropertyKey<Bitmap> MODULE_PRODUCT_IMAGE_BITMAP =
            new PropertyModel.WritableObjectPropertyKey<>();
    PropertyModel.WritableObjectPropertyKey<OnClickListener> MODULE_ON_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    PropertyModel.WritableObjectPropertyKey<String> MODULE_ACCESSIBILITY_LABEL =
            new PropertyModel.WritableObjectPropertyKey<>();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                MODULE_TITLE,
                MODULE_FAVICON_BITMAP,
                MODULE_PRODUCT_NAME_STRING,
                MODULE_DOMAIN_STRING,
                MODULE_PREVIOUS_PRICE_STRING,
                MODULE_CURRENT_PRICE_STRING,
                MODULE_PRODUCT_IMAGE_BITMAP,
                MODULE_ON_CLICK_LISTENER,
                MODULE_ACCESSIBILITY_LABEL,
            };
}

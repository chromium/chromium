// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce.coupons;

import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for discounts bottom sheet content. */
public class DiscountsBottomSheetContentProperties {

    public static final WritableObjectPropertyKey<String> DISCOUNT_CODE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> DESCRIPTION_DETAIL =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> EXPIRY_TIME =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> COPY_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<OnClickListener> COPY_BUTTON_ON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                DISCOUNT_CODE,
                DESCRIPTION_DETAIL,
                EXPIRY_TIME,
                COPY_BUTTON_TEXT,
                COPY_BUTTON_ON_CLICK_LISTENER
            };
}

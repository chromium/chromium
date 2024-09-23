// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce.coupons;

import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.COPY_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.COPY_BUTTON_TEXT;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.DESCRIPTION_DETAIL;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.DISCOUNT_CODE;
import static org.chromium.chrome.browser.commerce.coupons.DiscountsBottomSheetContentProperties.EXPIRY_TIME;

import android.view.View;
import android.widget.TextView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/** ViewBinder for the discounts bottom sheet content. */
public class DiscountsBottomSheetContentViewBinder {

    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        ButtonCompat copyButton = (ButtonCompat) view.findViewById(R.id.copy_button);
        if (COPY_BUTTON_TEXT == propertyKey) {
            copyButton.setText(model.get(COPY_BUTTON_TEXT));
        } else if (COPY_BUTTON_ON_CLICK_LISTENER == propertyKey) {
            copyButton.setOnClickListener(model.get(COPY_BUTTON_ON_CLICK_LISTENER));
        } else if (DISCOUNT_CODE == propertyKey) {
            ((TextView) view.findViewById(R.id.discount_code)).setText(model.get(DISCOUNT_CODE));
        } else if (DESCRIPTION_DETAIL == propertyKey) {
            ((TextView) view.findViewById(R.id.description_detail))
                    .setText(model.get(DESCRIPTION_DETAIL));
        } else if (EXPIRY_TIME == propertyKey) {
            ((TextView) view.findViewById(R.id.expiry_time)).setText(model.get(EXPIRY_TIME));
        }
    }
}

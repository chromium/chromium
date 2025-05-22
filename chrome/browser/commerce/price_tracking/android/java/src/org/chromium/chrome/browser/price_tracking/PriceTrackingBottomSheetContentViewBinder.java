// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_BUTTON_BACKGROUND_COLOR;
import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_BUTTON_FOREGROUND_COLOR;
import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_BUTTON_ICON;
import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_BUTTON_TEXT;
import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_TITLE;

import android.content.res.ColorStateList;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for the price tracking bottom sheet content. */
@NullMarked
public class PriceTrackingBottomSheetContentViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        View priceTrackingButton = view.findViewById(R.id.price_tracking_button);
        TextView priceTrackingButtonText =
                priceTrackingButton.findViewById(R.id.price_tracking_button_text);
        ImageView priceTrackingButtonIcon =
                priceTrackingButton.findViewById(R.id.price_tracking_button_icon);
        if (PRICE_TRACKING_TITLE == propertyKey) {
            ((TextView) view.findViewById(R.id.price_tracking_title))
                    .setText(model.get(PRICE_TRACKING_TITLE));
        } else if (PRICE_TRACKING_BUTTON_TEXT == propertyKey) {
            priceTrackingButtonText.setText(model.get(PRICE_TRACKING_BUTTON_TEXT));
        } else if (PRICE_TRACKING_BUTTON_ICON == propertyKey) {
            priceTrackingButtonIcon.setImageResource(model.get(PRICE_TRACKING_BUTTON_ICON));
        } else if (PRICE_TRACKING_BUTTON_FOREGROUND_COLOR == propertyKey) {
            priceTrackingButtonText.setTextColor(model.get(PRICE_TRACKING_BUTTON_FOREGROUND_COLOR));
            ImageViewCompat.setImageTintList(
                    priceTrackingButtonIcon,
                    ColorStateList.valueOf(model.get(PRICE_TRACKING_BUTTON_FOREGROUND_COLOR)));
        } else if (PRICE_TRACKING_BUTTON_BACKGROUND_COLOR == propertyKey) {
            ViewCompat.setBackgroundTintList(
                    priceTrackingButton,
                    ColorStateList.valueOf(model.get(PRICE_TRACKING_BUTTON_BACKGROUND_COLOR)));
        } else if (PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER == propertyKey) {
            ViewCompat.replaceAccessibilityAction(
                    priceTrackingButton,
                    AccessibilityActionCompat.ACTION_CLICK,
                    priceTrackingButton
                            .getContext()
                            .getString(
                                    R.string
                                            .price_insights_content_price_tracking_button_action_description),
                    null);
            priceTrackingButton.setOnClickListener(
                    model.get(PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER));
        }
    }
}

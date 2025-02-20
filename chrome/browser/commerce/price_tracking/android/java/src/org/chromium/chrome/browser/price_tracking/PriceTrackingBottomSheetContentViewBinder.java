// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_BACKGROUND_COLOR;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ENABLED;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_FOREGROUND_COLOR;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ICON;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_TEXT;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_TITLE;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.view.View;
import android.widget.TextView;

import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat;
import androidx.core.widget.TextViewCompat;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/** ViewBinder for the price tracking bottom sheet content. */
public class PriceTrackingBottomSheetContentViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        ButtonCompat priceTrackingButton =
                (ButtonCompat) view.findViewById(R.id.price_tracking_button);
        if (PRICE_TRACKING_TITLE == propertyKey) {
            ((TextView) view.findViewById(R.id.price_tracking_title))
                    .setText(model.get(PRICE_TRACKING_TITLE));
        } else if (PRICE_TRACKING_BUTTON_TEXT == propertyKey) {
            priceTrackingButton.setText(model.get(PRICE_TRACKING_BUTTON_TEXT));
        } else if (PRICE_TRACKING_BUTTON_ICON == propertyKey) {
            // Set price tracking button icon at the start position of the button.
            priceTrackingButton.setCompoundDrawablesRelativeWithIntrinsicBounds(
                    /* start= */ model.get(PRICE_TRACKING_BUTTON_ICON),
                    /* top= */ Resources.ID_NULL,
                    /* end= */ Resources.ID_NULL,
                    /* bottom= */ Resources.ID_NULL);
        } else if (PRICE_TRACKING_BUTTON_FOREGROUND_COLOR == propertyKey) {
            priceTrackingButton.setTextColor(model.get(PRICE_TRACKING_BUTTON_FOREGROUND_COLOR));
            TextViewCompat.setCompoundDrawableTintList(
                    priceTrackingButton,
                    ColorStateList.valueOf(model.get(PRICE_TRACKING_BUTTON_FOREGROUND_COLOR)));
        } else if (PRICE_TRACKING_BUTTON_BACKGROUND_COLOR == propertyKey) {
            ViewCompat.setBackgroundTintList(
                    priceTrackingButton,
                    ColorStateList.valueOf(model.get(PRICE_TRACKING_BUTTON_BACKGROUND_COLOR)));
        } else if (PRICE_TRACKING_BUTTON_ENABLED == propertyKey) {
            priceTrackingButton.setEnabled(model.get(PRICE_TRACKING_BUTTON_ENABLED));
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

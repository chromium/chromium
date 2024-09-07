// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.OPEN_URL_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.OPEN_URL_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_HISTORY_CHART;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_HISTORY_DESCRIPTION;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_HISTORY_TITLE;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_BACKGROUND_COLOR;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ENABLED;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_FOREGROUND_COLOR;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ICON;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_TEXT;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_TITLE;

import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;
import androidx.core.view.ViewCompat;
import androidx.core.widget.TextViewCompat;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/** ViewBinder for the price insights bottom sheet */
public class PriceInsightsBottomSheetViewBinder {

    private static final int PRICE_HISTORY_CHART_ID = View.generateViewId();

    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        ButtonCompat priceTrackingButton =
                (ButtonCompat) view.findViewById(R.id.price_tracking_button);
        TextView openUrlButton = (TextView) view.findViewById(R.id.open_jackpot_url_button);
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
            priceTrackingButton.setTextColor(
                    ContextCompat.getColor(
                            priceTrackingButton.getContext(),
                            model.get(PRICE_TRACKING_BUTTON_FOREGROUND_COLOR)));
            TextViewCompat.setCompoundDrawableTintList(
                    priceTrackingButton,
                    AppCompatResources.getColorStateList(
                            priceTrackingButton.getContext(),
                            model.get(PRICE_TRACKING_BUTTON_FOREGROUND_COLOR)));
        } else if (PRICE_TRACKING_BUTTON_BACKGROUND_COLOR == propertyKey) {
            ViewCompat.setBackgroundTintList(
                    priceTrackingButton,
                    AppCompatResources.getColorStateList(
                            priceTrackingButton.getContext(),
                            model.get(PRICE_TRACKING_BUTTON_BACKGROUND_COLOR)));
        } else if (PRICE_TRACKING_BUTTON_ENABLED == propertyKey) {
            priceTrackingButton.setEnabled(model.get(PRICE_TRACKING_BUTTON_ENABLED));
        } else if (PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER == propertyKey) {
            priceTrackingButton.setOnClickListener(
                    model.get(PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER));
        } else if (PRICE_HISTORY_TITLE == propertyKey) {
            ((TextView) view.findViewById(R.id.price_history_title))
                    .setText(model.get(PRICE_HISTORY_TITLE));
        } else if (PRICE_HISTORY_DESCRIPTION == propertyKey) {
            ((TextView) view.findViewById(R.id.price_history_description))
                    .setText(model.get(PRICE_HISTORY_DESCRIPTION));
        } else if (PRICE_HISTORY_CHART == propertyKey) {
            LinearLayout priceHistoryLayout =
                    (LinearLayout) view.findViewById(R.id.price_history_layout);
            View previousChart = priceHistoryLayout.findViewById(PRICE_HISTORY_CHART_ID);
            if (previousChart != null) {
                priceHistoryLayout.removeView(previousChart);
            }
            View priceHistoryChart = model.get(PRICE_HISTORY_CHART);
            priceHistoryChart.setId(PRICE_HISTORY_CHART_ID);
            int chartHeight =
                    priceHistoryChart
                            .getContext()
                            .getResources()
                            .getDimensionPixelSize(R.dimen.price_history_chart_height);
            priceHistoryChart.setLayoutParams(
                    new LayoutParams(LayoutParams.MATCH_PARENT, chartHeight));
            priceHistoryLayout.addView(priceHistoryChart);
        } else if (OPEN_URL_BUTTON_VISIBLE == propertyKey) {
            openUrlButton.setVisibility(
                    model.get(OPEN_URL_BUTTON_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (OPEN_URL_BUTTON_ON_CLICK_LISTENER == propertyKey) {
            openUrlButton.setOnClickListener(model.get(OPEN_URL_BUTTON_ON_CLICK_LISTENER));
        }
    }
}

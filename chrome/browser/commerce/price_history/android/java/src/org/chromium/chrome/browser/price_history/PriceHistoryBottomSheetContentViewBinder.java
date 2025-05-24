// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_history;

import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.OPEN_URL_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.OPEN_URL_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.PRICE_HISTORY_CHART;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.PRICE_HISTORY_CHART_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.PRICE_HISTORY_DESCRIPTION;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.PRICE_HISTORY_DESCRIPTION_VISIBLE;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.PRICE_HISTORY_TITLE;

import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for the price history bottom sheet content. */
@NullMarked
public class PriceHistoryBottomSheetContentViewBinder {

    private static final int PRICE_HISTORY_CHART_ID = View.generateViewId();

    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView priceHistoryDescription = view.findViewById(R.id.price_history_description);
        TextView openUrlButton = view.findViewById(R.id.open_jackpot_url_button);
        if (PRICE_HISTORY_TITLE == propertyKey) {
            ((TextView) view.findViewById(R.id.price_history_title))
                    .setText(model.get(PRICE_HISTORY_TITLE));
        } else if (PRICE_HISTORY_DESCRIPTION == propertyKey) {
            priceHistoryDescription.setText(model.get(PRICE_HISTORY_DESCRIPTION));
        } else if (PRICE_HISTORY_DESCRIPTION_VISIBLE == propertyKey) {
            priceHistoryDescription.setVisibility(
                    model.get(PRICE_HISTORY_DESCRIPTION_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (PRICE_HISTORY_CHART == propertyKey) {
            LinearLayout priceHistoryLayout = view.findViewById(R.id.price_history_layout);
            View previousChart = priceHistoryLayout.findViewById(PRICE_HISTORY_CHART_ID);
            if (previousChart != null) {
                priceHistoryLayout.removeView(previousChart);
            }
            View priceHistoryChart = model.get(PRICE_HISTORY_CHART);
            priceHistoryChart.setId(PRICE_HISTORY_CHART_ID);
            Resources resources = priceHistoryChart.getContext().getResources();
            priceHistoryChart.setContentDescription(
                    resources.getString(
                            R.string.price_history_chart_content_description,
                            model.get(PRICE_HISTORY_CHART_CONTENT_DESCRIPTION)));
            int chartHeight = resources.getDimensionPixelSize(R.dimen.price_history_chart_height);
            priceHistoryChart.setLayoutParams(
                    new LayoutParams(LayoutParams.MATCH_PARENT, chartHeight));
            priceHistoryLayout.addView(priceHistoryChart, 2);
        } else if (OPEN_URL_BUTTON_VISIBLE == propertyKey) {
            openUrlButton.setVisibility(
                    model.get(OPEN_URL_BUTTON_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (OPEN_URL_BUTTON_ON_CLICK_LISTENER == propertyKey) {
            openUrlButton.setOnClickListener(model.get(OPEN_URL_BUTTON_ON_CLICK_LISTENER));
        }
    }
}

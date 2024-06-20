// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import android.view.View;
import android.widget.TextView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for the price insights bottom sheet */
public class PriceInsightsBottomSheetViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (PriceInsightsBottomSheetProperties.PRICE_HISTORY_TITLE == propertyKey) {
            ((TextView) view.findViewById(R.id.price_history_title))
                    .setText(model.get(PriceInsightsBottomSheetProperties.PRICE_HISTORY_TITLE));
        }
    }
}

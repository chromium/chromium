// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.chrome.R;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for price insights bottom sheet responsible for model update. */
public class PriceInsightsBottomSheetMediator {
    private final Context mContext;
    private final ShoppingService mShoppingService;
    private final PropertyModel mPropertyModel;

    public PriceInsightsBottomSheetMediator(
            @NonNull Context context,
            @NonNull ShoppingService shoppingService,
            @NonNull PropertyModel propertyModel) {
        mContext = context;
        mShoppingService = shoppingService;
        mPropertyModel = propertyModel;
    }

    public void requestShowContent() {
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_HISTORY_TITLE,
                mContext.getResources().getString(R.string.price_history_title));
    }
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentProperties;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentProvider;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator of the price tracking bottom sheet content. */
public class PriceTrackingBottomSheetContentCoordinator
        implements CommerceBottomSheetContentProvider {
    private Context mContext;
    private Tab mTab;
    private View mPriceTrackingContentContainer;
    private PriceTrackingBottomSheetContentMediator mMediator;

    public PriceTrackingBottomSheetContentCoordinator(
            @NonNull Context context,
            @NonNull Supplier<Tab> tabSupplier,
            @NonNull PriceInsightsDelegate priceInsightsDelegate) {
        mContext = context;
        mTab = tabSupplier.get();
        mPriceTrackingContentContainer =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.price_tracking_layout_v2, /* root= */ null);
        PropertyModel propertyModel =
                new PropertyModel(PriceInsightsBottomSheetProperties.PRICE_TRACKING_KEYS);
        PropertyModelChangeProcessor.create(
                propertyModel,
                mPriceTrackingContentContainer,
                PriceTrackingBottomSheetContentViewBinder::bind);
        mMediator =
                new PriceTrackingBottomSheetContentMediator(
                        context, mTab, propertyModel, priceInsightsDelegate);
    }

    @Override
    public void requestContent(Callback<PropertyModel> contentReadyCallback) {
        Callback<Boolean> showContentCallback =
                (hasContent) -> {
                    contentReadyCallback.onResult(hasContent ? createContentModel() : null);
                };
        mMediator.requestShowContent(showContentCallback);
    }

    @Override
    public void hideContentView() {
        mMediator.closeContent();
    }

    private PropertyModel createContentModel() {
        return new PropertyModel.Builder(CommerceBottomSheetContentProperties.ALL_KEYS)
                .with(CommerceBottomSheetContentProperties.TYPE, ContentType.PRICE_TRACKING)
                .with(CommerceBottomSheetContentProperties.HAS_TITLE, false)
                .with(
                        CommerceBottomSheetContentProperties.CUSTOM_VIEW,
                        mPriceTrackingContentContainer)
                .build();
    }

    View getContentViewForTesting() {
        return mPriceTrackingContentContainer;
    }
}

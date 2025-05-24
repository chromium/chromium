// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_history;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentProperties;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentProvider;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator of the price history bottom sheet content. */
@NullMarked
public class PriceHistoryBottomSheetContentCoordinator
        implements CommerceBottomSheetContentProvider {
    private final Context mContext;
    private final View mPriceHistoryContentContainer;
    private final PriceHistoryBottomSheetContentMediator mMediator;

    public PriceHistoryBottomSheetContentCoordinator(
            Context context,
            Supplier<Tab> tabSupplier,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            PriceInsightsDelegate priceInsightsDelegate) {
        mContext = context;
        mPriceHistoryContentContainer =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.price_history_layout_v2, /* root= */ null);
        PropertyModel propertyModel =
                new PropertyModel(PriceHistoryBottomSheetContentProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                propertyModel,
                mPriceHistoryContentContainer,
                PriceHistoryBottomSheetContentViewBinder::bind);
        mMediator =
                new PriceHistoryBottomSheetContentMediator(
                        context,
                        tabSupplier,
                        tabModelSelectorSupplier,
                        propertyModel,
                        priceInsightsDelegate);
    }

    @Override
    public void requestContent(Callback<@Nullable PropertyModel> contentReadyCallback) {
        Callback<Boolean> showContentCallback =
                (hasContent) -> {
                    contentReadyCallback.onResult(hasContent ? createContentModel() : null);
                };
        mMediator.requestShowContent(showContentCallback);
    }

    @Override
    public void hideContentView() {}

    private PropertyModel createContentModel() {
        return new PropertyModel.Builder(CommerceBottomSheetContentProperties.ALL_KEYS)
                .with(CommerceBottomSheetContentProperties.TYPE, ContentType.PRICE_INSIGHTS)
                .with(CommerceBottomSheetContentProperties.HAS_TITLE, false)
                .with(CommerceBottomSheetContentProperties.HAS_CUSTOM_PADDING, true)
                .with(
                        CommerceBottomSheetContentProperties.CUSTOM_VIEW,
                        mPriceHistoryContentContainer)
                .build();
    }

    View getContentViewForTesting() {
        return mPriceHistoryContentContainer;
    }
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import android.content.Context;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.TouchDelegate;
import android.view.View;
import android.widget.LinearLayout;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentProperties;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentProvider;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.function.Supplier;

/** Coordinator of the price tracking bottom sheet content. */
@NullMarked
public class PriceTrackingBottomSheetContentCoordinator
        implements CommerceBottomSheetContentProvider {
    private final Context mContext;
    private final View mPriceTrackingContentContainer;
    private final PriceTrackingBottomSheetContentMediator mMediator;

    public PriceTrackingBottomSheetContentCoordinator(
            Context context,
            Supplier<@Nullable Tab> tabSupplier,
            PriceInsightsDelegate priceInsightsDelegate) {
        mContext = context;
        mPriceTrackingContentContainer =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.price_tracking_layout_v2, /* root= */ null);
        updateTouchDelegate();
        PropertyModel propertyModel =
                new PropertyModel(PriceTrackingBottomSheetContentProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                propertyModel,
                mPriceTrackingContentContainer,
                PriceTrackingBottomSheetContentViewBinder::bind);
        mMediator =
                new PriceTrackingBottomSheetContentMediator(
                        context, tabSupplier, propertyModel, priceInsightsDelegate);
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
    public void hideContentView() {
        mMediator.closeContent();
    }

    private PropertyModel createContentModel() {
        return new PropertyModel.Builder(CommerceBottomSheetContentProperties.ALL_KEYS)
                .with(CommerceBottomSheetContentProperties.TYPE, ContentType.PRICE_TRACKING)
                .with(CommerceBottomSheetContentProperties.HAS_TITLE, false)
                .with(CommerceBottomSheetContentProperties.HAS_CUSTOM_PADDING, false)
                .with(
                        CommerceBottomSheetContentProperties.CUSTOM_VIEW,
                        mPriceTrackingContentContainer)
                .build();
    }

    private void updateTouchDelegate() {
        // Post in the content container's message queue to make sure price tracking button lays out
        // before setting extra padding.
        mPriceTrackingContentContainer.post(
                new Runnable() {
                    @Override
                    public void run() {
                        Rect delegateArea = new Rect();
                        LinearLayout priceTrackingButton =
                                mPriceTrackingContentContainer.findViewById(
                                        R.id.price_tracking_button);
                        priceTrackingButton.getHitRect(delegateArea);
                        int extraPadding =
                                mContext.getResources()
                                        .getDimensionPixelSize(
                                                R.dimen
                                                        .price_tracking_button_touch_delegate_extra_padding);
                        delegateArea.top -= extraPadding;
                        delegateArea.bottom += extraPadding;
                        TouchDelegate touchDelegate =
                                new TouchDelegate(delegateArea, priceTrackingButton);
                        mPriceTrackingContentContainer.setTouchDelegate(touchDelegate);
                    }
                });
    }

    View getContentViewForTesting() {
        return mPriceTrackingContentContainer;
    }
}

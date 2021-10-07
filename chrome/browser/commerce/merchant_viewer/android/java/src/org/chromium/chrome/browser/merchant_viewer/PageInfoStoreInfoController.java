// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.annotation.SuppressLint;
import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.merchant_viewer.proto.MerchantTrustSignalsOuterClass.MerchantTrustSignals;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.page_info.PageInfoAction;
import org.chromium.components.page_info.PageInfoDiscoverabilityMetrics;
import org.chromium.components.page_info.PageInfoDiscoverabilityMetrics.DiscoverabilityAction;
import org.chromium.components.page_info.PageInfoMainController;
import org.chromium.components.page_info.PageInfoRowView;
import org.chromium.components.page_info.PageInfoSubpageController;

/**
 * Class for controlling the {@link ChromePageInfo} "store info" section.
 */
public class PageInfoStoreInfoController implements PageInfoSubpageController {
    public static final int STORE_INFO_ROW_ID = View.generateViewId();

    /** Handles the actions needed by the "store info" row. */
    public interface StoreInfoActionHandler {
        /** Called when the "store info" row is clicked. */
        void onStoreInfoClicked(MerchantTrustSignals trustSignals);
    }

    private final Supplier<StoreInfoActionHandler> mActionHandlerSupplier;
    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private final Context mContext;
    private final boolean mPageInfoOpenedFromStoreIcon;
    private final PageInfoDiscoverabilityMetrics mDiscoverabilityMetrics =
            new PageInfoDiscoverabilityMetrics();

    public PageInfoStoreInfoController(PageInfoMainController mainController,
            PageInfoRowView rowView,
            @Nullable Supplier<StoreInfoActionHandler> actionHandlerSupplier,
            boolean pageInfoOpenedFromStoreIcon) {
        mMainController = mainController;
        mRowView = rowView;
        mContext = mRowView.getContext();
        mActionHandlerSupplier = actionHandlerSupplier;
        mPageInfoOpenedFromStoreIcon = pageInfoOpenedFromStoreIcon;
        new MerchantTrustSignalsDataProvider().getDataForUrl(
                mMainController.getURL(), this::setupStoreInfoRow);
    }

    @SuppressLint("ResourceType")
    private void setupStoreInfoRow(@Nullable MerchantTrustSignals trustSignals) {
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        if (mActionHandlerSupplier == null || mActionHandlerSupplier.get() == null
                || trustSignals == null) {
            rowParams.visible = false;
        } else {
            rowParams.visible = true;
            rowParams.title =
                    mContext.getResources().getString(R.string.page_info_store_info_title);
            rowParams.subtitle = getRowSubtitle(trustSignals);
            // The icons in PageInfo are tinted automatically.
            rowParams.iconResId = R.drawable.ic_storefront_blue;
            // If user enters page info via the store icon in omnibox, highlight the "Store info"
            // row.
            if (mPageInfoOpenedFromStoreIcon) {
                rowParams.rowTint = mContext.getResources().getColor(R.color.iph_highlight_blue);
            }
            rowParams.clickCallback = () -> {
                if (mPageInfoOpenedFromStoreIcon) {
                    mDiscoverabilityMetrics.recordDiscoverabilityAction(
                            DiscoverabilityAction.STORE_INFO_OPENED);
                }
                mMainController.recordAction(PageInfoAction.PAGE_INFO_STORE_INFO_CLICKED);
                mActionHandlerSupplier.get().onStoreInfoClicked(trustSignals);
            };
        }
        mRowView.setParams(rowParams);
    }

    private CharSequence getRowSubtitle(MerchantTrustSignals trustSignals) {
        // TODO(zhiyuancai): Set subtitle based on trustSignals after updating the proto.
        return MerchantTrustMessageViewModel.getMessageDescription(mContext, trustSignals);
    }

    // PageInfoSubpageController implementations. We don't use subpage for "store info" row.
    @Override
    public String getSubpageTitle() {
        return "";
    }

    @Override
    public View createViewForSubpage(ViewGroup parent) {
        return null;
    }

    @Override
    public void onSubpageRemoved() {}

    @Override
    public void clearData() {}

    @Override
    public void updateRowIfNeeded() {}
}
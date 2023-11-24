// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMessageViewModel.MessageDescriptionUI;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.commerce.core.ShoppingService.MerchantInfo;
import org.chromium.components.page_info.PageInfoAction;
import org.chromium.components.page_info.PageInfoMainController;
import org.chromium.components.page_info.PageInfoRowView;
import org.chromium.components.page_info.PageInfoSubpageController;
import org.chromium.content_public.browser.WebContents;

/** Class for controlling the {@link ChromePageInfo} "store info" section. */
public class PageInfoStoreInfoController implements PageInfoSubpageController {
    public static final int STORE_INFO_ROW_ID = View.generateViewId();

    /** Handles the actions needed by the "store info" row. */
    public interface StoreInfoActionHandler {
        /** Called when the "store info" row is clicked. */
        void onStoreInfoClicked(MerchantInfo merchantInfo);
    }

    private final Supplier<StoreInfoActionHandler> mActionHandlerSupplier;
    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private final Context mContext;
    private final boolean mPageInfoOpenedFromStoreIcon;
    private final WebContents mWebContents;
    private final MerchantTrustMetrics mMetrics = new MerchantTrustMetrics();

    public PageInfoStoreInfoController(
            PageInfoMainController mainController,
            PageInfoRowView rowView,
            @Nullable Supplier<StoreInfoActionHandler> actionHandlerSupplier,
            boolean pageInfoOpenedFromStoreIcon,
            WebContents webContents,
            Profile profile) {
        mMainController = mainController;
        mRowView = rowView;
        mContext = mRowView.getContext();
        mActionHandlerSupplier = actionHandlerSupplier;
        mPageInfoOpenedFromStoreIcon = pageInfoOpenedFromStoreIcon;
        mWebContents = webContents;
        // Creating the instance of {@link MerchantTrustSignalsDataProvider} will force
        // OptimizationGuide to register for the MERCHANT_TRUST_SIGNALS type, so we need to check
        // the feature flag first.
        if (profile != null
                && ShoppingServiceFactory.getForProfile(profile).isMerchantViewerEnabled()) {
            new MerchantTrustSignalsDataProvider()
                    .getDataForUrl(profile, mMainController.getURL(), this::setupStoreInfoRow);
        } else {
            setupStoreInfoRow(null);
        }
    }

    private void setupStoreInfoRow(@Nullable MerchantInfo merchantInfo) {
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        if (mActionHandlerSupplier == null
                || mActionHandlerSupplier.get() == null
                || merchantInfo == null) {
            rowParams.visible = false;
        } else {
            rowParams.visible = true;
            rowParams.title =
                    mContext.getResources().getString(R.string.page_info_store_info_title);
            rowParams.subtitle = getRowSubtitle(merchantInfo);
            // The icons in PageInfo are tinted automatically.
            rowParams.iconResId = R.drawable.ic_storefront_blue;
            // If user enters page info via the store icon in omnibox, highlight the "Store info"
            // row.
            if (mPageInfoOpenedFromStoreIcon) {
                rowParams.rowTint = R.color.iph_highlight_blue;
            }
            rowParams.clickCallback =
                    () -> {
                        mMainController.recordAction(PageInfoAction.PAGE_INFO_STORE_INFO_CLICKED);
                        mMainController.dismiss();
                        mMetrics.recordUkmOnRowClicked(mWebContents);
                        mActionHandlerSupplier.get().onStoreInfoClicked(merchantInfo);
                    };
            mMetrics.recordUkmOnRowSeen(mWebContents);
        }
        mMetrics.recordMetricsForStoreInfoRowVisible(rowParams.visible);
        mRowView.setParams(rowParams);
    }

    private CharSequence getRowSubtitle(MerchantInfo merchantInfo) {
        if (merchantInfo.starRating > 0) {
            CharSequence subTitle =
                    MerchantTrustMessageViewModel.getMessageDescription(
                            mContext, merchantInfo, MessageDescriptionUI.RATING_AND_REVIEWS);
            if (subTitle != null) return subTitle;
        } else if (merchantInfo.hasReturnPolicy) {
            return mContext.getResources()
                    .getString(R.string.page_info_store_info_description_with_no_rating);
        }
        assert false : "Invalid trust signal";
        return "";
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

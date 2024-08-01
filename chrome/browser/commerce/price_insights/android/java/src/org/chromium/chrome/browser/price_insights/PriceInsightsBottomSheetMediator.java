// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfo;
import org.chromium.components.commerce.core.ShoppingService.ProductInfo;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Mediator for price insights bottom sheet responsible for model update. */
public class PriceInsightsBottomSheetMediator {
    private final Context mContext;
    private final Tab mTab;
    private final ShoppingService mShoppingService;
    private final TabModelSelector mTabModelSelector;
    private final PropertyModel mPropertyModel;
    private final PriceInsightsDelegate mPriceInsightsDelegate;

    public PriceInsightsBottomSheetMediator(
            @NonNull Context context,
            @NonNull Tab tab,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ShoppingService shoppingService,
            @NonNull PriceInsightsDelegate priceInsightsDelegate,
            @NonNull PropertyModel propertyModel) {
        mContext = context;
        mTab = tab;
        mTabModelSelector = tabModelSelector;
        mShoppingService = shoppingService;
        mPriceInsightsDelegate = priceInsightsDelegate;
        mPropertyModel = propertyModel;
    }

    public void requestShowContent() {
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_TITLE, mTab.getTitle());
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_DESCRIPTION,
                mContext.getResources()
                        .getString(R.string.price_insights_content_price_tracking_description));

        boolean priceTrackingEligible = isPriceTrackingEligible();

        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ENABLED,
                priceTrackingEligible);
        if (!priceTrackingEligible) {
            updatePriceTrackingButtonIneligible();
        } else {
            updatePriceTrackingButtonDisabled();
        }

        BookmarkId bookmarkId = mPriceInsightsDelegate.getBookmarkIdForTab(mTab);
        if (priceTrackingEligible && bookmarkId != null) {
            PriceTrackingUtils.isBookmarkPriceTracked(
                    mTab.getProfile(),
                    bookmarkId.getId(),
                    isTracked -> {
                        if (isTracked) {
                            updatePriceTrackingButtonEnabled();
                        }
                    });
        }

        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_HISTORY_TITLE,
                mContext.getResources().getString(R.string.price_history_title));

        ShoppingServiceFactory.getForProfile(mTab.getProfile())
                .getPriceInsightsInfoForUrl(
                        mTab.getUrl(),
                        (url, info) -> {
                            updatePriceInsightsInfo(info);
                        });
    }

    private boolean isPriceTrackingEligible() {
        ShoppingService service = ShoppingServiceFactory.getForProfile(mTab.getProfile());
        if (service == null) return false;
        ProductInfo info = service.getAvailableProductInfoForUrl(mTab.getUrl());
        return info != null && info.productClusterId.isPresent();
    }

    private void updatePriceTrackingButtonIneligible() {
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_TEXT,
                mContext.getResources()
                        .getString(
                                R.string
                                        .price_insights_content_price_tracking_disabled_button_text));
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ICON,
                R.drawable.price_insights_sheet_price_tracking_button_disabled);
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_FOREGROUND_COLOR,
                R.color.price_insights_sheet_price_tracking_ineligible_button_foreground_color);
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_BACKGROUND_COLOR,
                R.color.price_insights_sheet_price_tracking_ineligible_button_bg_color);
    }

    private void updatePriceTrackingButtonDisabled() {
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_TEXT,
                mContext.getResources()
                        .getString(
                                R.string
                                        .price_insights_content_price_tracking_disabled_button_text));
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ICON,
                R.drawable.price_insights_sheet_price_tracking_button_disabled);
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_FOREGROUND_COLOR,
                R.color.price_insights_sheet_price_tracking_disabled_button_foreground_color);
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_BACKGROUND_COLOR,
                R.color.price_insights_sheet_price_tracking_disabled_button_bg_color);
    }

    private void updatePriceTrackingButtonEnabled() {
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_TEXT,
                mContext.getResources()
                        .getString(
                                R.string
                                        .price_insights_content_price_tracking_enabled_button_text));
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ICON,
                R.drawable.price_insights_sheet_price_tracking_button_enabled);
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_FOREGROUND_COLOR,
                R.color.price_insights_sheet_price_tracking_enabled_button_foreground_color);
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_BACKGROUND_COLOR,
                R.color.price_insights_sheet_price_tracking_enabled_button_bg_color);
    }

    private void updatePriceInsightsInfo(PriceInsightsInfo info) {
        if (info == null || info.jackpotUrl == null || info.jackpotUrl.isEmpty()) {
            return;
        }
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.OPEN_URL_TITLE,
                mContext.getResources().getString(R.string.price_insights_open_url_title));
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.OPEN_URL_BUTTON_ICON,
                R.drawable.ic_open_in_new_24dp);
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.OPEN_URL_BUTTON_ON_CLICK_LISTENER,
                view -> openJackpotUrl(info.jackpotUrl.get()));
    }

    private void openJackpotUrl(GURL url) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        mTabModelSelector.openNewTab(
                loadUrlParams, TabLaunchType.FROM_LINK, mTab, /* incognito= */ false);
    }
}

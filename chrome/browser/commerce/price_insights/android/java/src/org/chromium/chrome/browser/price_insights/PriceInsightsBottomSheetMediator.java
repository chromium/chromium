// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_HISTORY_DESCRIPTION;

import android.content.Context;
import android.view.View.OnClickListener;

import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.core.app.NotificationManagerCompat;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.commerce.core.PriceBucket;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfo;
import org.chromium.components.commerce.core.ShoppingService.ProductInfo;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

/** Mediator for price insights bottom sheet responsible for model update. */
public class PriceInsightsBottomSheetMediator {
    private final Context mContext;
    private final Tab mTab;
    private final ShoppingService mShoppingService;
    private final TabModelSelector mTabModelSelector;
    private final PropertyModel mPropertyModel;
    private final PriceInsightsDelegate mPriceInsightsDelegate;
    private final ObservableSupplier<Boolean> mPriceTrackingStateSupplier;

    private @PriceBucket int mPriceBucket;

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

        mPriceTrackingStateSupplier = priceInsightsDelegate.getPriceTrackingStateSupplier(tab);
        mPriceTrackingStateSupplier.addObserver(this::updatePriceTrackingButtonModel);
    }

    public void requestShowContent() {
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_TITLE, mTab.getTitle());
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_DESCRIPTION,
                mContext.getResources()
                        .getString(R.string.price_insights_content_price_tracking_description));

        updatePriceTrackingButtonModel(mPriceTrackingStateSupplier.get());

        ShoppingServiceFactory.getForProfile(mTab.getProfile())
                .getPriceInsightsInfoForUrl(
                        mTab.getUrl(),
                        (url, info) -> {
                            updatePriceInsightsInfo(info);
                        });
    }

    public void closeContent() {
        mPriceTrackingStateSupplier.removeObserver(this::updatePriceTrackingButtonModel);
    }

    private void updatePriceTrackingButtonModel(boolean isPriceTracked) {
        boolean priceTrackingEligible = isPriceTrackingEligible();

        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ENABLED,
                priceTrackingEligible);

        if (!priceTrackingEligible) {
            updatePriceTrackingButtonIneligible();
            return;
        }

        updatePriceTrackingButtonState(isPriceTracked);
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER,
                createPriceTrackingButtonListener(!isPriceTracked));
    }

    // TODO(359182810): Use common price tracking utils.
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

    private void updatePriceTrackingButtonState(boolean enabled) {
        int buttonTextResId =
                enabled
                        ? R.string.price_insights_content_price_tracking_enabled_button_text
                        : R.string.price_insights_content_price_tracking_disabled_button_text;
        int buttonIconResId =
                enabled
                        ? R.drawable.price_insights_sheet_price_tracking_button_enabled
                        : R.drawable.price_insights_sheet_price_tracking_button_disabled;
        int buttonForegroundColorResId =
                enabled
                        ? R.color
                                .price_insights_sheet_price_tracking_enabled_button_foreground_color
                        : R.color
                                .price_insights_sheet_price_tracking_disabled_button_foreground_color;
        int buttonBackgroundColorResId =
                enabled
                        ? R.color.price_insights_sheet_price_tracking_enabled_button_bg_color
                        : R.color.price_insights_sheet_price_tracking_disabled_button_bg_color;

        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_TEXT,
                mContext.getResources().getString(buttonTextResId));
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ICON, buttonIconResId);
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_FOREGROUND_COLOR,
                buttonForegroundColorResId);
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_BACKGROUND_COLOR,
                buttonBackgroundColorResId);
    }

    private OnClickListener createPriceTrackingButtonListener(boolean shouldBeTracked) {
        return view -> {
            String histogramActionName = shouldBeTracked ? "Track" : "Untrack";
            RecordHistogram.recordEnumeratedHistogram(
                    "Commerce.PriceInsights.PriceTracking." + histogramActionName,
                    mPriceBucket,
                    PriceBucket.MAX_VALUE + 1);
            Callback<Boolean> callback =
                    (success) -> {
                        updatePriceTrackingButtonModel(mPriceTrackingStateSupplier.get());
                        showToastMessage(shouldBeTracked, success);
                    };
            updatePriceTrackingButtonState(shouldBeTracked);
            mPriceInsightsDelegate.setPriceTrackingStateForTab(mTab, shouldBeTracked, callback);
        };
    }

    private void showToastMessage(boolean shouldBeTracked, boolean success) {
        @StringRes int textResId = R.string.price_insights_content_price_tracking_error_message;
        if (success) {
            if (shouldBeTracked) {
                textResId =
                        NotificationManagerCompat.from(mContext).areNotificationsEnabled()
                                ? R.string
                                        .price_insights_content_price_tracked_success_notification_enabled_message
                                : R.string
                                        .price_insights_content_price_tracked_success_notification_disabled_message;
            } else {
                textResId = R.string.price_insights_content_price_untracked_success_message;
            }
        }
        Toast.makeText(mContext, textResId, Toast.LENGTH_SHORT).show();
    }

    private void updatePriceInsightsInfo(PriceInsightsInfo info) {
        if (info == null
                || info.currencyCode.isEmpty()
                || info.catalogHistoryPrices == null
                || info.catalogHistoryPrices.isEmpty()) {
            return;
        }
        mPriceBucket = info.priceBucket;
        @StringRes int priceHistoryTitleResId = R.string.price_history_title;
        if (info.hasMultipleCatalogs
                && info.catalogAttributes != null
                && !info.catalogAttributes.isEmpty()) {
            priceHistoryTitleResId = R.string.price_history_multiple_catalogs_title;
            mPropertyModel.set(PRICE_HISTORY_DESCRIPTION, info.catalogAttributes.get());
        }
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_HISTORY_TITLE,
                mContext.getResources().getString(priceHistoryTitleResId));
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.PRICE_HISTORY_CHART,
                mPriceInsightsDelegate.getPriceHistoryChartForPriceInsightsInfo(info));

        if (info.jackpotUrl == null || info.jackpotUrl.isEmpty()) {
            return;
        }
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.OPEN_URL_TITLE,
                mContext.getResources().getString(R.string.price_insights_open_url_title));
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.OPEN_URL_BUTTON_ICON,
                R.drawable.ic_open_in_new_20dp);
        mPropertyModel.set(
                PriceInsightsBottomSheetProperties.OPEN_URL_BUTTON_ON_CLICK_LISTENER,
                view -> openJackpotUrl(info.jackpotUrl.get()));
    }

    private void openJackpotUrl(GURL url) {
        RecordHistogram.recordEnumeratedHistogram(
                "Commerce.PriceInsights.BuyingOptionsClicked",
                mPriceBucket,
                PriceBucket.MAX_VALUE + 1);
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        mTabModelSelector.openNewTab(
                loadUrlParams, TabLaunchType.FROM_LINK, mTab, /* incognito= */ false);
    }
}

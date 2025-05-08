// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_BUTTON_BACKGROUND_COLOR;
import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_BUTTON_FOREGROUND_COLOR;
import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_BUTTON_ICON;
import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_BUTTON_TEXT;
import static org.chromium.chrome.browser.price_tracking.PriceTrackingBottomSheetContentProperties.PRICE_TRACKING_TITLE;

import android.content.Context;
import android.view.View.OnClickListener;

import androidx.annotation.StringRes;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.PriceBucket;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

/** Mediator for price tracking bottom sheet responsible for property model update. */
@NullMarked
public class PriceTrackingBottomSheetContentMediator {
    private final Context mContext;
    private final Supplier<Tab> mTabSupplier;
    private final PropertyModel mPropertyModel;
    private final PriceInsightsDelegate mPriceInsightsDelegate;
    private final Callback<Boolean> mUpdatePriceTrackingButtonModelCallback =
            this::updatePriceTrackingButtonModel;

    private @Nullable ObservableSupplier<Boolean> mPriceTrackingStateSupplier;
    private @PriceBucket int mPriceBucket;

    public PriceTrackingBottomSheetContentMediator(
            Context context,
            Supplier<Tab> tabSupplier,
            PropertyModel propertyModel,
            PriceInsightsDelegate priceInsightsDelegate) {
        mContext = context;
        mTabSupplier = tabSupplier;
        mPropertyModel = propertyModel;
        mPriceInsightsDelegate = priceInsightsDelegate;
    }

    public void requestShowContent(Callback<Boolean> contentReadyCallback) {
        ShoppingService shoppingService =
                ShoppingServiceFactory.getForProfile(mTabSupplier.get().getProfile());
        if (shoppingService == null
                || !CommerceFeatureUtils.isShoppingListEligible(shoppingService)) {
            contentReadyCallback.onResult(false);
        }

        mPriceTrackingStateSupplier =
                mPriceInsightsDelegate.getPriceTrackingStateSupplier(mTabSupplier.get());
        mPriceTrackingStateSupplier.addObserver(mUpdatePriceTrackingButtonModelCallback);

        shoppingService.getProductInfoForUrl(
                mTabSupplier.get().getUrl(),
                (url, info) -> {
                    boolean hasProductInfo = info != null && info.productClusterId.isPresent();
                    if (hasProductInfo) {
                        updatePriceTrackingButtonModel(
                                assumeNonNull(mPriceTrackingStateSupplier).get());
                    }
                    contentReadyCallback.onResult(hasProductInfo);
                });
        fetchPriceBucket();
    }

    private void updatePriceTrackingButtonModel(boolean isPriceTracked) {
        mPropertyModel.set(PRICE_TRACKING_TITLE, mTabSupplier.get().getTitle());

        updatePriceTrackingButtonState(isPriceTracked);
        mPropertyModel.set(
                PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER,
                createPriceTrackingButtonListener(!isPriceTracked));
    }

    private void fetchPriceBucket() {
        ShoppingServiceFactory.getForProfile(mTabSupplier.get().getProfile())
                .getPriceInsightsInfoForUrl(
                        mTabSupplier.get().getUrl(),
                        (url, info) -> {
                            if (info != null) {
                                mPriceBucket = info.priceBucket;
                            }
                        });
    }

    public void closeContent() {
        if (mPriceTrackingStateSupplier != null) {
            mPriceTrackingStateSupplier.removeObserver(mUpdatePriceTrackingButtonModelCallback);
        }
        mPriceTrackingStateSupplier = null;
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

        int buttonForegroundColor =
                enabled
                        ? SemanticColorUtils.getDefaultControlColorActive(mContext)
                        : SemanticColorUtils.getDefaultIconColorOnAccent1Container(mContext);
        int buttonBackgroundColor =
                enabled
                        ? SemanticColorUtils.getDefaultBgColor(mContext)
                        : SemanticColorUtils.getColorPrimaryContainer(mContext);

        mPropertyModel.set(PRICE_TRACKING_BUTTON_TEXT, mContext.getString(buttonTextResId));
        mPropertyModel.set(PRICE_TRACKING_BUTTON_ICON, buttonIconResId);
        mPropertyModel.set(PRICE_TRACKING_BUTTON_FOREGROUND_COLOR, buttonForegroundColor);
        mPropertyModel.set(PRICE_TRACKING_BUTTON_BACKGROUND_COLOR, buttonBackgroundColor);
    }

    private OnClickListener createPriceTrackingButtonListener(boolean shouldBeTracked) {
        return view -> {
            logPriceTrackingButtonClicked(shouldBeTracked);
            Callback<Boolean> callback =
                    (success) -> {
                        updatePriceTrackingButtonModel(
                                assumeNonNull(mPriceTrackingStateSupplier).get());
                        showToastMessage(shouldBeTracked, success);
                    };
            updatePriceTrackingButtonState(shouldBeTracked);
            mPriceInsightsDelegate.setPriceTrackingStateForTab(
                    mTabSupplier.get(), shouldBeTracked, callback);
        };
    }

    private void logPriceTrackingButtonClicked(boolean shouldBeTracked) {
        String histogramActionName = shouldBeTracked ? "Track" : "Untrack";
        RecordHistogram.recordEnumeratedHistogram(
                "Commerce.PriceInsights.PriceTracking." + histogramActionName,
                mPriceBucket,
                PriceBucket.MAX_VALUE);
    }

    private void showToastMessage(boolean shouldBeTracked, boolean success) {
        @StringRes int textResId = R.string.price_insights_content_price_tracking_error_message;
        if (success) {
            if (shouldBeTracked) {
                textResId =
                        NotificationProxyUtils.areNotificationsEnabled()
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
}

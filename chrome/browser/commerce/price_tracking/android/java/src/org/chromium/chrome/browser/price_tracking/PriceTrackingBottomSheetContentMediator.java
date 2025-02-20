// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_BACKGROUND_COLOR;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ENABLED;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_FOREGROUND_COLOR;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ICON;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_BUTTON_TEXT;
import static org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetProperties.PRICE_TRACKING_TITLE;

import android.content.Context;
import android.view.View.OnClickListener;

import androidx.annotation.NonNull;
import androidx.annotation.StringRes;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

/** Mediator for price tracking bottom sheet responsible for property model update. */
public class PriceTrackingBottomSheetContentMediator {
    private final Context mContext;
    private final Tab mTab;
    private final PropertyModel mPropertyModel;
    private final PriceInsightsDelegate mPriceInsightsDelegate;
    private final ObservableSupplier<Boolean> mPriceTrackingStateSupplier;
    private final Callback<Boolean> mUpdatePriceTrackingButtonModelCallback =
            this::updatePriceTrackingButtonModel;

    public PriceTrackingBottomSheetContentMediator(
            @NonNull Context context,
            @NonNull Tab tab,
            @NonNull PropertyModel propertyModel,
            @NonNull PriceInsightsDelegate priceInsightsDelegate) {
        mContext = context;
        mTab = tab;
        mPropertyModel = propertyModel;
        mPriceInsightsDelegate = priceInsightsDelegate;

        mPriceTrackingStateSupplier = priceInsightsDelegate.getPriceTrackingStateSupplier(tab);
        mPriceTrackingStateSupplier.addObserver(mUpdatePriceTrackingButtonModelCallback);
    }

    public void requestShowContent(Callback<Boolean> contentReadyCallback) {
        updatePriceTrackingButtonModel(mPriceTrackingStateSupplier.get());
        contentReadyCallback.onResult(true);
    }

    private void updatePriceTrackingButtonModel(boolean isPriceTracked) {
        boolean priceTrackingEligible =
                CommerceFeatureUtils.isShoppingListEligible(
                        ShoppingServiceFactory.getForProfile(mTab.getProfile()));

        mPropertyModel.set(PRICE_TRACKING_TITLE, mTab.getTitle());
        mPropertyModel.set(PRICE_TRACKING_BUTTON_ENABLED, priceTrackingEligible);

        if (!priceTrackingEligible) {
            updatePriceTrackingButtonIneligible();
            return;
        }
        updatePriceTrackingButtonState(isPriceTracked);
        mPropertyModel.set(
                PRICE_TRACKING_BUTTON_ON_CLICK_LISTENER,
                createPriceTrackingButtonListener(!isPriceTracked));
    }

    public void closeContent() {
        mPriceTrackingStateSupplier.removeObserver(mUpdatePriceTrackingButtonModelCallback);
    }

    private void updatePriceTrackingButtonIneligible() {
        mPropertyModel.set(
                PRICE_TRACKING_BUTTON_TEXT,
                mContext.getString(
                        R.string.price_insights_content_price_tracking_disabled_button_text));
        mPropertyModel.set(
                PRICE_TRACKING_BUTTON_ICON,
                R.drawable.price_insights_sheet_price_tracking_button_disabled);
        mPropertyModel.set(
                PRICE_TRACKING_BUTTON_FOREGROUND_COLOR,
                mContext.getColor(R.color.price_tracking_ineligible_button_foreground_color));
        mPropertyModel.set(
                PRICE_TRACKING_BUTTON_BACKGROUND_COLOR,
                mContext.getColor(R.color.price_tracking_ineligible_button_background_color));
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

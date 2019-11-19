// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.micro;

import android.support.v7.content.res.AppCompatResources;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Microtransaction view binder, which is stateless. It is called to bind a given model to a given
 * view. Should contain as little business logic as possible.
 */
/* package */ class MicrotransactionViewBinder {
    /* package */ static void bind(
            PropertyModel model, MicrotransactionView view, PropertyKey propertyKey) {
        if (MicrotransactionProperties.PAYMENT_APP_ICON == propertyKey) {
            view.mPaymentAppIcon.setImageDrawable(
                    model.get(MicrotransactionProperties.PAYMENT_APP_ICON));
        } else if (MicrotransactionProperties.PAYMENT_APP_NAME == propertyKey) {
            view.mPaymentAppName.setText(model.get(MicrotransactionProperties.PAYMENT_APP_NAME));
        } else if (MicrotransactionProperties.CURRENCY == propertyKey) {
            CharSequence currency = model.get(MicrotransactionProperties.CURRENCY);
            view.mToolbarCurrency.setText(currency);
            view.mContentCurrency.setText(currency);
            view.mAccountBalanceCurrency.setText(currency);
        } else if (MicrotransactionProperties.AMOUNT == propertyKey) {
            CharSequence amount = model.get(MicrotransactionProperties.AMOUNT);
            view.mToolbarAmount.setText(amount);
            view.mContentAmount.setText(amount);
        } else if (MicrotransactionProperties.ACCOUNT_BALANCE == propertyKey) {
            view.mAccountBalance.setText(model.get(MicrotransactionProperties.ACCOUNT_BALANCE));
        } else if (MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA == propertyKey) {
            float appNameAlpha = model.get(MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA);
            if (appNameAlpha == 0f) return;

            view.mPaymentAppName.setAlpha(appNameAlpha);

            view.mLargeToolbarStatusMessage.setVisibility(View.GONE);
            view.mSmallToolbarStatusMessage.setVisibility(View.GONE);
            view.mToolbarAmount.setVisibility(View.GONE);
            view.mToolbarCurrency.setVisibility(View.GONE);
            view.mToolbarPayButton.setVisibility(View.GONE);
            view.mToolbarProcessingSpinner.setVisibility(View.GONE);
            view.mToolbarStatusIcon.setVisibility(View.GONE);
        } else if (MicrotransactionProperties.IS_SHOWING_PAY_BUTTON == propertyKey) {
            boolean isShowingPayButton =
                    model.get(MicrotransactionProperties.IS_SHOWING_PAY_BUTTON);

            int payButtonVisibility = isShowingPayButton ? View.VISIBLE : View.GONE;
            view.mContentPayButton.setVisibility(payButtonVisibility);

            int nonPayButtonVisibility = isShowingPayButton ? View.GONE : View.VISIBLE;
            view.mContentStatusIcon.setVisibility(nonPayButtonVisibility);
            view.mContentStatusMessage.setVisibility(nonPayButtonVisibility);

            float appNameAlpha = model.get(MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA);
            int toolbarPayButtonVisibility =
                    isShowingPayButton && appNameAlpha == 0f ? View.VISIBLE : View.GONE;
            view.mToolbarPayButton.setVisibility(toolbarPayButtonVisibility);

            int toolbarContentVisibility =
                    isShowingPayButton || appNameAlpha > 0f ? View.GONE : View.VISIBLE;
            view.mToolbarStatusIcon.setVisibility(toolbarContentVisibility);
        } else if (MicrotransactionProperties.IS_STATUS_EMPHASIZED == propertyKey) {
            float appNameAlpha = model.get(MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA);
            if (appNameAlpha > 0f) return;

            boolean isStatusEmphasized = model.get(MicrotransactionProperties.IS_STATUS_EMPHASIZED);
            float nonEmphasizedStatusAlpha = isStatusEmphasized ? 0 : 1;
            view.mSmallToolbarStatusMessage.setAlpha(nonEmphasizedStatusAlpha);
            view.mToolbarAmount.setAlpha(nonEmphasizedStatusAlpha);
            view.mToolbarCurrency.setAlpha(nonEmphasizedStatusAlpha);
            view.mToolbarPayButton.setAlpha(nonEmphasizedStatusAlpha);

            float emphasizedStatusAlpha = isStatusEmphasized ? 1 : 0;
            view.mLargeToolbarStatusMessage.setAlpha(emphasizedStatusAlpha);
        } else if (MicrotransactionProperties.STATUS_TEXT == propertyKey) {
            CharSequence statusText = model.get(MicrotransactionProperties.STATUS_TEXT);
            if (statusText == null) return;

            view.mContentStatusMessage.setText(statusText);
            view.mLargeToolbarStatusMessage.setText(statusText);
            view.mSmallToolbarStatusMessage.setText(statusText);
        } else if (MicrotransactionProperties.STATUS_TEXT_RESOURCE == propertyKey) {
            CharSequence statusText = model.get(MicrotransactionProperties.STATUS_TEXT);
            if (statusText != null) return;

            Integer statusTextResourceId =
                    model.get(MicrotransactionProperties.STATUS_TEXT_RESOURCE);
            if (statusTextResourceId == null) return;

            view.mContentStatusMessage.setText(statusTextResourceId);
            view.mLargeToolbarStatusMessage.setText(statusTextResourceId);
            view.mSmallToolbarStatusMessage.setText(statusTextResourceId);
        } else if (MicrotransactionProperties.STATUS_ICON == propertyKey) {
            Integer statusIconResourceId = model.get(MicrotransactionProperties.STATUS_ICON);
            if (statusIconResourceId == null) return;

            view.mContentStatusIcon.setImageResource(statusIconResourceId);
            view.mToolbarStatusIcon.setImageResource(statusIconResourceId);
        } else if (MicrotransactionProperties.STATUS_ICON_TINT == propertyKey) {
            Integer statusIconTint = model.get(MicrotransactionProperties.STATUS_ICON_TINT);
            if (statusIconTint == null) return;

            ApiCompatibilityUtils.setImageTintList(view.mToolbarStatusIcon,
                    AppCompatResources.getColorStateList(view.mContext, statusIconTint));
            ApiCompatibilityUtils.setImageTintList(view.mContentStatusIcon,
                    AppCompatResources.getColorStateList(view.mContext, statusIconTint));
        } else if (MicrotransactionProperties.IS_SHOWING_PROCESSING_SPINNER == propertyKey) {
            boolean isShowingProcessingSpinner =
                    model.get(MicrotransactionProperties.IS_SHOWING_PROCESSING_SPINNER);
            int contentProcessingSpinnerVisibility =
                    isShowingProcessingSpinner ? View.VISIBLE : View.GONE;
            view.mContentProcessingSpinner.setVisibility(contentProcessingSpinnerVisibility);

            float appNameAlpha = model.get(MicrotransactionProperties.PAYMENT_APP_NAME_ALPHA);
            int toolbarProcessingSpinnerVisibility =
                    isShowingProcessingSpinner && appNameAlpha == 0f ? View.VISIBLE : View.GONE;
            view.mToolbarProcessingSpinner.setVisibility(toolbarProcessingSpinnerVisibility);
        } else if (MicrotransactionProperties.IS_SHOWING_LINE_ITEMS == propertyKey) {
            boolean isShowingLineItems =
                    model.get(MicrotransactionProperties.IS_SHOWING_LINE_ITEMS);
            int lineItemVisibility = isShowingLineItems ? View.VISIBLE : View.GONE;
            view.mAccountBalanceCurrency.setVisibility(lineItemVisibility);
            view.mAccountBalanceLabel.setVisibility(lineItemVisibility);
            view.mAccountBalance.setVisibility(lineItemVisibility);
            view.mContentAmount.setVisibility(lineItemVisibility);
            view.mContentCurrency.setVisibility(lineItemVisibility);
            view.mLineItemSeparator.setVisibility(lineItemVisibility);
            view.mPaymentLabel.setVisibility(lineItemVisibility);

            MarginLayoutParams params =
                    (MarginLayoutParams) view.mContentStatusIcon.getLayoutParams();
            int topSpacing = view.mContext.getResources().getDimensionPixelSize(isShowingLineItems
                            ? R.dimen.microtransaction_content_icon_spacing
                            : R.dimen.microtransaction_content_top_spacing);
            params.setMargins(
                    params.leftMargin, topSpacing, params.rightMargin, params.bottomMargin);
            view.mContentProcessingSpinner.setLayoutParams(params);
            view.mContentStatusIcon.setLayoutParams(params);
        } else if (MicrotransactionProperties.IS_PEEK_STATE_ENABLED == propertyKey) {
            view.mIsPeekStateEnabled = model.get(MicrotransactionProperties.IS_PEEK_STATE_ENABLED);
        }
    }
}

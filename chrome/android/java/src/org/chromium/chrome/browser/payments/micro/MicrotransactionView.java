// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.micro;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent;

/** Microtransaction UI. */
/* package */ class MicrotransactionView implements BottomSheetContent {
    private final View mContentView;
    private final View mToolbarView;

    /* package */ final Context mContext;

    /* package */ final Button mContentPayButton;
    /* package */ final Button mToolbarPayButton;

    /* package */ final ImageView mContentStatusIcon;
    /* package */ final ImageView mPaymentAppIcon;
    /* package */ final ImageView mToolbarStatusIcon;

    /* package */ final TextView mAccountBalance;
    /* package */ final TextView mAccountBalanceCurrency;
    /* package */ final TextView mContentAmount;
    /* package */ final TextView mContentCurrency;
    /* package */ final TextView mContentStatusMessage;
    /* package */ final TextView mLargeToolbarStatusMessage;
    /* package */ final TextView mPaymentAppName;
    /* package */ final TextView mSmallToolbarStatusMessage;
    /* package */ final TextView mToolbarAmount;
    /* package */ final TextView mToolbarCurrency;

    /* package */ final View mAccountBalanceLabel;
    /* package */ final View mContentProcessingSpinner;
    /* package */ final View mLineItemSeparator;
    /* package */ final View mPaymentLabel;
    /* package */ final View mToolbarProcessingSpinner;

    /* package */ boolean mIsPeekStateEnabled;

    /* package */ MicrotransactionView(Context context) {
        mContext = context;
        mContentView =
                LayoutInflater.from(mContext).inflate(R.layout.microtransaction_content, null);
        mToolbarView =
                LayoutInflater.from(mContext).inflate(R.layout.microtransaction_toolbar, null);

        mContentPayButton = (Button) mContentView.findViewById(R.id.pay_button);
        mToolbarPayButton = (Button) mToolbarView.findViewById(R.id.pay_button);

        mContentStatusIcon = (ImageView) mContentView.findViewById(R.id.status_icon);
        mPaymentAppIcon = (ImageView) mToolbarView.findViewById(R.id.payment_app_icon);
        mToolbarStatusIcon = (ImageView) mToolbarView.findViewById(R.id.status_icon);

        mAccountBalance = (TextView) mContentView.findViewById(R.id.account_balance);
        mAccountBalanceCurrency =
                (TextView) mContentView.findViewById(R.id.account_balance_currency);
        mContentAmount = (TextView) mContentView.findViewById(R.id.payment_amount);
        mContentCurrency = (TextView) mContentView.findViewById(R.id.payment_currency);
        mContentStatusMessage = (TextView) mContentView.findViewById(R.id.status_message);
        mLargeToolbarStatusMessage =
                (TextView) mToolbarView.findViewById(R.id.large_status_message);
        mPaymentAppName = (TextView) mToolbarView.findViewById(R.id.payment_app_name);
        mSmallToolbarStatusMessage =
                (TextView) mToolbarView.findViewById(R.id.small_emphasized_status_message);
        mToolbarAmount = (TextView) mToolbarView.findViewById(R.id.amount);
        mToolbarCurrency = (TextView) mToolbarView.findViewById(R.id.currency);

        mAccountBalanceLabel = mContentView.findViewById(R.id.account_balance_label);
        mContentProcessingSpinner = mContentView.findViewById(R.id.processing_spinner);
        mLineItemSeparator = mContentView.findViewById(R.id.line_item_separator);
        mPaymentLabel = mContentView.findViewById(R.id.payment_label);
        mToolbarProcessingSpinner = mToolbarView.findViewById(R.id.processing_spinner);
    }

    // BottomSheetContent:
    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    @Nullable
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    @ContentPriority
    public int getPriority() {
        // If multiple bottom sheets are queued up to be shown, prioritize microtransaction, because
        // it's triggered by a user gesture, such as a click on <button>Buy this article</button>.
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public int getPeekHeight() {
        return mIsPeekStateEnabled ? HeightMode.DEFAULT : HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.payment_request_payment_method_section_name;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.payment_request_payment_method_section_name;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.payment_request_payment_method_section_name;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.payment_request_payment_method_section_name;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }
}

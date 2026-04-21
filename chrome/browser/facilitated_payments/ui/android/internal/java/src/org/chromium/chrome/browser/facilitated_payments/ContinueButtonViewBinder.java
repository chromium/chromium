// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_ICON;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_NUMBER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_PAYMENT_RAIL;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_TRANSACTION_LIMIT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_TYPE;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.ON_BANK_ACCOUNT_CLICK_ACTION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.ACCOUNT_DISPLAY_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.EWALLET_DRAWABLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.EWALLET_ICON_BITMAP;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.EWALLET_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.EwalletProperties.ON_EWALLET_CLICK_ACTION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PaymentAppProperties.ON_PAYMENT_APP_CLICK_ACTION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PaymentAppProperties.PAYMENT_APP_ICON;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.PaymentAppProperties.PAYMENT_APP_NAME;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map property changes to the continue button view in the payment methods
 * bottom sheet.
 */
@NullMarked
class ContinueButtonViewBinder {
    static View createContinueButtonView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.facilitated_payments_continue_button, parent, false);
    }

    static void bindContinueButtonView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == ON_BANK_ACCOUNT_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_BANK_ACCOUNT_CLICK_ACTION).run());
            TextView buttonTitleText =
                    view.findViewById(R.id.facilitated_payments_continue_button_title);
            buttonTitleText.setText(R.string.autofill_payment_method_continue_button);
        } else if (propertyKey == ON_EWALLET_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_EWALLET_CLICK_ACTION).run());
            TextView buttonTitleText =
                    view.findViewById(R.id.facilitated_payments_continue_button_title);
            buttonTitleText.setText(R.string.autofill_payment_method_continue_button);
        } else if (propertyKey == ON_PAYMENT_APP_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_PAYMENT_APP_CLICK_ACTION).run());
            TextView buttonTitleText =
                    view.findViewById(R.id.facilitated_payments_continue_button_title);
            buttonTitleText.setText(R.string.autofill_payment_method_continue_button);
        } else if (propertyKey == BANK_NAME
                || propertyKey == BANK_ACCOUNT_PAYMENT_RAIL
                || propertyKey == BANK_ACCOUNT_TYPE
                || propertyKey == BANK_ACCOUNT_NUMBER
                || propertyKey == BANK_ACCOUNT_TRANSACTION_LIMIT
                || propertyKey == BANK_ACCOUNT_ICON
                || propertyKey == ACCOUNT_DISPLAY_NAME
                || propertyKey == EWALLET_ICON_BITMAP
                || propertyKey == EWALLET_NAME
                || propertyKey == EWALLET_DRAWABLE_ID
                || propertyKey == PAYMENT_APP_NAME
                || propertyKey == PAYMENT_APP_ICON) {
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private ContinueButtonViewBinder() {}
}

// Copyright 2024 The Chromium Authors
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

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map changes of {@link BankAccountProperties} in a {@link PropertyModel}
 * to the suitable method in {@link FacilitatedPaymentsPaymentMethodsView}.
 */
@NullMarked
class BankAccountViewBinder {
    static View createBankAccountItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.bank_account_item, parent, false);
    }

    static void bindBankAccountItemView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == BANK_NAME) {
            TextView bankName = view.findViewById(R.id.bank_name);
            bankName.setText(model.get(BANK_NAME));
        } else if (propertyKey == BANK_ACCOUNT_PAYMENT_RAIL) {
            TextView bankAccountPaymentRail = view.findViewById(R.id.bank_account_payment_rail);
            bankAccountPaymentRail.setText(model.get(BANK_ACCOUNT_PAYMENT_RAIL));
        } else if (propertyKey == BANK_ACCOUNT_TYPE) {
            TextView bankAccountType = view.findViewById(R.id.bank_account_type);
            bankAccountType.setText(model.get(BANK_ACCOUNT_TYPE));
        } else if (propertyKey == BANK_ACCOUNT_NUMBER) {
            TextView bankAccountNumber = view.findViewById(R.id.bank_account_number);
            bankAccountNumber.setText(model.get(BANK_ACCOUNT_NUMBER));
        } else if (propertyKey == BANK_ACCOUNT_TRANSACTION_LIMIT) {
            TextView transactionLimit = view.findViewById(R.id.bank_account_additional_info);
            transactionLimit.setText(model.get(BANK_ACCOUNT_TRANSACTION_LIMIT));
        } else if (propertyKey == BANK_ACCOUNT_ICON) {
            ImageView bankAccountIcon = view.findViewById(R.id.bank_account_icon);
            bankAccountIcon.setImageDrawable(model.get(BANK_ACCOUNT_ICON));
        } else if (propertyKey == ON_BANK_ACCOUNT_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_BANK_ACCOUNT_CLICK_ACTION).run());
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private BankAccountViewBinder() {}
}

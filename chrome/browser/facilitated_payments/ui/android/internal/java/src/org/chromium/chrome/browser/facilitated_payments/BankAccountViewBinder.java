// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_DRAWABLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_ICON_BITMAP;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_SUMMARY;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_TRANSACTION_LIMIT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.ON_BANK_ACCOUNT_CLICK_ACTION;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map changes of {@link BankAccountProperties} in a {@link PropertyModel}
 * to the suitable method in {@link FacilitatedPaymentsPaymentMethodsView}.
 */
class BankAccountViewBinder {
    static View createBankAccountItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.bank_account_item, parent, false);
    }

    static void bindBankAccountItemView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == BANK_NAME) {
            TextView bankName = view.findViewById(R.id.bank_name);
            bankName.setText(model.get(BANK_NAME));
        } else if (propertyKey == BANK_ACCOUNT_SUMMARY) {
            TextView bankAccountSummary = view.findViewById(R.id.bank_account_summary);
            bankAccountSummary.setText(model.get(BANK_ACCOUNT_SUMMARY));
        } else if (propertyKey == BANK_ACCOUNT_TRANSACTION_LIMIT) {
            TextView transactionLimit = view.findViewById(R.id.bank_account_additional_info);
            transactionLimit.setText(model.get(BANK_ACCOUNT_TRANSACTION_LIMIT));
        } else if (propertyKey == BANK_ACCOUNT_DRAWABLE_ID) {
            ImageView bankAccountIcon = view.findViewById(R.id.bank_account_icon);
            bankAccountIcon.setImageDrawable(
                    AppCompatResources.getDrawable(
                            view.getContext(), model.get(BANK_ACCOUNT_DRAWABLE_ID)));
        } else if (propertyKey == BANK_ACCOUNT_ICON_BITMAP) {
            ImageView bankAccountIcon = view.findViewById(R.id.bank_account_icon);
            bankAccountIcon.setImageBitmap(model.get(BANK_ACCOUNT_ICON_BITMAP));
        } else if (propertyKey == ON_BANK_ACCOUNT_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_BANK_ACCOUNT_CLICK_ACTION).run());
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private BankAccountViewBinder() {}
}

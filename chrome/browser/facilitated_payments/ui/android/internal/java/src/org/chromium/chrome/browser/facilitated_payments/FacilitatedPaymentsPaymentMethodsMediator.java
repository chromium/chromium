// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.AdditionalInfoProperties.DESCRIPTION_1_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_DRAWABLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_SUMMARY;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.DESCRIPTION_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.TITLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.BANK_ACCOUNT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VISIBLE;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsComponent.Delegate;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.AdditionalInfoProperties;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties;
import org.chromium.components.autofill.payments.AccountType;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Contains the logic for the facilitated payments component. It sets the state of the model and
 * reacts to events like clicks.
 */
class FacilitatedPaymentsPaymentMethodsMediator {
    private Context mContext;
    private PropertyModel mModel;
    private Delegate mDelegate;

    void initialize(Context context, PropertyModel model, Delegate delegate) {
        mContext = context;
        mModel = model;
        mDelegate = delegate;
    }

    boolean showSheet(List<BankAccount> bankAccounts) {
        if (bankAccounts == null || bankAccounts.isEmpty()) {
            return false;
        }

        ModelList sheetItems = mModel.get(SHEET_ITEMS);
        sheetItems.clear();

        sheetItems.add(buildHeader());

        for (BankAccount bankAccount : bankAccounts) {
            final PropertyModel model = createBankAccountModel(mContext, bankAccount);
            sheetItems.add(new ListItem(BANK_ACCOUNT, model));
        }

        sheetItems.add(buildAdditionalInfo());

        mModel.set(VISIBLE, true);

        return true;
    }

    private ListItem buildHeader() {
        return new ListItem(
                FacilitatedPaymentsPaymentMethodsProperties.ItemType.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(DESCRIPTION_ID, R.string.pix_payment_methods_bottom_sheet_description)
                        .with(IMAGE_DRAWABLE_ID, R.drawable.pix_gpay_logo)
                        .with(TITLE_ID, R.string.pix_payment_methods_bottom_sheet_title)
                        .build());
    }

    @VisibleForTesting
    static ListItem buildAdditionalInfo() {
        return new ListItem(
                FacilitatedPaymentsPaymentMethodsProperties.ItemType.ADDITIONAL_INFO,
                new PropertyModel.Builder(AdditionalInfoProperties.ALL_KEYS)
                        .with(
                                DESCRIPTION_1_ID,
                                R.string.pix_payment_transaction_exceeding_balance_note)
                        .build());
    }

    @VisibleForTesting
    static PropertyModel createBankAccountModel(Context context, BankAccount bankAccount) {
        PropertyModel.Builder bankAccountModelBuilder =
                new PropertyModel.Builder(BankAccountProperties.NON_TRANSFORMING_KEYS)
                        .with(BANK_NAME, bankAccount.getBankName())
                        .with(
                                BANK_ACCOUNT_SUMMARY,
                                getBankAccountSummaryString(context, bankAccount))
                        .with(BANK_ACCOUNT_DRAWABLE_ID, R.drawable.ic_account_balance);
        return bankAccountModelBuilder.build();
    }

    private static String getBankAccountSummaryString(Context context, BankAccount bankAccount) {
        return context.getResources()
                .getString(
                        R.string.settings_pix_bank_account_identifer,
                        getBankAccountTypeString(context, bankAccount.getAccountType()),
                        bankAccount.getAccountNumberSuffix());
    }

    private static String getBankAccountTypeString(
            Context context, @AccountType int bankAccountType) {
        switch (bankAccountType) {
            case AccountType.CHECKING:
                return context.getResources().getString(R.string.bank_account_type_checking);
            case AccountType.SAVINGS:
                return context.getResources().getString(R.string.bank_account_type_savings);
            case AccountType.CURRENT:
                return context.getResources().getString(R.string.bank_account_type_current);
            case AccountType.SALARY:
                return context.getResources().getString(R.string.bank_account_type_salary);
            case AccountType.TRANSACTING_ACCOUNT:
                return context.getResources().getString(R.string.bank_account_type_transacting);
            case AccountType.UNKNOWN:
            default:
                return "";
        }
    }
}

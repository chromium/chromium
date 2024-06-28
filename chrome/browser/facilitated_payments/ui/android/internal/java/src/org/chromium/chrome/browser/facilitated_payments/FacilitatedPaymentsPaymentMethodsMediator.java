// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.AdditionalInfoProperties.DESCRIPTION_1_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_DRAWABLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_ICON_BITMAP;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_SUMMARY;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.ON_BANK_ACCOUNT_CLICK_ACTION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.FopSelectorProperties.SCREEN_ITEMS;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.DESCRIPTION_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.TITLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.BANK_ACCOUNT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.ItemType.CONTINUE_BUTTON;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN_VIEW_MODEL;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.FOP_SELECTOR;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.PROGRESS_SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VISIBLE;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsComponent.Delegate;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.AdditionalInfoProperties;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.payments.AccountType;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.payments.InputProtector;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.Optional;
import java.util.stream.StreamSupport;

/**
 * Contains the logic for the facilitated payments component. It sets the state of the model and
 * reacts to events like clicks.
 */
class FacilitatedPaymentsPaymentMethodsMediator {
    private Context mContext;
    private PropertyModel mModel;
    private Delegate mDelegate;
    private Profile mProfile;
    private InputProtector mInputProtector = new InputProtector();

    void initialize(Context context, PropertyModel model, Delegate delegate, Profile profile) {
        mContext = context;
        mModel = model;
        mDelegate = delegate;
        mProfile = profile;
    }

    boolean showSheet(List<BankAccount> bankAccounts) {
        mInputProtector.markShowTime();
        if (bankAccounts == null || bankAccounts.isEmpty()) {
            return false;
        }

        mModel.set(SCREEN, FOP_SELECTOR);
        ModelList screenItems = mModel.get(SCREEN_VIEW_MODEL).get(SCREEN_ITEMS);
        screenItems.clear();

        for (BankAccount bankAccount : bankAccounts) {
            final PropertyModel model = createBankAccountModel(mContext, bankAccount);
            screenItems.add(new ListItem(BANK_ACCOUNT, model));
        }

        screenItems.add(buildAdditionalInfo());

        maybeShowContinueButton(screenItems);

        screenItems.add(0, buildHeader());

        mModel.set(VISIBLE, true);
        mInputProtector.markShowTime();

        return true;
    }

    void showProgressScreen() {
        mModel.set(SCREEN, PROGRESS_SCREEN);
        mModel.set(VISIBLE, true);
    }

    public void onDismissed(@StateChangeReason int reason) {
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);
        mDelegate.onDismissed();
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
    PropertyModel createBankAccountModel(Context context, BankAccount bankAccount) {
        PropertyModel.Builder bankAccountModelBuilder =
                new PropertyModel.Builder(BankAccountProperties.NON_TRANSFORMING_KEYS)
                        .with(BANK_NAME, bankAccount.getBankName())
                        .with(
                                BANK_ACCOUNT_SUMMARY,
                                getBankAccountSummaryString(context, bankAccount))
                        .with(BANK_ACCOUNT_DRAWABLE_ID, R.drawable.ic_account_balance)
                        .with(
                                ON_BANK_ACCOUNT_CLICK_ACTION,
                                () -> this.onBankAccountSelected(bankAccount));
        Optional<Bitmap> bankIconOptional = Optional.empty();
        if (bankAccount.getDisplayIconUrl() != null && bankAccount.getDisplayIconUrl().isValid()) {
            bankIconOptional =
                    PersonalDataManagerFactory.getForProfile(mProfile)
                            .getCustomImageForAutofillSuggestionIfAvailable(
                                    bankAccount.getDisplayIconUrl(),
                                    AutofillUiUtils.CardIconSpecs.create(
                                            context, AutofillUiUtils.CardIconSize.LARGE));
        }
        if (bankIconOptional.isPresent()) {
            bankAccountModelBuilder.with(BANK_ACCOUNT_ICON_BITMAP, bankIconOptional.get());
        } else {
            bankAccountModelBuilder.with(BANK_ACCOUNT_DRAWABLE_ID, R.drawable.ic_account_balance);
        }
        return bankAccountModelBuilder.build();
    }

    public void onBankAccountSelected(BankAccount bankAccount) {
        if (!mInputProtector.shouldInputBeProcessed()) return;
        mDelegate.onBankAccountSelected(bankAccount.getInstrumentId());
    }

    @VisibleForTesting
    static String getBankAccountSummaryString(Context context, BankAccount bankAccount) {
        return context.getResources()
                .getString(
                        R.string.settings_pix_bank_account_identifer,
                        getBankAccountTypeString(context, bankAccount.getAccountType()),
                        bankAccount.getAccountNumberSuffix());
    }

    @VisibleForTesting
    static String getBankAccountTypeString(Context context, @AccountType int bankAccountType) {
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

    private void maybeShowContinueButton(ModelList screenItems) {
        if (StreamSupport.stream(screenItems.spliterator(), false)
                        .filter(item -> item.type == BANK_ACCOUNT)
                        .count()
                != 1) {
            return;
        }
        PropertyModel model =
                StreamSupport.stream(screenItems.spliterator(), false)
                        .filter(item -> item.type == BANK_ACCOUNT)
                        .findFirst()
                        .get()
                        .model;
        screenItems.add(new ListItem(CONTINUE_BUTTON, model));
    }

    void setInputProtectorForTesting(InputProtector inputProtector) {
        mInputProtector = inputProtector;
    }
}

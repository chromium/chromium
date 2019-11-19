// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;
import org.chromium.chrome.browser.payments.CardEditor;

import java.util.List;

/**
 * The payment method section of the Autofill Assistant payment request.
 */
public class AssistantPaymentMethodSection
        extends AssistantCollectUserDataSection<AutofillPaymentInstrument> {
    private CardEditor mEditor;
    private boolean mIgnorePaymentMethodsChangeNotifications;
    private boolean mRequiresBillingPostalCode;
    private String mBillingPostalCodeMissingText;

    AssistantPaymentMethodSection(Context context, ViewGroup parent) {
        super(context, parent, R.layout.autofill_assistant_payment_method_summary,
                R.layout.autofill_assistant_payment_method_full,
                context.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_payment_request_payment_method_title_padding),
                context.getString(R.string.payments_add_card),
                context.getString(R.string.payments_add_card));
        setTitle(context.getString(R.string.payments_method_of_payment_label));
    }

    public void setEditor(CardEditor editor) {
        mEditor = editor;
        if (mEditor == null) {
            return;
        }

        PersonalDataManager personalDataManager = PersonalDataManager.getInstance();
        for (AutofillPaymentInstrument method : getItems()) {
            String guid = method.getCard().getBillingAddressId();
            PersonalDataManager.AutofillProfile profile = personalDataManager.getProfile(guid);
            if (profile != null) {
                addAutocompleteInformationToEditor(profile);
            }
        }
    }

    @Override
    protected void createOrEditItem(@Nullable AutofillPaymentInstrument oldItem) {
        if (mEditor == null) {
            return;
        }
        mEditor.edit(oldItem, newItem -> {
            assert (newItem != null && newItem.isComplete());
            mIgnorePaymentMethodsChangeNotifications = true;
            addOrUpdateItem(newItem, true);
            mIgnorePaymentMethodsChangeNotifications = false;
        }, cancel -> {});
    }

    @Override
    protected void updateFullView(View fullView, AutofillPaymentInstrument method) {
        if (method == null) {
            return;
        }
        updateSummaryView(fullView, method);
        TextView cardNameView = fullView.findViewById(R.id.credit_card_name);
        cardNameView.setText(method.getCard().getName());
        hideIfEmpty(cardNameView);
    }

    @Override
    protected void updateSummaryView(View summaryView, AutofillPaymentInstrument method) {
        if (method == null) {
            return;
        }

        ImageView cardIssuerImageView = summaryView.findViewById(R.id.credit_card_issuer_icon);
        try {
            cardIssuerImageView.setImageDrawable(
                    summaryView.getContext().getResources().getDrawable(
                            method.getCard().getIssuerIconDrawableId()));
        } catch (Resources.NotFoundException e) {
            cardIssuerImageView.setImageDrawable(null);
        }

        /**
         * By default, the obfuscated number contains the issuer (e.g., 'Visa'). This is needlessly
         * verbose, so we strip it away. See |PersonalDataManagerTest::testAddAndEditCreditCards|
         * for explanation of "\u0020...\u2060".
         */
        String obfuscatedNumber = method.getCard().getObfuscatedNumber();
        int beginningOfObfuscatedNumber =
                Math.max(obfuscatedNumber.indexOf("\u0020\u202A\u2022\u2060"), 0);
        obfuscatedNumber = obfuscatedNumber.substring(beginningOfObfuscatedNumber);
        TextView cardNumberView = summaryView.findViewById(R.id.credit_card_number);
        cardNumberView.setText(obfuscatedNumber);
        hideIfEmpty(cardNumberView);

        TextView cardExpirationView = summaryView.findViewById(R.id.credit_card_expiration);
        cardExpirationView.setText(
                method.getCard().getFormattedExpirationDate(summaryView.getContext()));
        hideIfEmpty(cardExpirationView);

        TextView methodIncompleteView = summaryView.findViewById(R.id.incomplete_error);
        setIncompleteErrorMessage(methodIncompleteView, method);
        hideIfEmpty(methodIncompleteView);
    }

    @Override
    protected boolean canEditOption(AutofillPaymentInstrument method) {
        return true;
    }

    @Override
    protected @DrawableRes int getEditButtonDrawable(AutofillPaymentInstrument method) {
        return R.drawable.ic_edit_24dp;
    }

    @Override
    protected String getEditButtonContentDescription(AutofillPaymentInstrument method) {
        return mContext.getString(R.string.autofill_edit_credit_card);
    }

    void onProfilesChanged(List<PersonalDataManager.AutofillProfile> profiles) {
        // TODO(crbug.com/806868): replace suggested billing addresses (remove if necessary).
        for (PersonalDataManager.AutofillProfile profile : profiles) {
            addAutocompleteInformationToEditor(profile);
        }
    }

    /**
     * The set of available payment methods has changed externally. This will rebuild the UI with
     * the new/changed set of payment methods, while keeping the selected item if possible.
     */
    void onAvailablePaymentMethodsChanged(List<AutofillPaymentInstrument> paymentMethods) {
        if (mIgnorePaymentMethodsChangeNotifications) {
            return;
        }
        AutofillPaymentInstrument previouslySelectedMethod = mSelectedOption;
        int selectedMethodIndex = -1;
        for (int i = 0; i < paymentMethods.size(); i++) {
            if (previouslySelectedMethod != null
                    && TextUtils.equals(paymentMethods.get(i).getIdentifier(),
                            previouslySelectedMethod.getIdentifier())) {
                selectedMethodIndex = i;
            }
        }

        // Replace current set of items, keep selection if possible.
        setItems(paymentMethods, selectedMethodIndex);
    }

    void setRequiresBillingPostalCode(boolean requiresBillingPostalCode) {
        mRequiresBillingPostalCode = requiresBillingPostalCode;
    }

    void setBillingPostalCodeMissingText(String text) {
        mBillingPostalCodeMissingText = text;
    }

    private void addAutocompleteInformationToEditor(PersonalDataManager.AutofillProfile profile) {
        // The check for non-null label is necessary to prevent crash in editor when opening.
        if (mEditor == null || profile.getLabel() == null) {
            return;
        }
        mEditor.updateBillingAddressIfComplete(new AutofillAddress(mContext, profile));
    }

    private boolean hasAllRequiredFields(AutofillPaymentInstrument method) {
        if (!method.isComplete()) {
            return false;
        }

        if (!mRequiresBillingPostalCode) {
            return true;
        }

        if (method.getBillingProfile() == null
                || TextUtils.isEmpty(method.getBillingProfile().getPostalCode())) {
            return false;
        }

        return true;
    }

    private void setIncompleteErrorMessage(
            TextView methodIncompleteView, AutofillPaymentInstrument method) {
        if (hasAllRequiredFields(method)) {
            methodIncompleteView.setText("");
            return;
        }

        // we have to show an error message either because the payment method is incomplete (missing
        // information), or because a postcode is required and the billing address does not have
        // one.
        if (method.isComplete()) {
            methodIncompleteView.setText(mBillingPostalCodeMissingText);
        } else {
            methodIncompleteView.setText(R.string.autofill_assistant_payment_information_missing);
        }
    }
}
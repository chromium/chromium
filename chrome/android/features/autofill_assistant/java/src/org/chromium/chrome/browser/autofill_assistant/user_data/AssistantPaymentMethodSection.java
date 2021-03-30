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
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.settings.CardEditor;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;

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
    private String mCreditCardExpiredText;

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
                addAutocompleteInformationToEditor(new AutofillAddress(mContext, profile));
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

        updateView(fullView, method);

        TextView cardNameView = fullView.findViewById(R.id.credit_card_name);
        cardNameView.setText(method.getCard().getName());
        hideIfEmpty(cardNameView);
    }

    @Override
    protected void updateSummaryView(View summaryView, AutofillPaymentInstrument method) {
        if (method == null) {
            return;
        }

        updateView(summaryView, method);
    }

    private void updateView(View view, AutofillPaymentInstrument method) {
        ImageView cardIssuerImageView = view.findViewById(R.id.credit_card_issuer_icon);
        try {
            cardIssuerImageView.setImageDrawable(view.getContext().getResources().getDrawable(
                    method.getCard().getIssuerIconDrawableId()));
        } catch (Resources.NotFoundException e) {
            cardIssuerImageView.setImageDrawable(null);
        }

        // By default, the obfuscated number contains the issuer (e.g., 'Visa'). This is needlessly
        // verbose, so we strip it away. See |PersonalDataManagerTest::testAddAndEditCreditCards|
        // for explanation of "\u0020...\u2060".
        String obfuscatedNumber = method.getCard().getObfuscatedNumber();
        int beginningOfObfuscatedNumber =
                Math.max(obfuscatedNumber.indexOf("\u0020\u202A\u2022\u2060"), 0);
        obfuscatedNumber = obfuscatedNumber.substring(beginningOfObfuscatedNumber);
        TextView cardNumberView = view.findViewById(R.id.credit_card_number);
        cardNumberView.setText(obfuscatedNumber);
        hideIfEmpty(cardNumberView);

        TextView cardExpirationView = view.findViewById(R.id.credit_card_expiration);
        cardExpirationView.setText(method.getCard().getFormattedExpirationDate(view.getContext()));
        hideIfEmpty(cardExpirationView);

        TextView errorMessageView = view.findViewById(R.id.incomplete_error);
        setErrorMessage(errorMessageView, method);
        hideIfEmpty(errorMessageView);
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

    @Override
    protected boolean areEqual(@Nullable AutofillPaymentInstrument optionA,
            @Nullable AutofillPaymentInstrument optionB) {
        if (optionA == null || optionB == null) {
            return optionA == optionB;
        }
        if (TextUtils.equals(optionA.getIdentifier(), optionB.getIdentifier())) {
            return true;
        }
        return areEqualCards(optionA.getCard(), optionB.getCard())
                && areEqualBillingProfiles(
                        optionA.getBillingProfile(), optionB.getBillingProfile());
    }
    private boolean areEqualCards(CreditCard cardA, CreditCard cardB) {
        // TODO(crbug.com/806868): Implement better check for the case where PDM is disabled, we
        //  won't have IDs.
        return TextUtils.equals(cardA.getGUID(), cardB.getGUID());
    }
    private boolean areEqualBillingProfiles(
            @Nullable AutofillProfile profileA, @Nullable AutofillProfile profileB) {
        if (profileA == null || profileB == null) {
            return profileA == profileB;
        }
        // TODO(crbug.com/806868): Implement better check for the case where PDM is disabled, we
        //  won't have IDs.
        return TextUtils.equals(profileA.getGUID(), profileB.getGUID());
    }

    void onAddressesChanged(List<AutofillAddress> addresses) {
        // TODO(crbug.com/806868): replace suggested billing addresses (remove if necessary).
        for (AutofillAddress address : addresses) {
            addAutocompleteInformationToEditor(address);
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

        int selectedMethodIndex = -1;
        if (mSelectedOption != null) {
            for (int i = 0; i < paymentMethods.size(); ++i) {
                if (areEqual(paymentMethods.get(i), mSelectedOption)) {
                    selectedMethodIndex = i;
                    break;
                }
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

    void setCreditCardExpiredText(String text) {
        mCreditCardExpiredText = text;
    }

    private void addAutocompleteInformationToEditor(AutofillAddress address) {
        if (mEditor == null) {
            return;
        }
        if (address.getProfile().getLabel() == null) {
            address.getProfile().setLabel(
                    PersonalDataManager.getInstance().getBillingAddressLabelForPaymentRequest(
                            address.getProfile()));
        }
        mEditor.updateBillingAddressIfComplete(address);
    }

    private void setErrorMessage(TextView errorMessageView, AutofillPaymentInstrument method) {
        // TODO(b/154068342): Remove these granular checks and send the error message directly
        //  from |Controller|.
        if (!method.isComplete() || method.getBillingProfile() == null
                || AutofillAddress.checkAddressCompletionStatus(method.getBillingProfile(),
                           AutofillAddress.CompletenessCheckType.IGNORE_PHONE)
                        != AutofillAddress.CompletionStatus.COMPLETE) {
            errorMessageView.setText(R.string.autofill_assistant_payment_information_missing);
            return;
        }

        if (mRequiresBillingPostalCode
                && TextUtils.isEmpty(method.getBillingProfile().getPostalCode())) {
            errorMessageView.setText(mBillingPostalCodeMissingText);
            return;
        }

        if ((method.getMissingFields()
                    & AutofillPaymentInstrument.CompletionStatus.CREDIT_CARD_EXPIRED)
                == 1) {
            errorMessageView.setText(mCreditCardExpiredText);
            return;
        }

        // Final check to catch things we might have missed above.
        if (!isComplete(method)) {
            errorMessageView.setText(R.string.autofill_assistant_payment_information_missing);
            return;
        }

        errorMessageView.setText("");
    }
}
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
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantCollectUserDataModel.PaymentInstrumentModel;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillAddress.CompletenessCheckType;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;

import java.util.List;

/**
 * The payment method section of the Autofill Assistant payment request.
 */
public class AssistantPaymentMethodSection
        extends AssistantCollectUserDataSection<PaymentInstrumentModel> {
    private CardEditor mEditor;
    private boolean mIgnorePaymentMethodsChangeNotifications;

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

        for (PaymentInstrumentModel item : getItems()) {
            AutofillProfile profile = item.mOption.getBillingProfile();
            if (profile != null) {
                addAutocompleteInformationToEditor(
                        new AutofillAddress(mContext, profile, CompletenessCheckType.IGNORE_PHONE));
            }
        }
    }

    @Override
    protected void createOrEditItem(@Nullable PaymentInstrumentModel oldItem) {
        if (mEditor == null) {
            return;
        }
        mEditor.edit(oldItem == null ? null : oldItem.mOption, paymentInstrument -> {
            assert (paymentInstrument != null && paymentInstrument.isComplete());
            mIgnorePaymentMethodsChangeNotifications = true;
            addOrUpdateItem(new PaymentInstrumentModel(paymentInstrument), /* select= */ true,
                    /* notify= */ true);
            mIgnorePaymentMethodsChangeNotifications = false;
        }, cancel -> {});
    }

    @Override
    protected void updateFullView(View fullView, PaymentInstrumentModel model) {
        if (model == null) {
            return;
        }

        updateView(fullView, model);

        TextView cardNameView = fullView.findViewById(R.id.credit_card_name);
        cardNameView.setText(model.mOption.getCard().getName());
        hideIfEmpty(cardNameView);

        TextView errorView = fullView.findViewById(R.id.incomplete_error);
        if (model.mErrors.isEmpty()) {
            errorView.setText("");
            errorView.setVisibility(View.GONE);
        } else {
            errorView.setText(TextUtils.join("\n", model.mErrors));
            errorView.setVisibility(View.VISIBLE);
        }
    }

    @Override
    protected void updateSummaryView(View summaryView, PaymentInstrumentModel model) {
        if (model == null) {
            return;
        }

        updateView(summaryView, model);

        TextView errorView = summaryView.findViewById(R.id.incomplete_error);
        errorView.setVisibility(model.mErrors.isEmpty() ? View.GONE : View.VISIBLE);
    }

    private void updateView(View view, PaymentInstrumentModel model) {
        AutofillPaymentInstrument method = model.mOption;
        ImageView cardIssuerImageView = view.findViewById(R.id.credit_card_issuer_icon);
        try {
            cardIssuerImageView.setImageDrawable(
                    view.getContext().getDrawable(method.getCard().getIssuerIconDrawableId()));
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
    }

    @Override
    protected boolean canEditOption(PaymentInstrumentModel model) {
        return true;
    }

    @Override
    protected @DrawableRes int getEditButtonDrawable(PaymentInstrumentModel model) {
        return R.drawable.ic_edit_24dp;
    }

    @Override
    protected String getEditButtonContentDescription(PaymentInstrumentModel model) {
        return mContext.getString(R.string.autofill_edit_credit_card);
    }

    @Override
    protected boolean areEqual(
            @Nullable PaymentInstrumentModel modelA, @Nullable PaymentInstrumentModel modelB) {
        if (modelA == null || modelB == null) {
            return modelA == modelB;
        }
        AutofillPaymentInstrument optionA = modelA.mOption;
        AutofillPaymentInstrument optionB = modelB.mOption;
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
    void onAvailablePaymentMethodsChanged(List<PaymentInstrumentModel> paymentMethods) {
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
}
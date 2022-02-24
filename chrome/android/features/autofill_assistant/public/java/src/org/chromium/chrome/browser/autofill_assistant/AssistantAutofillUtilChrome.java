// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillAddress.CompletenessCheckType;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;
import org.chromium.chrome.browser.payments.ContactEditor;
import org.chromium.components.autofill_assistant.AssistantAutofillCreditCard;
import org.chromium.components.autofill_assistant.AssistantAutofillProfile;
import org.chromium.components.autofill_assistant.AssistantPaymentInstrument;
import org.chromium.components.payments.MethodStrings;
import org.chromium.content_public.browser.WebContents;

/**
 * Utility class for Chrome to handle Autofill / Assistant conversions.
 */
public class AssistantAutofillUtilChrome {
    /**
     * Transform an {@link AssistantAutofillProfile} to an {@link AutofillProfile}.
     *
     * @param profile The {@link AssistantAutofillProfile} to transform.
     * @return The equivalent {@link AutofillProfile}.
     */
    public static AutofillProfile assistantAutofillProfileToAutofillProfile(
            AssistantAutofillProfile profile) {
        return new AutofillProfile(profile.getGUID(), profile.getOrigin(), profile.getIsLocal(),
                profile.getHonorificPrefix(), profile.getFullName(), profile.getCompanyName(),
                profile.getStreetAddress(), profile.getRegion(), profile.getLocality(),
                profile.getDependentLocality(), profile.getPostalCode(), profile.getSortingCode(),
                profile.getCountryCode(), profile.getPhoneNumber(), profile.getEmailAddress(),
                profile.getLanguageCode());
    }

    /**
     * Transform an {@link AutofillProfile} to an {@link AssistantAutofillProfile}.
     *
     * @param profile The {@link AutofillProfile} to transform.
     * @return The equivalent {@link AssistantAutofillProfile}.
     */
    public static AssistantAutofillProfile autofillProfileToAssistantAutofillProfile(
            AutofillProfile profile) {
        return new AssistantAutofillProfile(profile.getGUID(), profile.getOrigin(),
                profile.getIsLocal(), profile.getHonorificPrefix(), profile.getFullName(),
                profile.getCompanyName(), profile.getStreetAddress(), profile.getRegion(),
                profile.getLocality(), profile.getDependentLocality(), profile.getPostalCode(),
                profile.getSortingCode(), profile.getCountryCode(), profile.getPhoneNumber(),
                profile.getEmailAddress(), profile.getLanguageCode());
    }

    /**
     * Transform an {@link AssistantAutofillProfile} into an {@link AutofillContact}.
     *
     * @param profile The {@link AssistantAutofillProfile} to transform.
     * @param context The context the app is currently run as.
     * @param editor The contact editor that holds details about the treatment of the contact.
     * @return The equivalent {@link AutofillContact}.
     */
    public static AutofillContact assistantAutofillProfileToAutofillContact(
            AssistantAutofillProfile profile, Context context, ContactEditor editor) {
        String name = profile.getFullName();
        String phone = profile.getPhoneNumber();
        String email = profile.getEmailAddress();

        return new AutofillContact(context, assistantAutofillProfileToAutofillProfile(profile),
                name, phone, email, editor.checkContactCompletionStatus(name, phone, email),
                editor.getRequestPayerName(), editor.getRequestPayerPhone(),
                editor.getRequestPayerEmail());
    }

    /**
     * Transform an {@link AssistantAutofillProfile} into an {@link AutofillAddress}.
     *
     * @param profile The {@link AssistantAutofillProfile} to transform.
     * @param context The context the app is currently run as.
     * @return The equivalent {@link AutofillAddress}.
     */
    public static AutofillAddress assistantAutofillProfileToAutofillAddress(
            AssistantAutofillProfile profile, Context context) {
        return new AutofillAddress(context, assistantAutofillProfileToAutofillProfile(profile),
                CompletenessCheckType.IGNORE_PHONE);
    }

    private static CreditCard assistantAutofillCreditCardToAutofillCreditCard(
            AssistantAutofillCreditCard creditCard) {
        return new CreditCard(creditCard.getGUID(), creditCard.getOrigin(), creditCard.getIsLocal(),
                creditCard.getIsCached(), creditCard.getName(), creditCard.getNumber(),
                creditCard.getObfuscatedNumber(), creditCard.getMonth(), creditCard.getYear(),
                creditCard.getBasicCardIssuerNetwork(), creditCard.getIssuerIconDrawableId(),
                creditCard.getBillingAddressId(), creditCard.getServerId(),
                creditCard.getInstrumentId(), /* cardLabel= */ "", creditCard.getNickname(),
                creditCard.getCardArtUrl(), creditCard.getVirtualCardEnrollmentState());
    }

    /**
     * Transform an {@link AssistantPaymentInstrument} into an {@link AutofillPaymentInstrument}.
     *
     * @param paymentInstrument The {@link AssistantPaymentInstrument} to transform.
     * @param webContents The {@link WebContents} associated with this run.
     * @return The equivalent {@link AutofillPaymentInstrument}.
     */
    public static AutofillPaymentInstrument assistantPaymentInstrumentToAutofillPaymentInstrument(
            AssistantPaymentInstrument paymentInstrument, WebContents webContents) {
        @Nullable
        AssistantAutofillProfile assistantBillingProfile = paymentInstrument.getBillingAddress();
        return new AutofillPaymentInstrument(webContents,
                assistantAutofillCreditCardToAutofillCreditCard(paymentInstrument.getCreditCard()),
                assistantBillingProfile == null
                        ? null
                        : assistantAutofillProfileToAutofillProfile(assistantBillingProfile),
                MethodStrings.BASIC_CARD);
    }

    private static AssistantAutofillCreditCard autofillCreditCardToAssistantAutofillCreditCard(
            CreditCard creditCard) {
        return new AssistantAutofillCreditCard(creditCard.getGUID(), creditCard.getOrigin(),
                creditCard.getIsLocal(), creditCard.getIsCached(), creditCard.getName(),
                creditCard.getNumber(), creditCard.getObfuscatedNumber(), creditCard.getMonth(),
                creditCard.getYear(), creditCard.getBasicCardIssuerNetwork(),
                creditCard.getIssuerIconDrawableId(), creditCard.getBillingAddressId(),
                creditCard.getServerId(), creditCard.getInstrumentId(), creditCard.getNickname(),
                creditCard.getCardArtUrl(), creditCard.getVirtualCardEnrollmentState());
    }

    /**
     * Transform an {@link AutofillPaymentInstrument} into an {@link AssistantPaymentInstrument}.
     *
     * @param paymentInstrument The {@link AutofillPaymentInstrument} to transform.
     * @return The equivalent {@link AssistantPaymentInstrument}.
     */
    public static AssistantPaymentInstrument autofillPaymentInstrumentToAssistantPaymentInstrument(
            AutofillPaymentInstrument paymentInstrument) {
        @Nullable
        AutofillProfile autofillBillingProfile = paymentInstrument.getBillingProfile();
        return new AssistantPaymentInstrument(
                autofillCreditCardToAssistantAutofillCreditCard(paymentInstrument.getCard()),
                autofillBillingProfile == null
                        ? null
                        : autofillProfileToAssistantAutofillProfile(autofillBillingProfile));
    }
}

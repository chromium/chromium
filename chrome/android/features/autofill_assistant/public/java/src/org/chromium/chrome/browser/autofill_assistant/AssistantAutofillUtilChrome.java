// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.components.autofill_assistant.AssistantAutofillCreditCard;
import org.chromium.components.autofill_assistant.AssistantAutofillProfile;

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

    private static CreditCard assistantAutofillCreditCardToAutofillCreditCard(
            AssistantAutofillCreditCard creditCard) {
        return new CreditCard(creditCard.getGUID(), creditCard.getOrigin(), creditCard.getIsLocal(),
                creditCard.getIsCached(), creditCard.getName(), creditCard.getNumber(),
                creditCard.getObfuscatedNumber(), creditCard.getMonth(), creditCard.getYear(),
                creditCard.getBasicCardIssuerNetwork(), creditCard.getIssuerIconDrawableId(),
                creditCard.getBillingAddressId(), creditCard.getServerId(),
                creditCard.getInstrumentId(),
                /* cardLabel= */ "", creditCard.getNickname(), creditCard.getCardArtUrl(),
                creditCard.getVirtualCardEnrollmentState(), creditCard.getProductDescription());
    }

    private static AssistantAutofillCreditCard autofillCreditCardToAssistantAutofillCreditCard(
            CreditCard creditCard) {
        return new AssistantAutofillCreditCard(creditCard.getGUID(), creditCard.getOrigin(),
                creditCard.getIsLocal(), creditCard.getIsCached(), creditCard.getName(),
                creditCard.getNumber(), creditCard.getObfuscatedNumber(), creditCard.getMonth(),
                creditCard.getYear(), creditCard.getBasicCardIssuerNetwork(),
                creditCard.getIssuerIconDrawableId(), creditCard.getBillingAddressId(),
                creditCard.getServerId(), creditCard.getInstrumentId(), creditCard.getNickname(),
                creditCard.getCardArtUrl(), creditCard.getVirtualCardEnrollmentState(),
                creditCard.getProductDescription());
    }

}

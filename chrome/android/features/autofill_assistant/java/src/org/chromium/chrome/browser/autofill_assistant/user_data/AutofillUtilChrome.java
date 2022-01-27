// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import android.content.Context;

import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill_assistant.AssistantAutofillProfile;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillAddress.CompletenessCheckType;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.ContactEditor;

/**
 * Utility class for Chrome to handle Autofill / Assistant conversions.
 */
public class AutofillUtilChrome {
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

    /**
     * Get the label for an {@link AssistantAutofillProfile} used as a shipping address.
     *
     * @param profile The {@link AssistantAutofillProfile}.
     * @param withCountry Flag to add country.
     * @return The label.
     */
    public static String getShippingAddressLabel(
            AssistantAutofillProfile profile, boolean withCountry) {
        if (withCountry) {
            return PersonalDataManager.getInstance()
                    .getShippingAddressLabelWithCountryForPaymentRequest(
                            assistantAutofillProfileToAutofillProfile(profile));
        } else {
            return PersonalDataManager.getInstance()
                    .getShippingAddressLabelWithoutCountryForPaymentRequest(
                            assistantAutofillProfileToAutofillProfile(profile));
        }
    }
}

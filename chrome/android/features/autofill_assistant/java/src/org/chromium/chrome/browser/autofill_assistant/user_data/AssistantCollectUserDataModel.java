// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill_assistant.AssistantInfoPopup;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantAdditionalSectionFactory;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantPopupListSection;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantStaticTextSection;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantTextInputSection;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantTextInputSection.TextInputFactory;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantTextInputType;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;
import org.chromium.chrome.browser.payments.ContactEditor;
import org.chromium.components.payments.MethodStrings;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * State for the header of the Autofill Assistant.
 */
@JNINamespace("autofill_assistant")
public class AssistantCollectUserDataModel extends PropertyModel {
    // TODO(crbug.com/806868): Add |setSelectedLogin|.

    /** Options specifying how to summarize an {@code AutofillContact}. */
    public static class ContactDescriptionOptions {
        public @AssistantContactField int[] mFields;
        public int mMaxNumberLines;
    }

    public static final WritableObjectPropertyKey<AssistantCollectUserDataDelegate> DELEGATE =
            new WritableObjectPropertyKey<>();

    /** The web contents the payment request is associated with. */
    public static final WritableObjectPropertyKey<WebContents> WEB_CONTENTS =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();

    /** The chosen shipping address. */
    public static final WritableObjectPropertyKey<AutofillAddress> SELECTED_SHIPPING_ADDRESS =
            new WritableObjectPropertyKey<>();

    /** The chosen payment method (including billing address). */
    public static final WritableObjectPropertyKey<AutofillPaymentInstrument>
            SELECTED_PAYMENT_INSTRUMENT = new WritableObjectPropertyKey<>();

    /** The chosen contact details. */
    public static final WritableObjectPropertyKey<AutofillContact> SELECTED_CONTACT_DETAILS =
            new WritableObjectPropertyKey<>();

    /** The contact details section title. */
    public static final WritableObjectPropertyKey<String> CONTACT_SECTION_TITLE =
            new WritableObjectPropertyKey<>();

    /** The login section title. */
    public static final WritableObjectPropertyKey<String> LOGIN_SECTION_TITLE =
            new WritableObjectPropertyKey<>();

    /** The chosen login option. */
    public static final WritableObjectPropertyKey<AssistantLoginChoice> SELECTED_LOGIN =
            new WritableObjectPropertyKey<>();

    /** The status of the third party terms & conditions. */
    public static final WritableIntPropertyKey TERMS_STATUS = new WritableIntPropertyKey();

    /** The shipping section title. */
    public static final WritableObjectPropertyKey<String> SHIPPING_SECTION_TITLE =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey REQUEST_NAME = new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey REQUEST_EMAIL = new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey REQUEST_PHONE = new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey REQUEST_SHIPPING_ADDRESS =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey REQUEST_PAYMENT =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<String> ACCEPT_TERMS_AND_CONDITIONS_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey SHOW_TERMS_AS_CHECKBOX =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey REQUEST_LOGIN_CHOICE =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<List<AutofillAddress>>
            AVAILABLE_BILLING_ADDRESSES = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<List<AutofillContact>> AVAILABLE_CONTACTS =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<List<AutofillAddress>>
            AVAILABLE_SHIPPING_ADDRESSES = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<List<AutofillPaymentInstrument>>
            AVAILABLE_PAYMENT_INSTRUMENTS = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<List<String>> SUPPORTED_BASIC_CARD_NETWORKS =
            new WritableObjectPropertyKey<>();

    /** The available login choices. */
    public static final WritableObjectPropertyKey<List<AssistantLoginChoice>> AVAILABLE_LOGINS =
            new WritableObjectPropertyKey<>();

    /** The currently expanded section (may be null). */
    public static final WritableObjectPropertyKey<AssistantVerticalExpander> EXPANDED_SECTION =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey REQUIRE_BILLING_POSTAL_CODE =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<String> BILLING_POSTAL_CODE_MISSING_TEXT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> CREDIT_CARD_EXPIRED_TEXT =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey REQUEST_DATE_RANGE =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<AssistantDateChoiceOptions>
            DATE_RANGE_START_OPTIONS = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<AssistantDateTime> DATE_RANGE_START_DATE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Integer> DATE_RANGE_START_TIMESLOT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> DATE_RANGE_START_DATE_LABEL =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> DATE_RANGE_START_TIME_LABEL =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<AssistantDateChoiceOptions>
            DATE_RANGE_END_OPTIONS = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<AssistantDateTime> DATE_RANGE_END_DATE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Integer> DATE_RANGE_END_TIMESLOT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> DATE_RANGE_END_DATE_LABEL =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> DATE_RANGE_END_TIME_LABEL =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> DATE_RANGE_DATE_NOT_SET_ERROR_MESSAGE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> DATE_RANGE_TIME_NOT_SET_ERROR_MESSAGE =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<List<AssistantAdditionalSectionFactory>>
            PREPENDED_SECTIONS = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<List<AssistantAdditionalSectionFactory>>
            APPENDED_SECTIONS = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> TERMS_REQUIRE_REVIEW_TEXT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> PRIVACY_NOTICE_TEXT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> INFO_SECTION_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey INFO_SECTION_TEXT_CENTER =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<View> GENERIC_USER_INTERFACE_PREPENDED =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<View> GENERIC_USER_INTERFACE_APPENDED =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<ContactDescriptionOptions>
            CONTACT_SUMMARY_DESCRIPTION_OPTIONS = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<ContactDescriptionOptions>
            CONTACT_FULL_DESCRIPTION_OPTIONS = new WritableObjectPropertyKey<>();

    public AssistantCollectUserDataModel() {
        super(DELEGATE, WEB_CONTENTS, VISIBLE, SELECTED_SHIPPING_ADDRESS,
                SELECTED_PAYMENT_INSTRUMENT, SELECTED_CONTACT_DETAILS, CONTACT_SECTION_TITLE,
                LOGIN_SECTION_TITLE, SELECTED_LOGIN, SHIPPING_SECTION_TITLE, TERMS_STATUS,
                REQUEST_NAME, REQUEST_EMAIL, REQUEST_PHONE, REQUEST_SHIPPING_ADDRESS,
                REQUEST_PAYMENT, ACCEPT_TERMS_AND_CONDITIONS_TEXT, SHOW_TERMS_AS_CHECKBOX,
                REQUEST_LOGIN_CHOICE, AVAILABLE_BILLING_ADDRESSES, AVAILABLE_CONTACTS,
                AVAILABLE_SHIPPING_ADDRESSES, AVAILABLE_PAYMENT_INSTRUMENTS,
                SUPPORTED_BASIC_CARD_NETWORKS, AVAILABLE_LOGINS, EXPANDED_SECTION,
                REQUIRE_BILLING_POSTAL_CODE, BILLING_POSTAL_CODE_MISSING_TEXT,
                CREDIT_CARD_EXPIRED_TEXT, REQUEST_DATE_RANGE, DATE_RANGE_START_OPTIONS,
                DATE_RANGE_START_DATE, DATE_RANGE_START_TIMESLOT, DATE_RANGE_START_DATE_LABEL,
                DATE_RANGE_START_TIME_LABEL, DATE_RANGE_END_OPTIONS, DATE_RANGE_END_DATE,
                DATE_RANGE_END_TIMESLOT, DATE_RANGE_END_DATE_LABEL, DATE_RANGE_END_TIME_LABEL,
                DATE_RANGE_DATE_NOT_SET_ERROR_MESSAGE, DATE_RANGE_TIME_NOT_SET_ERROR_MESSAGE,
                PREPENDED_SECTIONS, APPENDED_SECTIONS, TERMS_REQUIRE_REVIEW_TEXT,
                PRIVACY_NOTICE_TEXT, INFO_SECTION_TEXT, INFO_SECTION_TEXT_CENTER,
                GENERIC_USER_INTERFACE_PREPENDED, GENERIC_USER_INTERFACE_APPENDED,
                CONTACT_SUMMARY_DESCRIPTION_OPTIONS, CONTACT_FULL_DESCRIPTION_OPTIONS);

        /**
         * Set initial state for basic type properties (others are implicitly null).
         * This is necessary to ensure that the initial UI state is consistent with the model.
         */
        set(VISIBLE, false);
        set(TERMS_STATUS, AssistantTermsAndConditionsState.NOT_SELECTED);
        set(REQUEST_NAME, false);
        set(REQUEST_EMAIL, false);
        set(REQUEST_PHONE, false);
        set(REQUEST_PAYMENT, false);
        set(REQUEST_SHIPPING_ADDRESS, false);
        set(REQUEST_LOGIN_CHOICE, false);
        set(REQUIRE_BILLING_POSTAL_CODE, false);
        set(PREPENDED_SECTIONS, Collections.emptyList());
        set(APPENDED_SECTIONS, Collections.emptyList());
        set(AVAILABLE_PAYMENT_INSTRUMENTS, Collections.emptyList());
        set(AVAILABLE_CONTACTS, Collections.emptyList());
        set(AVAILABLE_SHIPPING_ADDRESSES, Collections.emptyList());
        set(AVAILABLE_BILLING_ADDRESSES, Collections.emptyList());
        set(INFO_SECTION_TEXT, "");
    }

    @CalledByNative
    private void setRequestName(boolean requestName) {
        set(REQUEST_NAME, requestName);
    }

    @CalledByNative
    private void setRequestEmail(boolean requestEmail) {
        set(REQUEST_EMAIL, requestEmail);
    }

    @CalledByNative
    private void setRequestPhone(boolean requestPhone) {
        set(REQUEST_PHONE, requestPhone);
    }

    @CalledByNative
    private void setRequestShippingAddress(boolean requestShippingAddress) {
        set(REQUEST_SHIPPING_ADDRESS, requestShippingAddress);
    }

    @CalledByNative
    private void setRequestPayment(boolean requestPayment) {
        set(REQUEST_PAYMENT, requestPayment);
    }

    @CalledByNative
    private void setAcceptTermsAndConditionsText(String text) {
        set(ACCEPT_TERMS_AND_CONDITIONS_TEXT, text);
    }

    @CalledByNative
    private void setShowTermsAsCheckbox(boolean showTermsAsCheckbox) {
        set(SHOW_TERMS_AS_CHECKBOX, showTermsAsCheckbox);
    }

    @CalledByNative
    private void setRequireBillingPostalCode(boolean requireBillingPostalCode) {
        set(REQUIRE_BILLING_POSTAL_CODE, requireBillingPostalCode);
    }

    @CalledByNative
    private void setBillingPostalCodeMissingText(String text) {
        set(BILLING_POSTAL_CODE_MISSING_TEXT, text);
    }

    @CalledByNative
    private void setCreditCardExpiredText(String text) {
        set(CREDIT_CARD_EXPIRED_TEXT, text);
    }

    @CalledByNative
    private void setContactSectionTitle(String text) {
        set(CONTACT_SECTION_TITLE, text);
    }

    @CalledByNative
    private void setLoginSectionTitle(String loginSectionTitle) {
        set(LOGIN_SECTION_TITLE, loginSectionTitle);
    }

    @CalledByNative
    private void setRequestLoginChoice(boolean requestLoginChoice) {
        set(REQUEST_LOGIN_CHOICE, requestLoginChoice);
    }

    @CalledByNative
    private void setShippingSectionTitle(String text) {
        set(SHIPPING_SECTION_TITLE, text);
    }

    @CalledByNative
    private void setSupportedBasicCardNetworks(String[] supportedBasicCardNetworks) {
        set(SUPPORTED_BASIC_CARD_NETWORKS, Arrays.asList(supportedBasicCardNetworks));
    }

    @CalledByNative
    private void setVisible(boolean visible) {
        set(VISIBLE, visible);
    }

    @CalledByNative
    private void setTermsStatus(@AssistantTermsAndConditionsState int termsStatus) {
        set(TERMS_STATUS, termsStatus);
    }

    @CalledByNative
    private void setWebContents(WebContents webContents) {
        set(WEB_CONTENTS, webContents);
    }

    @CalledByNative
    private void setDelegate(AssistantCollectUserDataDelegate delegate) {
        set(DELEGATE, delegate);
    }

    @CalledByNative
    private void setSelectedContactDetails(@Nullable AutofillContact contact) {
        set(SELECTED_CONTACT_DETAILS, contact);
    }

    @CalledByNative
    private void setSelectedShippingAddress(@Nullable AutofillAddress shippingAddress) {
        set(SELECTED_SHIPPING_ADDRESS, shippingAddress);
    }

    @CalledByNative
    private void setSelectedPaymentInstrument(WebContents webContents,
            @Nullable PersonalDataManager.CreditCard card,
            @Nullable PersonalDataManager.AutofillProfile billingProfile) {
        set(SELECTED_PAYMENT_INSTRUMENT,
                createAutofillPaymentInstrument(webContents, card, billingProfile));
    }

    /** Creates an empty list of login options. */
    @CalledByNative
    private static List<AssistantLoginChoice> createLoginChoiceList() {
        return new ArrayList<>();
    }

    /** Appends a login choice to {@code loginChoices}. */
    @CalledByNative
    private static void addLoginChoice(List<AssistantLoginChoice> loginChoices, String identifier,
            String label, String sublabel, String sublabelAccessibilityHint, int priority,
            @Nullable AssistantInfoPopup infoPopup, @Nullable String editButtonContentDescription) {
        loginChoices.add(new AssistantLoginChoice(identifier, label, sublabel,
                sublabelAccessibilityHint, priority, infoPopup, editButtonContentDescription));
    }

    /** Sets the list of available login choices. */
    @CalledByNative
    private void setLoginChoices(List<AssistantLoginChoice> loginChoices) {
        set(AVAILABLE_LOGINS, loginChoices);
    }

    @CalledByNative
    private void setRequestDateRange(boolean requestDateRange) {
        set(REQUEST_DATE_RANGE, requestDateRange);
    }

    /** Create an instance of {@code AssistantDateTime}. */
    @CalledByNative
    private static AssistantDateTime createAssistantDateTime(
            int year, int month, int day, int hour, int minute, int second) {
        return new AssistantDateTime(year, month, day, hour, minute, second);
    }

    /** Configures the start of the date/time range. */
    @CalledByNative
    private void setDateTimeRangeStartOptions(
            AssistantDateTime minDate, AssistantDateTime maxDate, String[] timeSlots) {
        AssistantDateChoiceOptions options =
                new AssistantDateChoiceOptions(minDate, maxDate, Arrays.asList(timeSlots));
        set(DATE_RANGE_START_OPTIONS, options);
    }

    /** Configures the end of the date/time range. */
    @CalledByNative
    private void setDateTimeRangeEndOptions(
            AssistantDateTime minDate, AssistantDateTime maxDate, String[] timeSlots) {
        AssistantDateChoiceOptions options =
                new AssistantDateChoiceOptions(minDate, maxDate, Arrays.asList(timeSlots));
        set(DATE_RANGE_END_OPTIONS, options);
    }

    @CalledByNative
    private void setDateTimeRangeStartDate(AssistantDateTime date) {
        set(DATE_RANGE_START_DATE, date);
    }

    @CalledByNative
    private void setDateTimeRangeStartTimeSlot(int timeSlot) {
        set(DATE_RANGE_START_TIMESLOT, timeSlot);
    }

    @CalledByNative
    private void setDateTimeRangeEndDate(AssistantDateTime date) {
        set(DATE_RANGE_END_DATE, date);
    }

    @CalledByNative
    private void setDateTimeRangeEndTimeSlot(int timeSlot) {
        set(DATE_RANGE_END_TIMESLOT, timeSlot);
    }

    @CalledByNative
    private void clearDateTimeRangeStartDate() {
        set(DATE_RANGE_START_DATE, null);
    }

    @CalledByNative
    private void clearDateTimeRangeStartTimeSlot() {
        set(DATE_RANGE_START_TIMESLOT, null);
    }

    @CalledByNative
    private void clearDateTimeRangeEndDate() {
        set(DATE_RANGE_END_DATE, null);
    }

    @CalledByNative
    private void clearDateTimeRangeEndTimeSlot() {
        set(DATE_RANGE_END_TIMESLOT, null);
    }

    @CalledByNative
    private void setDateTimeRangeStartDateLabel(String label) {
        set(DATE_RANGE_START_DATE_LABEL, label);
    }

    @CalledByNative
    private void setDateTimeRangeStartTimeLabel(String label) {
        set(DATE_RANGE_START_TIME_LABEL, label);
    }

    @CalledByNative
    private void setDateTimeRangeEndDateLabel(String label) {
        set(DATE_RANGE_END_DATE_LABEL, label);
    }

    @CalledByNative
    private void setDateTimeRangeEndTimeLabel(String label) {
        set(DATE_RANGE_END_TIME_LABEL, label);
    }

    @CalledByNative
    private void setDateTimeRangeDateNotSetErrorMessage(String message) {
        set(DATE_RANGE_DATE_NOT_SET_ERROR_MESSAGE, message);
    }

    @CalledByNative
    private void setDateTimeRangeTimeNotSetErrorMessage(String message) {
        set(DATE_RANGE_TIME_NOT_SET_ERROR_MESSAGE, message);
    }

    @CalledByNative
    private static List<AssistantAdditionalSectionFactory> createAdditionalSectionsList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static void appendStaticTextSection(
            List<AssistantAdditionalSectionFactory> sections, String title, String text) {
        sections.add(new AssistantStaticTextSection.Factory(title, text));
    }

    @CalledByNative
    private static void appendTextInputSection(List<AssistantAdditionalSectionFactory> sections,
            String title, List<TextInputFactory> inputs) {
        sections.add(new AssistantTextInputSection.Factory(title, inputs));
    }

    @CalledByNative
    private static void appendPopupListSection(List<AssistantAdditionalSectionFactory> sections,
            String title, String identifier, String[] items, int[] initialSelection,
            boolean allowMultiselect, boolean selectionMandatory, String noSelectionErrorMessage) {
        sections.add(new AssistantPopupListSection.Factory(title, identifier, items,
                initialSelection, allowMultiselect, selectionMandatory, noSelectionErrorMessage));
    }

    @CalledByNative
    private static List<TextInputFactory> createTextInputList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static void appendTextInput(List<TextInputFactory> inputs,
            @AssistantTextInputType int type, String hint, String value, String key) {
        inputs.add(new TextInputFactory(type, hint, value, key));
    }

    /** Configures the list of prepended sections. */
    @CalledByNative
    private void setPrependedSections(List<AssistantAdditionalSectionFactory> sections) {
        set(PREPENDED_SECTIONS, sections);
    }

    /** Configures the list of appended sections. */
    @CalledByNative
    private void setAppendedSections(List<AssistantAdditionalSectionFactory> sections) {
        set(APPENDED_SECTIONS, sections);
    }

    @CalledByNative
    private void setTermsRequireReviewText(String text) {
        set(TERMS_REQUIRE_REVIEW_TEXT, text);
    }

    @CalledByNative
    private void setInfoSectionText(String text, boolean center) {
        set(INFO_SECTION_TEXT, text);
        set(INFO_SECTION_TEXT_CENTER, center);
    }

    @CalledByNative
    private void setPrivacyNoticeText(String text) {
        set(PRIVACY_NOTICE_TEXT, text);
    }

    @CalledByNative
    private static List<AutofillContact> createAutofillContactList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static void addAutofillContact(
            List<AutofillContact> contacts, AutofillContact contact) {
        contacts.add(contact);
    }

    @VisibleForTesting
    @CalledByNative
    @Nullable
    public static AutofillContact createAutofillContact(Context context,
            @Nullable PersonalDataManager.AutofillProfile profile, boolean requestName,
            boolean requestPhone, boolean requestEmail) {
        if (profile == null || !(requestName || requestPhone || requestEmail)) {
            return null;
        }
        ContactEditor editor =
                new ContactEditor(requestName, requestPhone, requestEmail, /* saveToDisk= */ false);
        String name = profile.getFullName();
        String phone = profile.getPhoneNumber();
        String email = profile.getEmailAddress();
        return new AutofillContact(context, profile, name, phone, email,
                editor.checkContactCompletionStatus(name, phone, email), requestName, requestPhone,
                requestEmail);
    }

    @CalledByNative
    private void setAvailableContacts(List<AutofillContact> contacts) {
        set(AVAILABLE_CONTACTS, contacts);
    }

    @CalledByNative
    private static List<AutofillAddress> createAutofillAddressList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static void addAutofillAddress(
            List<AutofillAddress> addresses, AutofillAddress address) {
        addresses.add(address);
    }

    @VisibleForTesting
    @CalledByNative
    @Nullable
    public static AutofillAddress createAutofillAddress(
            Context context, @Nullable PersonalDataManager.AutofillProfile profile) {
        if (profile == null) {
            return null;
        }
        return new AutofillAddress(context, profile);
    }

    @CalledByNative
    private void setAvailableShippingAddresses(List<AutofillAddress> addresses) {
        set(AVAILABLE_SHIPPING_ADDRESSES, addresses);
    }

    @CalledByNative
    private void setAvailableBillingAddresses(List<AutofillAddress> addresses) {
        set(AVAILABLE_BILLING_ADDRESSES, addresses);
    }

    @CalledByNative
    private static List<AutofillPaymentInstrument> createAutofillPaymentInstrumentList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static void addAutofillPaymentInstrument(
            List<AutofillPaymentInstrument> paymentInstruments, WebContents webContents,
            @Nullable PersonalDataManager.CreditCard card,
            @Nullable PersonalDataManager.AutofillProfile billingProfile) {
        AutofillPaymentInstrument paymentInstrument =
                createAutofillPaymentInstrument(webContents, card, billingProfile);
        if (paymentInstrument != null) {
            paymentInstruments.add(paymentInstrument);
        }
    }

    // TODO(b/144005336): Call from native instead.
    @VisibleForTesting
    @Nullable
    public static AutofillPaymentInstrument createAutofillPaymentInstrument(WebContents webContents,
            @Nullable PersonalDataManager.CreditCard card,
            @Nullable PersonalDataManager.AutofillProfile billingProfile) {
        if (webContents == null) {
            return null;
        }
        if (card == null) {
            return null;
        }
        return new AutofillPaymentInstrument(
                webContents, card, billingProfile, MethodStrings.BASIC_CARD);
    }

    @CalledByNative
    private void setAvailablePaymentInstruments(
            List<AutofillPaymentInstrument> paymentInstruments) {
        set(AVAILABLE_PAYMENT_INSTRUMENTS, paymentInstruments);
    }

    @CalledByNative
    private void setGenericUserInterfacePrepended(@Nullable View userInterface) {
        set(GENERIC_USER_INTERFACE_PREPENDED, userInterface);
    }

    @CalledByNative
    private void setGenericUserInterfaceAppended(@Nullable View userInterface) {
        set(GENERIC_USER_INTERFACE_APPENDED, userInterface);
    }

    @CalledByNative
    private static ContactDescriptionOptions createContactDescriptionOptions(
            @AssistantContactField int[] fields, int maxNumberLines) {
        ContactDescriptionOptions options = new ContactDescriptionOptions();
        options.mFields = fields;
        options.mMaxNumberLines = maxNumberLines;
        return options;
    }

    @CalledByNative
    private void setContactSummaryDescriptionOptions(ContactDescriptionOptions options) {
        set(CONTACT_SUMMARY_DESCRIPTION_OPTIONS, options);
    }

    @CalledByNative
    private void setContactFullDescriptionOptions(ContactDescriptionOptions options) {
        set(CONTACT_FULL_DESCRIPTION_OPTIONS, options);
    }
}

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DONE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_FIELDS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_TITLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.SHOW_REQUIRED_INDICATOR;

import android.telephony.PhoneNumberUtils;
import android.text.TextUtils;
import android.util.Patterns;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.StrictModeContext;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PhoneNumberUtil;
import org.chromium.chrome.browser.autofill.editors.EditorBase;
import org.chromium.chrome.browser.autofill.editors.EditorFieldModel;
import org.chromium.chrome.browser.autofill.editors.EditorFieldModel.EditorFieldValidator;
import org.chromium.chrome.browser.autofill.editors.EditorFieldModel.TextInputType;
import org.chromium.payments.mojom.PayerErrors;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.UUID;

/**
 * Contact information editor.
 */
public class ContactEditor extends EditorBase<AutofillContact> {
    // Bit field values are identical to ProfileFields in payments_profile_comparator.h.
    // Please also modify payments_profile_comparator.h after changing these bits.
    public @interface CompletionStatus {}
    /** Can be sent to the merchant as-is without editing first. */
    public static final int COMPLETE = 0;
    /** The contact name is missing. */
    public static final int INVALID_NAME = 1 << 0;
    /** The contact phone number is invalid or missing. */
    public static final int INVALID_PHONE_NUMBER = 1 << 1;
    /** The contact email is invalid or missing. */
    public static final int INVALID_EMAIL = 1 << 2;

    private final boolean mRequestPayerName;
    private final boolean mRequestPayerPhone;
    private final boolean mRequestPayerEmail;
    private final boolean mSaveToDisk;
    private final Set<CharSequence> mPayerNames;
    private final Set<CharSequence> mPhoneNumbers;
    private final Set<CharSequence> mEmailAddresses;
    @Nullable private PayerErrors mPayerErrors;
    @Nullable private EditorFieldValidator mPhoneValidator;
    @Nullable private EditorFieldValidator mEmailValidator;

    /**
     * Builds a contact information editor.
     *
     * @param requestPayerName  Whether to request the user's name.
     * @param requestPayerPhone Whether to request the user's phone number.
     * @param requestPayerEmail Whether to request the user's email address.
     * @param saveToDisk        Whether to save changes to disk.
     */
    public ContactEditor(boolean requestPayerName, boolean requestPayerPhone,
            boolean requestPayerEmail, boolean saveToDisk) {
        assert requestPayerName || requestPayerPhone || requestPayerEmail;
        mRequestPayerName = requestPayerName;
        mRequestPayerPhone = requestPayerPhone;
        mRequestPayerEmail = requestPayerEmail;
        mSaveToDisk = saveToDisk;
        mPayerNames = new HashSet<>();
        mPhoneNumbers = new HashSet<>();
        mEmailAddresses = new HashSet<>();
    }

    /**
     * @return Whether this editor requires the payer name.
     */
    public boolean getRequestPayerName() {
        return mRequestPayerName;
    }

    /**
     * @return Whether this editor requires the payer phone.
     */
    public boolean getRequestPayerPhone() {
        return mRequestPayerPhone;
    }

    /**
     * @return Whether this editor requires the payer email.
     */
    public boolean getRequestPayerEmail() {
        return mRequestPayerEmail;
    }

    /**
     * Returns the contact completion status with the given name, phone and email.
     *
     * @param name  The payer name to check.
     * @param phone The phone number to check.
     * @param email The email address to check.
     * @return The completion status.
     */
    @CompletionStatus
    public int checkContactCompletionStatus(
            @Nullable String name, @Nullable String phone, @Nullable String email) {
        int completionStatus = COMPLETE;

        if (mRequestPayerName && TextUtils.isEmpty(name)) {
            completionStatus |= INVALID_NAME;
        }

        if (mRequestPayerPhone && !getPhoneValidator().isValid(phone)) {
            completionStatus |= INVALID_PHONE_NUMBER;
        }

        if (mRequestPayerEmail && !getEmailValidator().isValid(email)) {
            completionStatus |= INVALID_EMAIL;
        }

        return completionStatus;
    }

    /**
     * Adds the given payer name to the autocomplete set, if it's valid.
     *
     * @param payerName The payer name to possibly add.
     */
    public void addPayerNameIfValid(@Nullable CharSequence payerName) {
        if (!TextUtils.isEmpty(payerName)) mPayerNames.add(payerName);
    }

    /**
     * Adds the given phone number to the autocomplete set, if it's valid.
     *
     * @param phoneNumber The phone number to possibly add.
     */
    public void addPhoneNumberIfValid(@Nullable CharSequence phoneNumber) {
        if (getPhoneValidator().isValid(phoneNumber)) mPhoneNumbers.add(phoneNumber);
    }

    /**
     * Adds the given email address to the autocomplete set, if it's valid.
     *
     * @param emailAddress The email address to possibly add.
     */
    public void addEmailAddressIfValid(@Nullable CharSequence emailAddress) {
        if (getEmailValidator().isValid(emailAddress)) mEmailAddresses.add(emailAddress);
    }

    /**
     * Sets the payer errors to indicate error messages from merchant's retry() call.
     *
     * @param errors The payer errors from merchant's retry() call.
     */
    public void setPayerErrors(@Nullable PayerErrors errors) {
        mPayerErrors = errors;
    }

    /**
     * Allows calling |edit| with a single callback used for both 'done' and 'cancel'.
     * @see #edit(AutofillContact, Callback, Callback)
     */
    public void edit(
            @Nullable final AutofillContact toEdit, final Callback<AutofillContact> callback) {
        edit(toEdit, callback, callback);
    }

    @Override
    public void edit(@Nullable final AutofillContact toEdit,
            final Callback<AutofillContact> doneCallback,
            final Callback<AutofillContact> cancelCallback) {
        super.edit(toEdit, doneCallback, cancelCallback);

        final AutofillContact contact = toEdit == null
                ? new AutofillContact(mContext, AutofillProfile.builder().build(), null, null, null,
                        INVALID_NAME | INVALID_PHONE_NUMBER | INVALID_EMAIL, mRequestPayerName,
                        mRequestPayerPhone, mRequestPayerEmail)
                : toEdit;

        final EditorFieldModel nameField = mRequestPayerName
                ? EditorFieldModel.createTextInput(TextInputType.PERSON_NAME_INPUT,
                        mContext.getString(R.string.payments_name_field_in_contact_details),
                        mPayerNames, /*formatter=*/null, /*validator=*/null,
                        mContext.getString(
                                R.string.pref_edit_dialog_field_required_validation_message),
                        null, EditorFieldModel.LENGTH_COUNTER_LIMIT_NONE, contact.getPayerName())
                : null;

        final EditorFieldModel phoneField = mRequestPayerPhone
                ? EditorFieldModel.createTextInput(TextInputType.PHONE_NUMBER_INPUT,
                        mContext.getString(R.string.autofill_profile_editor_phone_number),
                        mPhoneNumbers, new PhoneNumberUtil.CountryAwareFormatTextWatcher(),
                        getPhoneValidator(),
                        mContext.getString(
                                R.string.pref_edit_dialog_field_required_validation_message),
                        mContext.getString(R.string.payments_phone_invalid_validation_message),
                        EditorFieldModel.LENGTH_COUNTER_LIMIT_NONE, contact.getPayerPhone())
                : null;

        final EditorFieldModel emailField = mRequestPayerEmail
                ? EditorFieldModel.createTextInput(TextInputType.EMAIL_ADDRESS_INPUT,
                        mContext.getString(R.string.autofill_profile_editor_email_address),
                        mEmailAddresses, null, getEmailValidator(),
                        mContext.getString(
                                R.string.pref_edit_dialog_field_required_validation_message),
                        mContext.getString(R.string.payments_email_invalid_validation_message),
                        EditorFieldModel.LENGTH_COUNTER_LIMIT_NONE, contact.getPayerEmail())
                : null;

        final String editorTitle = toEdit == null
                ? mContext.getString(R.string.payments_add_contact_details_label)
                : toEdit.getEditTitle();

        List<EditorFieldModel> editorFields = new ArrayList<>();
        if (nameField != null) {
            nameField.setCustomErrorMessage(mPayerErrors != null ? mPayerErrors.name : null);
            editorFields.add(nameField);
        }
        if (phoneField != null) {
            phoneField.setCustomErrorMessage(mPayerErrors != null ? mPayerErrors.phone : null);
            editorFields.add(phoneField);
        }
        if (emailField != null) {
            emailField.setCustomErrorMessage(mPayerErrors != null ? mPayerErrors.email : null);
            editorFields.add(emailField);
        }

        // If the user clicks [Cancel], send |toEdit| contact back to the caller, which was the
        // original state (could be null, a complete contact, a partial contact).
        Runnable onCancel = cancelCallback.bind(toEdit);

        Runnable onDone = () -> {
            String name = null;
            String phone = null;
            String email = null;
            AutofillProfile profile = contact.getProfile();

            if (nameField != null) {
                name = nameField.getValue().toString();
                profile.setFullName(name);
            }

            if (phoneField != null) {
                phone = phoneField.getValue().toString();
                profile.setPhoneNumber(phone);
            }

            if (emailField != null) {
                email = emailField.getValue().toString();
                profile.setEmailAddress(email);
            }

            if (mSaveToDisk) {
                profile.setGUID(PersonalDataManager.getInstance().setProfileToLocal(profile));
            }

            if (profile.getGUID().isEmpty()) {
                assert !mSaveToDisk;

                // Set a fake guid for a new temp AutofillProfile.
                profile.setGUID(UUID.randomUUID().toString());
            }

            profile.setIsLocal(true);
            contact.completeContact(profile.getGUID(), name, phone, email);
            doneCallback.onResult(contact);
        };

        PropertyModel editorModel = new PropertyModel.Builder(ALL_KEYS)
                                            .with(EDITOR_TITLE, editorTitle)
                                            .with(SHOW_REQUIRED_INDICATOR, true)
                                            .with(EDITOR_FIELDS, editorFields)
                                            .with(DONE_RUNNABLE, onDone)
                                            .with(CANCEL_RUNNABLE, onCancel)
                                            .build();

        mEditorDialog.show(editorModel);
        if (mPayerErrors != null) mEditorDialog.validateForm();
    }

    private EditorFieldValidator getPhoneValidator() {
        if (mPhoneValidator == null) {
            mPhoneValidator = new EditorFieldValidator() {
                @Override
                public boolean isValid(@Nullable CharSequence value) {
                    // TODO(crbug.com/999286): PhoneNumberUtils internally trigger disk reads for
                    //                         certain devices/configurations.
                    try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                        return value != null
                                && PhoneNumberUtils.isGlobalPhoneNumber(
                                        PhoneNumberUtils.stripSeparators(value.toString()));
                    }
                }

                @Override
                public boolean isLengthMaximum(@Nullable CharSequence value) {
                    return false;
                }
            };
        }
        return mPhoneValidator;
    }

    private EditorFieldValidator getEmailValidator() {
        if (mEmailValidator == null) {
            mEmailValidator = new EditorFieldValidator() {
                @Override
                public boolean isValid(@Nullable CharSequence value) {
                    return value != null && Patterns.EMAIL_ADDRESS.matcher(value).matches();
                }

                @Override
                public boolean isLengthMaximum(@Nullable CharSequence value) {
                    return false;
                }
            };
        }
        return mEmailValidator;
    }
}

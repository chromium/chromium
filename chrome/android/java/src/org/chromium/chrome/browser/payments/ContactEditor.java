// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ALLOW_DELETE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DONE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_FIELDS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_TITLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALIDATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.NOTICE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.TEXT_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.NoticeProperties.IMPORTANT_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.NoticeProperties.NOTICE_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.NoticeProperties.NOTICE_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_FIELD_TYPE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_FORMATTER;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_SUGGESTIONS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.VALIDATE_ON_SHOW;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.VISIBLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.scrollToFieldWithErrorMessage;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.validateForm;

import android.telephony.PhoneNumberUtils;
import android.text.TextUtils;
import android.util.Patterns;

import org.chromium.base.Callback;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PhoneNumberUtil;
import org.chromium.chrome.browser.autofill.editors.EditorBase;
import org.chromium.chrome.browser.autofill.editors.EditorDialogViewBinder;
import org.chromium.chrome.browser.autofill.editors.EditorFieldValidator;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.EditorItem;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.FieldType;
import org.chromium.payments.mojom.PayerErrors;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;
import java.util.UUID;

/** Contact information editor. */
@NullMarked
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
    private final PersonalDataManager mPersonalDataManager;
    private final Set<String> mPayerNames;
    private final Set<String> mPhoneNumbers;
    private final Set<String> mEmailAddresses;
    private @Nullable PayerErrors mPayerErrors;
    private boolean mContactNew;
    private @Nullable AutofillContact mContact;

    private @Nullable PropertyModel mNameField;
    private @Nullable PropertyModel mPhoneField;
    private @Nullable PropertyModel mEmailField;

    private @Nullable Callback<AutofillContact> mDoneCallback;
    private @Nullable Callback<@Nullable AutofillContact> mCancelCallback;
    private boolean mIsShown;

    /**
     * Builds a contact information editor.
     *
     * @param requestPayerName Whether to request the user's name.
     * @param requestPayerPhone Whether to request the user's phone number.
     * @param requestPayerEmail Whether to request the user's email address.
     * @param saveToDisk Whether to save changes to disk.
     * @param personalDataManager The context appropriate PersonalDataManager reference.
     */
    public ContactEditor(
            boolean requestPayerName,
            boolean requestPayerPhone,
            boolean requestPayerEmail,
            boolean saveToDisk,
            PersonalDataManager personalDataManager) {
        assert requestPayerName || requestPayerPhone || requestPayerEmail;
        mRequestPayerName = requestPayerName;
        mRequestPayerPhone = requestPayerPhone;
        mRequestPayerEmail = requestPayerEmail;
        mSaveToDisk = saveToDisk;
        mPersonalDataManager = personalDataManager;
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
    public @CompletionStatus int checkContactCompletionStatus(
            @Nullable String name, @Nullable String phone, @Nullable String email) {
        int completionStatus = COMPLETE;

        if (mRequestPayerName && TextUtils.isEmpty(name)) {
            completionStatus |= INVALID_NAME;
        }

        if (mRequestPayerPhone && !isPhoneValid(phone)) {
            completionStatus |= INVALID_PHONE_NUMBER;
        }

        if (mRequestPayerEmail && !isEmailValid(email)) {
            completionStatus |= INVALID_EMAIL;
        }

        return completionStatus;
    }

    /**
     * Adds the given payer name to the autocomplete set, if it's valid.
     *
     * @param payerName The payer name to possibly add.
     */
    public void addPayerNameIfValid(@Nullable String payerName) {
        if (!TextUtils.isEmpty(payerName)) mPayerNames.add(payerName);
    }

    /**
     * Adds the given phone number to the autocomplete set, if it's valid.
     *
     * @param phoneNumber The phone number to possibly add.
     */
    public void addPhoneNumberIfValid(@Nullable String phoneNumber) {
        if (isPhoneValid(phoneNumber)) mPhoneNumbers.add(phoneNumber);
    }

    /**
     * Adds the given email address to the autocomplete set, if it's valid.
     *
     * @param emailAddress The email address to possibly add.
     */
    public void addEmailAddressIfValid(@Nullable String emailAddress) {
        if (isEmailValid(emailAddress)) mEmailAddresses.add(emailAddress);
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
     * Allows calling |showEditPrompt| with a single callback used for both 'done' and 'cancel'.
     *
     * @see #showEditPrompt(AutofillContact, Callback, Callback)
     */
    public void showEditPrompt(
            final @Nullable AutofillContact toEdit,
            final Callback<@Nullable AutofillContact> callback) {
        showEditPrompt(toEdit, (Callback<AutofillContact>) callback, callback);
    }

    @Override
    public void showEditPrompt(
            final @Nullable AutofillContact toEdit,
            final Callback<AutofillContact> doneCallback,
            final Callback<@Nullable AutofillContact> cancelCallback) {
        mDoneCallback = doneCallback;
        mCancelCallback = cancelCallback;

        mContactNew = (toEdit == null);
        var context = mContext;
        AutofillContact contact =
                toEdit == null
                        ? new AutofillContact(
                                context,
                                AutofillProfile.builder().build(),
                                null,
                                null,
                                null,
                                INVALID_NAME | INVALID_PHONE_NUMBER | INVALID_EMAIL,
                                mRequestPayerName,
                                mRequestPayerPhone,
                                mRequestPayerEmail)
                        : toEdit;
        mContact = contact;

        final String nameCustomErrorMessage = mPayerErrors != null ? mPayerErrors.name : null;
        PropertyModel nameField = null;
        if (mRequestPayerName) {
            String label = context.getString(R.string.payments_name_field_in_contact_details);
            String errorMessage =
                    context.getString(R.string.pref_edit_dialog_field_required_validation_message);
            nameField =
                    new PropertyModel.Builder(TEXT_ALL_KEYS)
                            .with(TEXT_FIELD_TYPE, FieldType.NAME_FULL)
                            .with(LABEL, label)
                            .with(TEXT_SUGGESTIONS, new ArrayList<>(mPayerNames))
                            .with(IS_REQUIRED, true)
                            .with(
                                    VALIDATOR,
                                    EditorFieldValidator.builder()
                                            .withRequiredErrorMessage(errorMessage)
                                            .withInitialErrorMessage(nameCustomErrorMessage)
                                            .build())
                            .with(VALUE, contact.getPayerName())
                            .build();
        }
        mNameField = nameField;

        PropertyModel phoneField = null;
        if (mRequestPayerPhone) {
            String label = context.getString(R.string.autofill_profile_editor_phone_number);
            phoneField =
                    new PropertyModel.Builder(TEXT_ALL_KEYS)
                            .with(TEXT_FIELD_TYPE, FieldType.PHONE_HOME_WHOLE_NUMBER)
                            .with(LABEL, label)
                            .with(TEXT_SUGGESTIONS, new ArrayList<>(mPhoneNumbers))
                            .with(
                                    TEXT_FORMATTER,
                                    new PhoneNumberUtil.CountryAwareFormatTextWatcher())
                            .with(IS_REQUIRED, true)
                            .with(VALIDATOR, getPhoneValidator())
                            .with(VALUE, contact.getPayerPhone())
                            .build();
        }
        mPhoneField = phoneField;

        PropertyModel emailField = null;
        if (mRequestPayerEmail) {
            String label = context.getString(R.string.autofill_profile_editor_email_address);
            emailField =
                    new PropertyModel.Builder(TEXT_ALL_KEYS)
                            .with(TEXT_FIELD_TYPE, FieldType.EMAIL_ADDRESS)
                            .with(LABEL, label)
                            .with(TEXT_SUGGESTIONS, new ArrayList<>(mEmailAddresses))
                            .with(IS_REQUIRED, true)
                            .with(VALIDATOR, getEmailValidator())
                            .with(VALUE, contact.getPayerEmail())
                            .build();
        }
        mEmailField = emailField;

        final String editorTitle =
                toEdit == null
                        ? context.getString(R.string.payments_add_contact_details_label)
                        : toEdit.getEditTitle();

        ListModel<EditorItem> editorFields = new ListModel<>();
        if (mNameField != null) {
            editorFields.add(new EditorItem(TEXT_INPUT, mNameField, /* isFullLine= */ true));
        }
        if (mPhoneField != null) {
            editorFields.add(new EditorItem(TEXT_INPUT, mPhoneField, /* isFullLine= */ true));
        }
        if (mEmailField != null) {
            editorFields.add(new EditorItem(TEXT_INPUT, mEmailField, /* isFullLine= */ true));
        }
        editorFields.add(
                new EditorItem(
                        NOTICE,
                        new PropertyModel.Builder(NOTICE_ALL_KEYS)
                                .with(
                                        NOTICE_TEXT,
                                        mContext.getString(
                                                R.string.payments_required_field_message))
                                // Required fields are indicated by an asterisk (*) and announced
                                // separately by screen readers. Don't announce the message itself.
                                .with(IMPORTANT_FOR_ACCESSIBILITY, false)
                                .build(),
                        /* isFullLine= */ true));

        mEditorModel =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(EDITOR_TITLE, editorTitle)
                        .with(EDITOR_FIELDS, editorFields)
                        .with(DONE_RUNNABLE, this::onDone)
                        .with(CANCEL_RUNNABLE, this::onCancel)
                        .with(ALLOW_DELETE, false)
                        // Form validation must be performed only for non-empty address profiles.
                        .with(VALIDATE_ON_SHOW, !mContactNew)
                        .build();

        mEditorMCP =
                PropertyModelChangeProcessor.create(
                        mEditorModel, mEditorDialog, EditorDialogViewBinder::bindEditorDialogView);
        mEditorModel.set(VISIBLE, true);

        mIsShown = true;
    }

    @EnsuresNonNullIf({
        "mDoneCallback",
        "mCancelCallback",
        "mContact",
        "mNameField",
        "mPhoneField",
        "mEmailField",
        "mEditorModel",
        "mEditorMCP"
    })
    @SuppressWarnings("NullAway")
    private boolean isShown() {
        return mIsShown;
    }

    private void onDone() {
        assert isShown();

        if (!validateForm(mEditorModel)) {
            scrollToFieldWithErrorMessage(mEditorModel);
            return;
        }
        mEditorModel.set(VISIBLE, false);

        String name = null;
        String phone = null;
        String email = null;
        AutofillProfile profile = mContact.getProfile();

        if (mNameField != null) {
            name = mNameField.get(VALUE);
            profile.setFullName(name);
        }

        if (mPhoneField != null) {
            phone = mPhoneField.get(VALUE);
            profile.setPhoneNumber(phone);
        }

        if (mEmailField != null) {
            email = mEmailField.get(VALUE);
            profile.setEmailAddress(email);
        }

        if (mSaveToDisk) {
            profile.setGUID(mPersonalDataManager.setProfileToLocal(profile));
        }

        if (profile.getGUID().isEmpty()) {
            assert !mSaveToDisk;

            // Set a fake guid for a new temp AutofillProfile.
            profile.setGUID(UUID.randomUUID().toString());
        }

        mContact.completeContact(profile.getGUID(), name, phone, email);
        mDoneCallback.onResult(mContact);

        // Clean up the state of this editor.
        reset();
    }

    private void onCancel() {
        assert isShown();

        mEditorModel.set(VISIBLE, false);

        mCancelCallback.onResult(mContactNew ? null : mContact);

        // Clean up the state of this editor.
        reset();
    }

    private static boolean isEmailValid(@Nullable String email) {
        return email != null && Patterns.EMAIL_ADDRESS.matcher(email).matches();
    }

    private static boolean isPhoneValid(@Nullable String phone) {
        // TODO(crbug.com/41479087): PhoneNumberUtils internally trigger
        // disk reads for certain devices/configurations.
        return phone != null
                && PhoneNumberUtils.isGlobalPhoneNumber(PhoneNumberUtils.stripSeparators(phone));
    }

    private EditorFieldValidator getEmailValidator() {
        var context = mContext;
        return EditorFieldValidator.builder()
                .withRequiredErrorMessage(
                        context.getString(
                                R.string.pref_edit_dialog_field_required_validation_message))
                .withInitialErrorMessage(mPayerErrors != null ? mPayerErrors.email : null)
                .withValidationPredicate(
                        ContactEditor::isEmailValid,
                        context.getString(R.string.payments_email_invalid_validation_message))
                .build();
    }

    private EditorFieldValidator getPhoneValidator() {
        var context = mContext;
        return EditorFieldValidator.builder()
                .withRequiredErrorMessage(
                        context.getString(
                                R.string.pref_edit_dialog_field_required_validation_message))
                .withInitialErrorMessage(mPayerErrors != null ? mPayerErrors.phone : null)
                .withValidationPredicate(
                        ContactEditor::isPhoneValid,
                        context.getString(R.string.payments_phone_invalid_validation_message))
                .build();
    }
}

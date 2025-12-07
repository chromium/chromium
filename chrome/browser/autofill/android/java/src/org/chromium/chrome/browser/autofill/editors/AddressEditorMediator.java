// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ALLOW_DELETE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CUSTOM_DONE_BUTTON_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_CONFIRMATION_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_CONFIRMATION_TITLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DONE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_CALLBACK;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_FIELDS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_TITLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALIDATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.DROPDOWN;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.NON_EDITABLE_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.NOTICE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.TEXT_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.NonEditableTextProperties.CLICK_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.NonEditableTextProperties.CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.NonEditableTextProperties.ICON;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.NonEditableTextProperties.NON_EDITABLE_TEXT_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.NonEditableTextProperties.PRIMARY_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.NoticeProperties.IMPORTANT_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.NoticeProperties.NOTICE_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.NoticeProperties.NOTICE_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.SHOW_BUTTONS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_FIELD_TYPE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_FORMATTER;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.VALIDATE_ON_SHOW;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.VISIBLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.scrollToFieldWithErrorMessage;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.validateForm;

import android.content.Context;
import android.text.TextUtils;
import android.text.style.ClickableSpan;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.AddressValidationType;
import org.chromium.chrome.browser.autofill.AutofillAddress;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PhoneNumberUtil;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.SaveUpdateAddressProfilePromptMode;
import org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.Delegate;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.EditorItem;
import org.chromium.components.autofill.AutofillAddressEditorUiInfo;
import org.chromium.components.autofill.AutofillAddressUiComponent;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.FieldType;
import org.chromium.components.autofill.RecordType;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.SpanApplier;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.UUID;
import java.util.function.Predicate;

/**
 * Contains the logic for the autofill address editor component. It sets the state of the model and
 * reacts to events like address country selection.
 */
@NullMarked
class AddressEditorMediator {
    private final PhoneNumberUtil.CountryAwareFormatTextWatcher mPhoneFormatter =
            new PhoneNumberUtil.CountryAwareFormatTextWatcher();
    private final AutofillProfileBridge mAutofillProfileBridge = new AutofillProfileBridge();
    private final Context mContext;
    private final Delegate mDelegate;
    private final IdentityManager mIdentityManager;
    private final @Nullable SyncService mSyncService;
    private final PersonalDataManager mPersonalDataManager;
    private final AutofillProfile mProfileToEdit;
    private final AutofillAddress mAddressToEdit;
    private final @SaveUpdateAddressProfilePromptMode int mPromptMode;
    private final boolean mSaveToDisk;
    private final Map<Integer, PropertyModel> mAddressFields = new HashMap<>();
    private final PropertyModel mCountryField;
    private final PropertyModel mPhoneField;
    private final PropertyModel mEmailField;

    private @Nullable AutofillAddressEditorUiInfo mEditorUiInfo;
    private @Nullable String mCustomDoneButtonText;
    private boolean mAllowDelete;

    private @Nullable PropertyModel mEditorModel;

    private PropertyModel getFieldForFieldType(@FieldType int fieldType) {
        if (!mAddressFields.containsKey(fieldType)) {
            // Address fields are cached.
            mAddressFields.put(
                    fieldType,
                    new PropertyModel.Builder(TEXT_ALL_KEYS)
                            .with(TEXT_FIELD_TYPE, fieldType)
                            .with(VALUE, mProfileToEdit.getInfo(fieldType))
                            .build());
        }
        return mAddressFields.get(fieldType);
    }

    AddressEditorMediator(
            Context context,
            Delegate delegate,
            IdentityManager identityManager,
            @Nullable SyncService syncService,
            PersonalDataManager personalDataManager,
            AutofillAddress addressToEdit,
            @SaveUpdateAddressProfilePromptMode int promptMode,
            boolean saveToDisk) {
        mContext = context;
        mDelegate = delegate;
        mIdentityManager = identityManager;
        mSyncService = syncService;
        mPersonalDataManager = personalDataManager;
        mProfileToEdit = addressToEdit.getProfile();
        mAddressToEdit = addressToEdit;
        mPromptMode = promptMode;
        mSaveToDisk = saveToDisk;

        // The country dropdown is always present on the editor.
        mCountryField =
                new PropertyModel.Builder(DROPDOWN_ALL_KEYS)
                        .with(LABEL, mContext.getString(R.string.autofill_profile_editor_country))
                        .with(
                                DROPDOWN_KEY_VALUE_LIST,
                                AutofillProfileBridge.getSupportedCountries())
                        .with(IS_REQUIRED, false)
                        .with(
                                VALUE,
                                AutofillAddress.getCountryCode(
                                        mProfileToEdit, mPersonalDataManager))
                        .build();

        // Phone number is present for all countries.
        mPhoneField =
                new PropertyModel.Builder(TEXT_ALL_KEYS)
                        .with(TEXT_FIELD_TYPE, FieldType.PHONE_HOME_WHOLE_NUMBER)
                        .with(
                                LABEL,
                                mContext.getString(R.string.autofill_profile_editor_phone_number))
                        .with(TEXT_FORMATTER, mPhoneFormatter)
                        .with(VALUE, mProfileToEdit.getInfo(FieldType.PHONE_HOME_WHOLE_NUMBER))
                        .build();

        // Phone number is present for all countries.
        mEmailField =
                new PropertyModel.Builder(TEXT_ALL_KEYS)
                        .with(TEXT_FIELD_TYPE, FieldType.EMAIL_ADDRESS)
                        .with(
                                LABEL,
                                mContext.getString(R.string.autofill_profile_editor_email_address))
                        .with(VALIDATOR, getEmailValidator())
                        .with(VALUE, mProfileToEdit.getInfo(FieldType.EMAIL_ADDRESS))
                        .build();

        assert mCountryField.get(VALUE) != null;
        mPhoneFormatter.setCountryCode(mCountryField.get(VALUE));
    }

    public void setAllowDelete(boolean allowDelete) {
        mAllowDelete = allowDelete;
    }

    void setCustomDoneButtonText(@Nullable String customDoneButtonText) {
        mCustomDoneButtonText = customDoneButtonText;
    }

    /**
     * Builds an editor model with the following fields.
     *
     * [ country dropdown    ] <----- country dropdown is always present.
     * [ an address field    ] \
     * [ an address field    ]  \
     *         ...                <-- field order, presence, required, and labels depend on country.
     * [ an address field    ]  /
     * [ an address field    ] /
     * [ phone number field  ] <----- phone is always present.
     * [ email address field ] <----- only present if purpose is Purpose.AUTOFILL_SETTINGS.
     */
    PropertyModel getEditorModel() {
        if (mEditorModel != null) {
            return mEditorModel;
        }

        mEditorModel =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(EDITOR_TITLE, getEditorTitle())
                        .with(CUSTOM_DONE_BUTTON_TEXT, mCustomDoneButtonText)
                        .with(DELETE_CONFIRMATION_TITLE, getDeleteConfirmationTitle())
                        .with(DELETE_CONFIRMATION_TEXT, getDeleteConfirmationText())
                        .with(
                                DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT,
                                getDeleteConfirmationPrimaryButtonText())
                        .with(EDITOR_FIELDS, setEditorFields())
                        .with(DONE_RUNNABLE, this::onCommitChanges)
                        // If the user clicks [Cancel], send `toEdit` address back to the caller,
                        // which was the original state (could be null, a complete address, a
                        // partial address).
                        .with(CANCEL_RUNNABLE, this::onCancelEditing)
                        .with(ALLOW_DELETE, mAllowDelete)
                        .with(DELETE_RUNNABLE, () -> mDelegate.onDelete(mAddressToEdit))
                        .with(
                                VALIDATE_ON_SHOW,
                                mPromptMode
                                        != SaveUpdateAddressProfilePromptMode.CREATE_NEW_PROFILE)
                        .with(SHOW_BUTTONS, !isNonEditableProfile())
                        .build();

        mCountryField.set(
                DROPDOWN_CALLBACK,
                new Callback<>() {
                    /** Update the list of fields according to the selected country. */
                    @Override
                    public void onResult(String countryCode) {
                        assumeNonNull(mEditorModel)
                                .set(
                                        EDITOR_FIELDS,
                                        buildEditorFieldList(
                                                countryCode, Locale.getDefault().getLanguage()));

                        mPhoneFormatter.setCountryCode(countryCode);
                    }
                });

        return mEditorModel;
    }

    private ListModel<EditorItem> setEditorFields() {
        if (isNonEditableProfile()) {
            return buildNonEditableItemsList();
        }
        return buildEditorFieldList(
                AutofillAddress.getCountryCode(mProfileToEdit, mPersonalDataManager),
                mProfileToEdit.getLanguageCode());
    }

    private boolean isNonEditableProfile() {
        return mProfileToEdit.isHomeOrWorkProfile()
                || mProfileToEdit.getRecordType() == RecordType.ACCOUNT_NAME_EMAIL;
    }

    private boolean shouldDisplayRequiredErrorIfFieldEmpty(AutofillAddressUiComponent component) {
        if (!isAccountAddressProfile()) {
            return false; // Required fields shouldn't be enforced for non-account address profiles.
        }

        if (!component.isRequired) return false;

        boolean isContentEmpty = TextUtils.isEmpty(mProfileToEdit.getInfo(component.id));
        // Already empty fields in existing address profiles are made optional even if they
        // are required by account storage rules. This allows users to save address profiles
        // as is without making them more complete during the process.
        return mPromptMode == SaveUpdateAddressProfilePromptMode.CREATE_NEW_PROFILE
                || !isContentEmpty;
    }

    /**
     * Creates a list of editor based on the country and language code of the profile that's being
     * edited.
     *
     * <p>For example, "US" will not add dependent locality to the list. A "JP" address will start
     * with a person's full name or with a prefecture name, depending on whether the language code
     * is "ja-Latn" or "ja".
     *
     * @param countryCode The country for which fields are to be added.
     * @param languageCode The language in which localized strings (e.g. label) are presented.
     */
    private ListModel<EditorItem> buildEditorFieldList(String countryCode, String languageCode) {
        ListModel<EditorItem> editorFields = new ListModel<>();
        mEditorUiInfo =
                mAutofillProfileBridge.getAddressEditorUiInfo(
                        countryCode, languageCode, AddressValidationType.ACCOUNT);

        // In terms of order, country must be the first field.
        editorFields.add(new EditorItem(DROPDOWN, mCountryField, /* isFullLine= */ true));

        for (AutofillAddressUiComponent component : mEditorUiInfo.getComponents()) {
            PropertyModel field = getFieldForFieldType(component.id);

            // Labels depend on country, e.g., state is called province in some countries. These are
            // already localized.
            field.set(LABEL, component.label);

            if (shouldDisplayRequiredErrorIfFieldEmpty(component)) {
                String message =
                        mContext.getString(R.string.autofill_edit_address_required_field_error)
                                .replace("$1", component.label);
                // Note: the error message itself will be displayed only if the field is or
                // becomes empty, this just marks "candidate" fields that should be taken
                // into account for the error.
                field.set(IS_REQUIRED, true);
                field.set(
                        VALIDATOR,
                        EditorFieldValidator.builder().withRequiredErrorMessage(message).build());
            } else {
                field.set(IS_REQUIRED, false);
            }

            final boolean isFullLine =
                    component.isFullLine
                            || component.id == FieldType.ADDRESS_HOME_CITY
                            || component.id == FieldType.ADDRESS_HOME_DEPENDENT_LOCALITY;
            editorFields.add(new EditorItem(TEXT_INPUT, field, isFullLine));
        }
        // Phone number (and email if applicable) are the last fields of the address.
        if (mPhoneField != null) {
            mPhoneField.set(VALIDATOR, getPhoneValidator(countryCode));
            editorFields.add(new EditorItem(TEXT_INPUT, mPhoneField, /* isFullLine= */ true));
        }
        if (mEmailField != null) {
            editorFields.add(new EditorItem(TEXT_INPUT, mEmailField, /* isFullLine= */ true));
        }
        for (EditorItem item : editorFields) {
            if (item.model.get(IS_REQUIRED)) {
                editorFields.add(
                        new EditorItem(
                                NOTICE,
                                new PropertyModel.Builder(NOTICE_ALL_KEYS)
                                        .with(
                                                NOTICE_TEXT,
                                                mContext.getString(
                                                        R.string.payments_required_field_message))
                                        // Required fields are indicated by an asterisk (*) and
                                        // announced separately by screen readers. Don't announce
                                        // the message itself.
                                        .with(IMPORTANT_FOR_ACCESSIBILITY, false)
                                        .build(),
                                /* isFullLine= */ true));
                break;
            }
        }
        maybeAddRecordTypeNotice(editorFields);

        return editorFields;
    }

    /**
     * Build a special list of items to display for non-editable profiles, e.g. home and work, gaia
     * name and email.
     */
    private ListModel<EditorItem> buildNonEditableItemsList() {
        ListModel<EditorItem> editorFields = new ListModel<>();

        addProfileDescriptionItem(editorFields);
        maybeAddRecordTypeNotice(editorFields);

        PropertyModel model =
                new PropertyModel.Builder(NON_EDITABLE_TEXT_ALL_KEYS)
                        .with(
                                PRIMARY_TEXT,
                                mContext.getString(R.string.autofill_edit_address_label))
                        .with(ICON, R.drawable.autofill_external_link)
                        .with(CLICK_RUNNABLE, () -> mDelegate.onExternalEdit(mProfileToEdit))
                        .with(
                                CONTENT_DESCRIPTION,
                                mContext.getString(
                                        R.string.autofill_edit_address_label_content_description))
                        .build();
        editorFields.add(new EditorItem(NON_EDITABLE_TEXT, model, /* isFullLine= */ true));

        return editorFields;
    }

    private void addProfileDescriptionItem(ListModel<EditorItem> editorFields) {
        PropertyModel.Builder descriptionModelBuilder =
                new PropertyModel.Builder(NON_EDITABLE_TEXT_ALL_KEYS);
        if (mProfileToEdit.getRecordType() == RecordType.ACCOUNT_NAME_EMAIL) {
            descriptionModelBuilder
                    .with(PRIMARY_TEXT, mProfileToEdit.getInfo(FieldType.NAME_FULL))
                    .with(
                            EditorProperties.NonEditableTextProperties.SECONDARY_TEXT,
                            mProfileToEdit.getInfo(FieldType.EMAIL_ADDRESS));
        } else {
            descriptionModelBuilder.with(
                    PRIMARY_TEXT,
                    mPersonalDataManager.getProfileDescriptionForEditor(mProfileToEdit.getGUID()));
        }
        editorFields.add(
                new EditorItem(
                        NON_EDITABLE_TEXT,
                        descriptionModelBuilder.build(),
                        /* isFullLine= */ true));
    }

    private void maybeAddRecordTypeNotice(ListModel<EditorItem> editorFields) {
        @Nullable String recordTypeNoticeText = getRecordTypeNoticeText();
        if (recordTypeNoticeText != null) {
            editorFields.add(
                    new EditorItem(
                            NOTICE,
                            new PropertyModel.Builder(NOTICE_ALL_KEYS)
                                    .with(NOTICE_TEXT, recordTypeNoticeText)
                                    .with(IMPORTANT_FOR_ACCESSIBILITY, true)
                                    .build(),
                            /* isFullLine= */ true));
        }
    }

    private void onCommitChanges() {
        assumeNonNull(mEditorModel);
        if (!validateForm(mEditorModel)) {
            scrollToFieldWithErrorMessage(mEditorModel);
            return;
        }
        mEditorModel.set(VISIBLE, false);

        commitChanges(mProfileToEdit);
        // The address cannot be marked "complete" because it has not been
        // checked for all required fields.
        mAddressToEdit.updateAddress(mProfileToEdit);

        mDelegate.onDone(mAddressToEdit);
    }

    private void onCancelEditing() {
        assumeNonNull(mEditorModel);
        mEditorModel.set(VISIBLE, false);

        mDelegate.onCancel();
    }

    /** Saves the edited profile on disk. */
    private void commitChanges(AutofillProfile profile) {
        String country = mCountryField.get(VALUE);
        if (willBeSavedInAccount()
                && mPromptMode == SaveUpdateAddressProfilePromptMode.CREATE_NEW_PROFILE) {
            profile.setRecordType(RecordType.ACCOUNT);
        }
        // Country code and phone number are always required and are always collected from the
        // editor model.
        profile.setInfo(FieldType.ADDRESS_HOME_COUNTRY, country);
        if (mPhoneField != null) {
            profile.setInfo(FieldType.PHONE_HOME_WHOLE_NUMBER, mPhoneField.get(VALUE));
        }
        if (mEmailField != null) {
            profile.setInfo(FieldType.EMAIL_ADDRESS, mEmailField.get(VALUE));
        }

        assumeNonNull(mEditorUiInfo);
        // Autofill profile bridge normalizes the language code for the autofill profile.
        profile.setLanguageCode(mEditorUiInfo.getBestLanguageTag());

        // Collect data from all visible fields and store it in the autofill profile.
        for (AutofillAddressUiComponent component : mEditorUiInfo.getComponents()) {
            if (component.id != FieldType.ADDRESS_HOME_COUNTRY) {
                assert mAddressFields.containsKey(component.id);
                profile.setInfo(component.id, mAddressFields.get(component.id).get(VALUE));
            }
        }

        // Save the edited autofill profile locally.
        if (mSaveToDisk) {
            profile.setGUID(mPersonalDataManager.setProfileToLocal(mProfileToEdit));
        }

        if (profile.getGUID().isEmpty()) {
            assert !mSaveToDisk;

            // Set a fake guid for a new temp AutofillProfile to be used in CardEditor. Note that
            // this temp AutofillProfile should not be saved to disk.
            profile.setGUID(UUID.randomUUID().toString());
        }
    }

    private boolean willBeSavedInAccount() {
        switch (mPromptMode) {
            case SaveUpdateAddressProfilePromptMode.MIGRATE_PROFILE:
                return true;
            case SaveUpdateAddressProfilePromptMode.UPDATE_PROFILE:
                return false;
            case SaveUpdateAddressProfilePromptMode.SAVE_NEW_PROFILE:
                return mProfileToEdit.getRecordType() == RecordType.ACCOUNT;
            case SaveUpdateAddressProfilePromptMode.CREATE_NEW_PROFILE:
                return mPersonalDataManager.isEligibleForAddressAccountStorage();
        }
        assert false : String.format(Locale.US, "Missing account target for flow %d", mPromptMode);
        return false;
    }

    private boolean isAccountAddressProfile() {
        return willBeSavedInAccount() || isAlreadySavedInAccount();
    }

    private String getEditorTitle() {
        return mPromptMode == SaveUpdateAddressProfilePromptMode.CREATE_NEW_PROFILE
                ? mContext.getString(R.string.autofill_create_profile)
                : mContext.getString(R.string.autofill_edit_address_dialog_title);
    }

    private @Nullable String getUserEmail() {
        CoreAccountInfo accountInfo = mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        return CoreAccountInfo.getEmailFrom(accountInfo);
    }

    private String getDeleteConfirmationTitle() {
        if (mProfileToEdit.getRecordType() == RecordType.ACCOUNT_HOME) {
            return mContext.getString(
                    R.string.autofill_remove_home_profile_suggestion_confirmation_title);
        }
        if (mProfileToEdit.getRecordType() == RecordType.ACCOUNT_WORK) {
            return mContext.getString(
                    R.string.autofill_remove_work_profile_suggestion_confirmation_title);
        }
        if (mProfileToEdit.getRecordType() == RecordType.ACCOUNT_NAME_EMAIL) {
            return mContext.getString(
                    R.string
                            .autofill_remove_account_name_and_email_profile_suggestion_confirmation_title);
        }
        return mContext.getString(R.string.autofill_delete_address_confirmation_dialog_title);
    }

    private CharSequence createMessageWithLink(String body) {
        ClickableSpan span =
                new ClickableSpan() {
                    @Override
                    public void onClick(View view) {
                        mDelegate.onExternalEdit(mProfileToEdit);
                    }
                };
        return SpanApplier.applySpans(body, new SpanApplier.SpanInfo("<link>", "</link>", span));
    }

    private CharSequence getDeleteConfirmationText() {
        if (isAccountAddressProfile()) {
            @Nullable String email = getUserEmail();
            if (email == null) return "";
            if (mProfileToEdit.getRecordType() == RecordType.ACCOUNT_HOME) {
                return createMessageWithLink(
                        mContext.getString(
                                        R.string
                                                .autofill_remove_home_profile_suggestion_confirmation_body)
                                .replace("$1", email));
            }
            if (mProfileToEdit.getRecordType() == RecordType.ACCOUNT_WORK) {
                return createMessageWithLink(
                        mContext.getString(
                                        R.string
                                                .autofill_remove_work_profile_suggestion_confirmation_body)
                                .replace("$1", email));
            }
            if (mProfileToEdit.getRecordType() == RecordType.ACCOUNT_NAME_EMAIL) {
                return createMessageWithLink(
                        mContext.getString(
                                        R.string
                                                .autofill_remove_account_name_and_email_profile_suggestion_confirmation_body)
                                .replace("$1", email));
            }
            return mContext.getString(R.string.autofill_delete_account_address_record_type_notice)
                    .replace("$1", email);
        }
        if (isAddressSyncOn()) {
            return mContext.getString(R.string.autofill_delete_sync_address_record_type_notice);
        }
        return mContext.getString(R.string.autofill_delete_local_address_record_type_notice);
    }

    private String getDeleteConfirmationPrimaryButtonText() {
        if (isNonEditableProfile()) {
            return mContext.getString(R.string.autofill_remove_suggestion_button);
        }
        return mContext.getString(R.string.autofill_delete_suggestion_button);
    }

    private @Nullable String getRecordTypeNoticeText() {
        if (!isAccountAddressProfile()) return null;
        @Nullable String email = getUserEmail();
        if (email == null) return null;

        if (isAlreadySavedInAccount()) {
            if (isNonEditableProfile()) {
                return mContext.getString(
                                R.string.autofill_address_home_and_work_record_type_notice)
                        .replace("$1", email);
            }
            return mContext.getString(
                            R.string.autofill_address_already_saved_in_account_record_type_notice)
                    .replace("$1", email);
        }

        return mContext.getString(
                        R.string.autofill_address_will_be_saved_in_account_record_type_notice)
                .replace("$1", email);
    }

    private boolean isAlreadySavedInAccount() {
        // User edits an account address profile either from Chrome settings or upon form
        // submission.
        return (mPromptMode == SaveUpdateAddressProfilePromptMode.UPDATE_PROFILE
                        && mProfileToEdit.getRecordType() == RecordType.ACCOUNT)
                || isNonEditableProfile();
    }

    private boolean isAddressSyncOn() {
        if (mSyncService == null) return false;
        return mSyncService.getSelectedTypes().contains(UserSelectableType.AUTOFILL);
    }

    private EditorFieldValidator getEmailValidator() {
        return EditorFieldValidator.builder()
                .withValidationPredicate(
                        unused -> true,
                        mContext.getString(R.string.payments_email_invalid_validation_message))
                .build();
    }

    private EditorFieldValidator getPhoneValidator(String countryCode) {
        // Note that isPossibleNumber is used since the metadata in libphonenumber has to be
        // updated frequently (daily) to do more strict validation.
        Predicate<String> validationPredicate =
                value ->
                        TextUtils.isEmpty(value)
                                || PhoneNumberUtil.isPossibleNumber(value, countryCode);

        return EditorFieldValidator.builder()
                .withValidationPredicate(
                        validationPredicate,
                        mContext.getString(R.string.payments_phone_invalid_validation_message))
                .build();
    }

    String getProfileRecordTypeSuffix() {
        return getProfileRecordTypeSuffixFromProfile(mProfileToEdit);
    }

    private static String getProfileRecordTypeSuffixFromProfile(AutofillProfile profile) {
        // LINT.IfChange(ProfileRecordTypeSuffix)
        switch (profile.getRecordType()) {
            case RecordType.LOCAL_OR_SYNCABLE:
                return "LocalOrSyncable";
            case RecordType.ACCOUNT:
                return "Account";
            case RecordType.ACCOUNT_HOME:
                return "AccountHome";
            case RecordType.ACCOUNT_WORK:
                return "AccountWork";
            case RecordType.ACCOUNT_NAME_EMAIL:
                return "AccountNameEmail";
            default:
                // Other types are not expected for addresses.
                return "Unknown";
        }
        // LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/histograms.xml:ProfileRecordTypeSuffix)
    }
}

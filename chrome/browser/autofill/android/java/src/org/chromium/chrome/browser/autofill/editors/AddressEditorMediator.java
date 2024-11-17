// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.UserFlow.CREATE_NEW_ADDRESS_PROFILE;
import static org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.UserFlow.MIGRATE_EXISTING_ADDRESS_PROFILE;
import static org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.UserFlow.SAVE_NEW_ADDRESS_PROFILE;
import static org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.UserFlow.UPDATE_EXISTING_ADDRESS_PROFILE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ALLOW_DELETE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CUSTOM_DONE_BUTTON_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_CONFIRMATION_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_CONFIRMATION_TITLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DONE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_CALLBACK;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_FIELDS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_TITLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FOOTER_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALIDATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.DROPDOWN;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.TEXT_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.SHOW_REQUIRED_INDICATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_FIELD_TYPE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_FORMATTER;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.VALIDATE_ON_SHOW;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.VISIBLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.scrollToFieldWithErrorMessage;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.validateForm;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.autofill.AddressValidationType;
import org.chromium.chrome.browser.autofill.AutofillAddress;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge.AutofillAddressUiComponent;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PhoneNumberUtil;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.Delegate;
import org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.UserFlow;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownKeyValue;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldItem;
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

import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.UUID;
import java.util.function.Predicate;

/**
 * Contains the logic for the autofill address editor component. It sets the state of the model and
 * reacts to events like address country selection.
 */
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
    private final @UserFlow int mUserFlow;
    private final boolean mSaveToDisk;
    private final Map<Integer, PropertyModel> mAddressFields = new HashMap<>();
    private final PropertyModel mCountryField;
    private final PropertyModel mPhoneField;
    private final PropertyModel mEmailField;

    private List<AutofillAddressUiComponent> mVisibleEditorFields;
    @Nullable private String mCustomDoneButtonText;
    private boolean mAllowDelete;

    @Nullable private PropertyModel mEditorModel;

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

    // TODO(crbug.com/40263955): remove temporary unsupported countries filtering.
    private static List<DropdownKeyValue> getSupportedCountries(
            PersonalDataManager personalDataManager, boolean filterOutUnsupportedCountries) {
        List<DropdownKeyValue> supportedCountries = AutofillProfileBridge.getSupportedCountries();
        if (filterOutUnsupportedCountries) {
            supportedCountries.removeIf(
                    entry ->
                            !personalDataManager.isCountryEligibleForAccountStorage(
                                    entry.getKey()));
        }

        return supportedCountries;
    }

    AddressEditorMediator(
            Context context,
            Delegate delegate,
            IdentityManager identityManager,
            @Nullable SyncService syncService,
            PersonalDataManager personalDataManager,
            AutofillAddress addressToEdit,
            @UserFlow int userFlow,
            boolean saveToDisk) {
        mContext = context;
        mDelegate = delegate;
        mIdentityManager = identityManager;
        mSyncService = syncService;
        mPersonalDataManager = personalDataManager;
        mProfileToEdit = addressToEdit.getProfile();
        mAddressToEdit = addressToEdit;
        mUserFlow = userFlow;
        mSaveToDisk = saveToDisk;

        // The country dropdown is always present on the editor.
        mCountryField =
                new PropertyModel.Builder(DROPDOWN_ALL_KEYS)
                        .with(LABEL, mContext.getString(R.string.autofill_profile_editor_country))
                        .with(
                                DROPDOWN_KEY_VALUE_LIST,
                                getSupportedCountries(
                                        mPersonalDataManager,
                                        isAccountAddressProfile()
                                                && mUserFlow != CREATE_NEW_ADDRESS_PROFILE))
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
     * [ address nickname    ] <----- only present if nickname support is enabled.
     */
    PropertyModel getEditorModel() {
        if (mEditorModel != null) {
            return mEditorModel;
        }

        mEditorModel =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(EDITOR_TITLE, getEditorTitle())
                        .with(CUSTOM_DONE_BUTTON_TEXT, mCustomDoneButtonText)
                        .with(FOOTER_MESSAGE, getRecordTypeNoticeText())
                        .with(DELETE_CONFIRMATION_TITLE, getDeleteConfirmationTitle())
                        .with(DELETE_CONFIRMATION_TEXT, getDeleteConfirmationText())
                        .with(SHOW_REQUIRED_INDICATOR, false)
                        .with(
                                EDITOR_FIELDS,
                                buildEditorFieldList(
                                        AutofillAddress.getCountryCode(
                                                mProfileToEdit, mPersonalDataManager),
                                        mProfileToEdit.getLanguageCode()))
                        .with(DONE_RUNNABLE, this::onCommitChanges)
                        // If the user clicks [Cancel], send |toEdit| address back to the caller,
                        // which was the original state (could be null, a complete address, a
                        // partial address).
                        .with(CANCEL_RUNNABLE, this::onCancelEditing)
                        .with(ALLOW_DELETE, mAllowDelete)
                        .with(DELETE_RUNNABLE, () -> mDelegate.onDelete(mAddressToEdit))
                        .with(VALIDATE_ON_SHOW, mUserFlow != CREATE_NEW_ADDRESS_PROFILE)
                        .build();

        mCountryField.set(
                DROPDOWN_CALLBACK,
                new Callback<String>() {
                    /** Update the list of fields according to the selected country. */
                    @Override
                    public void onResult(String countryCode) {
                        mEditorModel.set(
                                EDITOR_FIELDS,
                                buildEditorFieldList(
                                        countryCode, Locale.getDefault().getLanguage()));

                        mPhoneFormatter.setCountryCode(countryCode);
                    }
                });

        return mEditorModel;
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
        return mUserFlow == CREATE_NEW_ADDRESS_PROFILE || !isContentEmpty;
    }

    /**
     * Creates a list of editor based on the country and language code of the profile that's being
     * edited.
     *
     * For example, "US" will not add dependent locality to the list. A "JP" address will start
     * with a person's full name or with a prefecture name, depending on whether the language code
     * is "ja-Latn" or "ja".
     *
     * @param countryCode The country for which fields are to be added.
     * @param languageCode The language in which localized strings (e.g. label) are presented.
     */
    private ListModel<FieldItem> buildEditorFieldList(String countryCode, String languageCode) {
        ListModel<FieldItem> editorFields = new ListModel<>();
        mVisibleEditorFields =
                mAutofillProfileBridge.getAddressUiComponents(
                        countryCode, languageCode, AddressValidationType.ACCOUNT);

        // In terms of order, country must be the first field.
        editorFields.add(new FieldItem(DROPDOWN, mCountryField, /* isFullLine= */ true));

        for (AutofillAddressUiComponent component : mVisibleEditorFields) {
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
            editorFields.add(new FieldItem(TEXT_INPUT, field, isFullLine));
        }
        // Phone number (and email if applicable) are the last fields of the address.
        if (mPhoneField != null) {
            mPhoneField.set(VALIDATOR, getPhoneValidator(countryCode));
            editorFields.add(new FieldItem(TEXT_INPUT, mPhoneField, /* isFullLine= */ true));
        }
        if (mEmailField != null) {
            editorFields.add(new FieldItem(TEXT_INPUT, mEmailField, /* isFullLine= */ true));
        }

        return editorFields;
    }

    private void onCommitChanges() {
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
        mEditorModel.set(VISIBLE, false);

        mDelegate.onCancel();
    }

    /** Saves the edited profile on disk. */
    private void commitChanges(AutofillProfile profile) {
        String country = mCountryField.get(VALUE);
        if (willBeSavedInAccount()
                && mUserFlow == CREATE_NEW_ADDRESS_PROFILE
                && mPersonalDataManager.isCountryEligibleForAccountStorage(country)) {
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

        // Autofill profile bridge normalizes the language code for the autofill profile.
        profile.setLanguageCode(mAutofillProfileBridge.getCurrentBestLanguageCode());

        // Collect data from all visible fields and store it in the autofill profile.
        for (AutofillAddressUiComponent component : mVisibleEditorFields) {
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
        switch (mUserFlow) {
            case MIGRATE_EXISTING_ADDRESS_PROFILE:
                return true;
            case UPDATE_EXISTING_ADDRESS_PROFILE:
                return false;
            case SAVE_NEW_ADDRESS_PROFILE:
                return mProfileToEdit.getRecordType() == RecordType.ACCOUNT;
            case CREATE_NEW_ADDRESS_PROFILE:
                return mPersonalDataManager.isEligibleForAddressAccountStorage();
        }
        assert false : String.format(Locale.US, "Missing account target for flow %d", mUserFlow);
        return false;
    }

    private boolean isAccountAddressProfile() {
        return willBeSavedInAccount() || isAlreadySavedInAccount();
    }

    private String getEditorTitle() {
        return mUserFlow == CREATE_NEW_ADDRESS_PROFILE
                ? mContext.getString(R.string.autofill_create_profile)
                : mContext.getString(R.string.autofill_edit_address_dialog_title);
    }

    private @Nullable String getUserEmail() {
        CoreAccountInfo accountInfo = mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        return CoreAccountInfo.getEmailFrom(accountInfo);
    }

    private @Nullable String getDeleteConfirmationText() {
        if (isAccountAddressProfile()) {
            @Nullable String email = getUserEmail();
            if (email == null) return null;
            return mContext.getString(R.string.autofill_delete_account_address_record_type_notice)
                    .replace("$1", email);
        }
        if (isAddressSyncOn()) {
            return mContext.getString(R.string.autofill_delete_sync_address_record_type_notice);
        }
        return mContext.getString(R.string.autofill_delete_local_address_record_type_notice);
    }

    private @Nullable String getRecordTypeNoticeText() {
        if (!isAccountAddressProfile()) return null;
        @Nullable String email = getUserEmail();
        if (email == null) return null;

        if (isAlreadySavedInAccount()) {
            return mContext.getString(
                            R.string.autofill_address_already_saved_in_account_record_type_notice)
                    .replace("$1", email);
        }

        return mContext.getString(
                        R.string.autofill_address_will_be_saved_in_account_record_type_notice)
                .replace("$1", email);
    }

    private String getDeleteConfirmationTitle() {
        return mContext.getString(R.string.autofill_delete_address_confirmation_dialog_title);
    }

    private boolean isAlreadySavedInAccount() {
        // User edits an account address profile either from Chrome settings or upon form
        // submission.
        return mUserFlow == UPDATE_EXISTING_ADDRESS_PROFILE
                && mProfileToEdit.getRecordType() == RecordType.ACCOUNT;
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
}

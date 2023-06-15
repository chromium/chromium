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
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.INVALID_ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_FULL_LINE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.REQUIRED_ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALIDATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.DROPDOWN;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.TEXT_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.SHOW_REQUIRED_INDICATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TRIGGER_DONE_CALLBACK_BEFORE_CLOSE_ANIMATION;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.LENGTH_COUNTER_LIMIT_NONE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_FORMATTER;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_INPUT_TYPE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_LENGTH_COUNTER_LIMIT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.ALPHA_NUMERIC_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.EMAIL_ADDRESS_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.PERSON_NAME_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.PHONE_NUMBER_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.PLAIN_TEXT_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.STREET_ADDRESS_INPUT;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.autofill.AddressValidationType;
import org.chromium.chrome.browser.autofill.AutofillAddress;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge.AddressField;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge.AddressUiComponent;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PhoneNumberUtil;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.Source;
import org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.Delegate;
import org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.UserFlow;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownKeyValue;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.EditorFieldValidator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.UUID;

/**
 * Contains the logic for the autofill address editor component. It sets the state of the model and
 * reacts to events like address country selection.
 */
class AddressEditorMediator {
    private final PhoneNumberUtil.CountryAwareFormatTextWatcher mPhoneFormatter =
            new PhoneNumberUtil.CountryAwareFormatTextWatcher();
    private final CountryAwarePhoneNumberValidator mPhoneValidator =
            new CountryAwarePhoneNumberValidator(true);
    private final AutofillProfileBridge mAutofillProfileBridge = new AutofillProfileBridge();
    private final Context mContext;
    private final Delegate mDelegate;
    private final IdentityManager mIdentityManager;
    private final @Nullable SyncService mSyncService;
    private final AutofillProfile mProfileToEdit;
    private final AutofillAddress mAddressToEdit;
    private final @UserFlow int mUserFlow;
    private final boolean mSaveToDisk;
    private final Map<Integer, PropertyModel> mAddressFields = new HashMap<>();
    private final PropertyModel mCountryField;
    private final @Nullable PropertyModel mHonorificField;
    private final PropertyModel mPhoneField;
    private final PropertyModel mEmailField;
    private final @Nullable PropertyModel mNicknameField;

    private List<AddressUiComponent> mVisibleEditorFields;
    @Nullable
    private String mCustomDoneButtonText;
    private boolean mAllowDelete;
    private boolean mShouldTriggerDoneCallbackBeforeCloseAnimation;

    /**
     * The list of possible address fields for editing is determined statically.
     *
     * @return the list of address fields this address editor supports.
     */
    private static Map<Integer, PropertyModel> getAddressFields() {
        Map<Integer, PropertyModel> addressFields = new HashMap<>();

        // Don't use REGION_INPUT to avoid capitalizing all characters.
        addressFields.put(AddressField.ADMIN_AREA,
                new PropertyModel.Builder(TEXT_ALL_KEYS)
                        .with(TEXT_INPUT_TYPE, PLAIN_TEXT_INPUT)
                        .build());

        // City, dependent locality, and organization don't have any special formatting hints.
        addressFields.put(AddressField.LOCALITY,
                new PropertyModel.Builder(TEXT_ALL_KEYS)
                        .with(TEXT_INPUT_TYPE, PLAIN_TEXT_INPUT)
                        .build());
        addressFields.put(AddressField.DEPENDENT_LOCALITY,
                new PropertyModel.Builder(TEXT_ALL_KEYS)
                        .with(TEXT_INPUT_TYPE, PLAIN_TEXT_INPUT)
                        .build());
        addressFields.put(AddressField.ORGANIZATION,
                new PropertyModel.Builder(TEXT_ALL_KEYS)
                        .with(TEXT_INPUT_TYPE, PLAIN_TEXT_INPUT)
                        .build());

        // Sorting code and postal code (a.k.a. ZIP code) should show both letters and digits on
        // the keyboard, if possible.
        addressFields.put(AddressField.SORTING_CODE,
                new PropertyModel.Builder(TEXT_ALL_KEYS)
                        .with(TEXT_INPUT_TYPE, ALPHA_NUMERIC_INPUT)
                        .build());
        addressFields.put(AddressField.POSTAL_CODE,
                new PropertyModel.Builder(TEXT_ALL_KEYS)
                        .with(TEXT_INPUT_TYPE, ALPHA_NUMERIC_INPUT)
                        .build());

        // Street line field can contain \n to indicate line breaks.
        addressFields.put(AddressField.STREET_ADDRESS,
                new PropertyModel.Builder(TEXT_ALL_KEYS)
                        .with(TEXT_INPUT_TYPE, STREET_ADDRESS_INPUT)
                        .build());

        // Android has special formatting rules for names.
        addressFields.put(AddressField.RECIPIENT,
                new PropertyModel.Builder(TEXT_ALL_KEYS)
                        .with(TEXT_INPUT_TYPE, PERSON_NAME_INPUT)
                        .build());

        return addressFields;
    }

    // TODO(crbug.com/1432505): remove temporary unsupported countries filtering.
    private static List<DropdownKeyValue> getSupportedCountries(
            boolean filterOutUnsupportedCountries) {
        List<DropdownKeyValue> supportedCountries = AutofillProfileBridge.getSupportedCountries();
        if (filterOutUnsupportedCountries) {
            PersonalDataManager personalDataManager = PersonalDataManager.getInstance();
            supportedCountries.removeIf(entry
                    -> !personalDataManager.isCountryEligibleForAccountStorage(entry.getKey()));
        }

        return supportedCountries;
    }

    AddressEditorMediator(Context context, Delegate delegate, IdentityManager identityManager,
            @Nullable SyncService syncService, AutofillAddress addressToEdit,
            @UserFlow int userFlow, boolean saveToDisk) {
        mContext = context;
        mDelegate = delegate;
        mIdentityManager = identityManager;
        mSyncService = syncService;
        mProfileToEdit = addressToEdit.getProfile();
        mAddressToEdit = addressToEdit;
        mUserFlow = userFlow;
        mSaveToDisk = saveToDisk;

        // The country dropdown is always present on the editor.
        mCountryField =
                new PropertyModel.Builder(DROPDOWN_ALL_KEYS)
                        .with(LABEL, mContext.getString(R.string.autofill_profile_editor_country))
                        .with(DROPDOWN_KEY_VALUE_LIST,
                                getSupportedCountries(isAccountAddressProfile()
                                        && mUserFlow != CREATE_NEW_ADDRESS_PROFILE))
                        .with(IS_FULL_LINE, true)
                        .build();

        // Honorific prefix is present only for autofill settings.
        mHonorificField = ChromeFeatureList.isEnabled(
                                  ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HONORIFIC_PREFIXES)
                ? new PropertyModel.Builder(TEXT_ALL_KEYS)
                          .with(TEXT_INPUT_TYPE, PLAIN_TEXT_INPUT)
                          .with(LABEL,
                                  mContext.getString(
                                          R.string.autofill_profile_editor_honorific_prefix))
                          .with(IS_FULL_LINE, true)
                          .build()
                : null;

        // There's a finite number of fields for address editing. Changing the country will re-order
        // and relabel the fields. The meaning of each field remains the same.
        mAddressFields.putAll(getAddressFields());

        // Phone number is present for all countries.
        mPhoneField =
                new PropertyModel.Builder(TEXT_ALL_KEYS)
                        .with(TEXT_INPUT_TYPE, PHONE_NUMBER_INPUT)
                        .with(LABEL,
                                mContext.getString(R.string.autofill_profile_editor_phone_number))
                        .with(TEXT_FORMATTER, mPhoneFormatter)
                        .with(VALIDATOR, mPhoneValidator)
                        .with(IS_REQUIRED, false)
                        .with(INVALID_ERROR_MESSAGE,
                                mContext.getString(
                                        R.string.payments_phone_invalid_validation_message))
                        .with(IS_FULL_LINE, true)
                        .with(TEXT_LENGTH_COUNTER_LIMIT, LENGTH_COUNTER_LIMIT_NONE)
                        .build();

        // Phone number is present for all countries.
        mEmailField =
                new PropertyModel.Builder(TEXT_ALL_KEYS)
                        .with(TEXT_INPUT_TYPE, EMAIL_ADDRESS_INPUT)
                        .with(LABEL,
                                mContext.getString(R.string.autofill_profile_editor_email_address))
                        .with(IS_REQUIRED, false)
                        .with(INVALID_ERROR_MESSAGE,
                                mContext.getString(
                                        R.string.payments_email_invalid_validation_message))
                        .with(IS_FULL_LINE, true)
                        .with(TEXT_LENGTH_COUNTER_LIMIT, LENGTH_COUNTER_LIMIT_NONE)
                        .build();

        // TODO(crbug.com/1445020): Use localized string.
        mNicknameField =
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.AUTOFILL_ADDRESS_PROFILE_SAVE_PROMPT_NICKNAME_SUPPORT)
                ? new PropertyModel.Builder(TEXT_ALL_KEYS)
                          .with(TEXT_INPUT_TYPE, PLAIN_TEXT_INPUT)
                          .with(LABEL, "Label")
                          .with(IS_FULL_LINE, true)
                          .build()
                : null;

        // This should be called when all required fields are put in mAddressField.
        setAddressFieldValues();

        assert mCountryField.get(VALUE) != null;
        mPhoneValidator.setCountryCode(mCountryField.get(VALUE));
        mPhoneFormatter.setCountryCode(mCountryField.get(VALUE));
    }

    public void setAllowDelete(boolean allowDelete) {
        mAllowDelete = allowDelete;
    }

    private void setAddressFieldValues() {
        mCountryField.set(VALUE, AutofillAddress.getCountryCode(mProfileToEdit));
        if (mHonorificField != null) {
            mHonorificField.set(VALUE, mProfileToEdit.getHonorificPrefix());
        }
        // Address fields are cached, so their values need to be updated for every new profile
        // that's being edited.
        for (Map.Entry<Integer, PropertyModel> entry : mAddressFields.entrySet()) {
            entry.getValue().set(
                    VALUE, AutofillAddress.getProfileField(mProfileToEdit, entry.getKey()));
        }
        mPhoneField.set(VALUE, mProfileToEdit.getPhoneNumber());
        mEmailField.set(VALUE, mProfileToEdit.getEmailAddress());
    }

    public void setShouldTriggerDoneCallbackBeforeCloseAnimation(boolean shouldTrigger) {
        mShouldTriggerDoneCallbackBeforeCloseAnimation = shouldTrigger;
    }

    void setCustomDoneButtonText(@Nullable String customDoneButtonText) {
        mCustomDoneButtonText = customDoneButtonText;
    }

    /**
     * Builds an editor model with the following fields.
     *
     * [ country dropdown    ] <----- country dropdown is always present.
     * [ honorific field     ] <----- above name, present if purpose is Purpose.AUTOFILL_SETTINGS.
     * [ an address field    ] \
     * [ an address field    ]  \
     *         ...                <-- field order, presence, required, and labels depend on country.
     * [ an address field    ]  /
     * [ an address field    ] /
     * [ phone number field  ] <----- phone is always present.
     * [ email address field ] <----- only present if purpose is Purpose.AUTOFILL_SETTINGS.
     * [ address nickname    ] <----- only present if nickname support is enabled.
     */
    PropertyModel buildEditorModel() {
        PropertyModel editorModel =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(EDITOR_TITLE, getEditorTitle())
                        .with(CUSTOM_DONE_BUTTON_TEXT, mCustomDoneButtonText)
                        .with(FOOTER_MESSAGE, getSourceNoticeText())
                        .with(DELETE_CONFIRMATION_TITLE, getDeleteConfirmationTitle())
                        .with(DELETE_CONFIRMATION_TEXT, getDeleteConfirmationText())
                        .with(SHOW_REQUIRED_INDICATOR, false)
                        .with(TRIGGER_DONE_CALLBACK_BEFORE_CLOSE_ANIMATION,
                                mShouldTriggerDoneCallbackBeforeCloseAnimation)
                        .with(EDITOR_FIELDS,
                                buildEditorFieldList(AutofillAddress.getCountryCode(mProfileToEdit),
                                        mProfileToEdit.getLanguageCode()))
                        .with(DONE_RUNNABLE, this::onCommitChanges)
                        // If the user clicks [Cancel], send |toEdit| address back to the caller,
                        // which was the original state (could be null, a complete address, a
                        // partial address).
                        .with(CANCEL_RUNNABLE, mDelegate::onCancel)
                        .with(ALLOW_DELETE, mAllowDelete)
                        .with(DELETE_RUNNABLE, () -> mDelegate.onDelete(mAddressToEdit))
                        .build();

        mCountryField.set(DROPDOWN_CALLBACK, new Callback<String>() {
            /**
             * Update the list of fields according to the selected country.
             */
            @Override
            public void onResult(String countryCode) {
                editorModel.set(EDITOR_FIELDS,
                        buildEditorFieldList(countryCode, Locale.getDefault().getLanguage()));

                mPhoneFormatter.setCountryCode(countryCode);
                mPhoneValidator.setCountryCode(countryCode);
            }
        });

        return editorModel;
    }

    private boolean shouldDisplayRequiredErrorIfFieldEmpty(AddressUiComponent component) {
        if (!isAccountAddressProfile()) {
            return false; // Required fields shouldn't be enforced for non-account address profiles.
        }

        if (!component.isRequired) return false;

        boolean isContentEmpty =
                TextUtils.isEmpty(AutofillAddress.getProfileField(mProfileToEdit, component.id));
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
    private ListModel<ListItem> buildEditorFieldList(String countryCode, String languageCode) {
        ListModel<ListItem> editorFields = new ListModel<>();
        mVisibleEditorFields = mAutofillProfileBridge.getAddressUiComponents(
                countryCode, languageCode, AddressValidationType.ACCOUNT);

        // In terms of order, country must be the first field.
        editorFields.add(new ListItem(DROPDOWN, mCountryField));

        for (int i = 0; i < mVisibleEditorFields.size(); i++) {
            AddressUiComponent component = mVisibleEditorFields.get(i);

            // Honorific prefix should go before name.
            if (component.id == AddressField.RECIPIENT && mHonorificField != null) {
                editorFields.add(new ListItem(TEXT_INPUT, mHonorificField));
            }

            PropertyModel field = mAddressFields.get(component.id);

            // Labels depend on country, e.g., state is called province in some countries. These are
            // already localized.
            field.set(LABEL, component.label);
            field.set(IS_FULL_LINE,
                    component.isFullLine || component.id == AddressField.LOCALITY
                            || component.id == AddressField.DEPENDENT_LOCALITY);

            if (shouldDisplayRequiredErrorIfFieldEmpty(component)) {
                String message =
                        mContext.getString(R.string.autofill_edit_address_required_field_error)
                                .replace("$1", component.label);
                // Note: the error message itself will be displayed only if the field is or
                // becomes empty, this just marks "candidate" fields that should be taken
                // into account for the error.
                field.set(IS_REQUIRED, true);
                field.set(REQUIRED_ERROR_MESSAGE, message);
            }

            editorFields.add(new ListItem(TEXT_INPUT, field));
        }
        // Phone number (and email/nickname if applicable) are the last fields of the address.
        if (mPhoneField != null) editorFields.add(new ListItem(TEXT_INPUT, mPhoneField));
        if (mEmailField != null) editorFields.add(new ListItem(TEXT_INPUT, mEmailField));
        if (mNicknameField != null) editorFields.add(new ListItem(TEXT_INPUT, mNicknameField));

        return editorFields;
    }

    private void onCommitChanges() {
        // If the user clicks [Done], save changes on disk, mark the address
        // "complete" if possible,
        // and send it back to the caller.
        commitChanges(mProfileToEdit);

        // The address cannot be marked "complete" because it has not been
        // checked for all required fields.
        mAddressToEdit.updateAddress(mProfileToEdit);

        mDelegate.onDone(mAddressToEdit);
    }

    /** Saves the edited profile on disk. */
    private void commitChanges(AutofillProfile profile) {
        String country = mCountryField.get(VALUE);
        if (willBeSavedInAccount() && mUserFlow == CREATE_NEW_ADDRESS_PROFILE
                && PersonalDataManager.getInstance().isCountryEligibleForAccountStorage(country)) {
            profile.setSource(Source.ACCOUNT);
        }
        // Country code and phone number are always required and are always collected from the
        // editor model.
        profile.setCountryCode(country);
        if (mPhoneField != null) profile.setPhoneNumber(mPhoneField.get(VALUE));
        if (mEmailField != null) profile.setEmailAddress(mEmailField.get(VALUE));
        if (mHonorificField != null) {
            profile.setHonorificPrefix(mHonorificField.get(VALUE));
        }

        // Autofill profile bridge normalizes the language code for the autofill profile.
        profile.setLanguageCode(mAutofillProfileBridge.getCurrentBestLanguageCode());

        // Collect data from all visible fields and store it in the autofill profile.
        Set<Integer> visibleFields = new HashSet<>();
        for (int i = 0; i < mVisibleEditorFields.size(); i++) {
            AddressUiComponent component = mVisibleEditorFields.get(i);
            visibleFields.add(component.id);
            if (component.id != AddressField.COUNTRY) {
                setProfileField(profile, component.id, mAddressFields.get(component.id).get(VALUE));
            }
        }

        // Clear the fields that are hidden from the user interface, so
        // AutofillAddress.toPaymentAddress() will send them to the renderer as empty strings.
        for (Map.Entry<Integer, PropertyModel> entry : mAddressFields.entrySet()) {
            if (!visibleFields.contains(entry.getKey())) {
                setProfileField(profile, entry.getKey(), "");
            }
        }

        // Save the edited autofill profile locally.
        if (mSaveToDisk) {
            profile.setGUID(PersonalDataManager.getInstance().setProfileToLocal(mProfileToEdit));
        }

        if (profile.getGUID().isEmpty()) {
            assert !mSaveToDisk;

            // Set a fake guid for a new temp AutofillProfile to be used in CardEditor. Note that
            // this temp AutofillProfile should not be saved to disk.
            profile.setGUID(UUID.randomUUID().toString());
        }

        profile.setIsLocal(true);
    }

    /** Writes the given value into the specified autofill profile field. */
    private static void setProfileField(
            AutofillProfile profile, int field, @Nullable String value) {
        assert profile != null;
        switch (field) {
            case AddressField.COUNTRY:
                profile.setCountryCode(ensureNotNull(value));
                return;
            case AddressField.ADMIN_AREA:
                profile.setRegion(ensureNotNull(value));
                return;
            case AddressField.LOCALITY:
                profile.setLocality(ensureNotNull(value));
                return;
            case AddressField.DEPENDENT_LOCALITY:
                profile.setDependentLocality(ensureNotNull(value));
                return;
            case AddressField.SORTING_CODE:
                profile.setSortingCode(ensureNotNull(value));
                return;
            case AddressField.POSTAL_CODE:
                profile.setPostalCode(ensureNotNull(value));
                return;
            case AddressField.STREET_ADDRESS:
                profile.setStreetAddress(ensureNotNull(value));
                return;
            case AddressField.ORGANIZATION:
                profile.setCompanyName(ensureNotNull(value));
                return;
            case AddressField.RECIPIENT:
                profile.setFullName(ensureNotNull(value));
                return;
        }

        assert false;
    }

    private static String ensureNotNull(@Nullable String value) {
        return value == null ? "" : value;
    }

    private boolean willBeSavedInAccount() {
        switch (mUserFlow) {
            case MIGRATE_EXISTING_ADDRESS_PROFILE:
                return true;
            case UPDATE_EXISTING_ADDRESS_PROFILE:
                return false;
            case SAVE_NEW_ADDRESS_PROFILE:
                return mProfileToEdit.getSource() == Source.ACCOUNT;
            case CREATE_NEW_ADDRESS_PROFILE:
                return PersonalDataManager.getInstance().isEligibleForAddressAccountStorage();
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
            @Nullable
            String email = getUserEmail();
            if (email == null) return null;
            return mContext.getString(R.string.autofill_delete_account_address_source_notice)
                    .replace("$1", email);
        }
        if (isAddressSyncOn()) {
            return mContext.getString(R.string.autofill_delete_sync_address_source_notice);
        }
        return mContext.getString(R.string.autofill_delete_local_address_source_notice);
    }

    private @Nullable String getSourceNoticeText() {
        if (!isAccountAddressProfile()) return null;
        @Nullable
        String email = getUserEmail();
        if (email == null) return null;

        if (isAlreadySavedInAccount()) {
            return mContext
                    .getString(R.string.autofill_address_already_saved_in_account_source_notice)
                    .replace("$1", email);
        }

        return mContext.getString(R.string.autofill_address_will_be_saved_in_account_source_notice)
                .replace("$1", email);
    }

    private String getDeleteConfirmationTitle() {
        return mContext.getString(R.string.autofill_delete_address_confirmation_dialog_title);
    }

    private boolean isAlreadySavedInAccount() {
        // User edits an account address profile either from Chrome settings or upon form
        // submission.
        return mUserFlow == UPDATE_EXISTING_ADDRESS_PROFILE
                && mProfileToEdit.getSource() == Source.ACCOUNT;
    }

    private boolean isAddressSyncOn() {
        if (mSyncService == null) return false;
        return mSyncService.isSyncFeatureEnabled()
                && mSyncService.getSelectedTypes().contains(UserSelectableType.AUTOFILL);
    }

    /** Country based phone number validator. */
    private static class CountryAwarePhoneNumberValidator implements EditorFieldValidator {
        @Nullable
        private String mCountryCode;
        private final boolean mAllowEmptyValue;

        /**
         * Builds a country based phone number validator.
         *
         * @param allowEmptyValue whether null or 0-length string is considered valid.
         */
        CountryAwarePhoneNumberValidator(boolean allowEmptyValue) {
            mAllowEmptyValue = allowEmptyValue;
        }

        /**
         * Sets the country code used to validate the phone number.
         *
         * @param countryCode The given country code.
         */
        public void setCountryCode(@Nullable String countryCode) {
            mCountryCode = countryCode;
        }

        @Override
        public boolean isValid(@Nullable String value) {
            // Note that isPossibleNumber is used since the metadata in libphonenumber has to be
            // updated frequently (daily) to do more strict validation.
            return TextUtils.isEmpty(value) ? mAllowEmptyValue
                                            : PhoneNumberUtil.isPossibleNumber(value, mCountryCode);
        }

        @Override
        public boolean isLengthMaximum(@Nullable String value) {
            return false;
        }
    }
}

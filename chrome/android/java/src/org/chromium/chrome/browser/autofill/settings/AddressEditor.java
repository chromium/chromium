// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.os.Handler;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PhoneNumberUtil;
import org.chromium.chrome.browser.autofill.prefeditor.EditorBase;
import org.chromium.chrome.browser.autofill.prefeditor.EditorModel;
import org.chromium.chrome.browser.autofill.settings.AutofillProfileBridge.AddressField;
import org.chromium.chrome.browser.autofill.settings.AutofillProfileBridge.AddressUiComponent;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillAddress.CompletenessCheckType;
import org.chromium.components.autofill.prefeditor.EditorFieldModel;
import org.chromium.components.autofill.prefeditor.EditorFieldModel.EditorFieldValidator;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.UUID;

/**
 * An address editor. Can be used for either shipping or billing address editing.
 */
public class AddressEditor extends EditorBase<AutofillAddress> {
    private final Handler mHandler = new Handler();
    private final Map<Integer, EditorFieldModel> mAddressFields = new HashMap<>();
    private final Set<CharSequence> mPhoneNumbers = new HashSet<>();
    private final boolean mSaveToDisk;
    private final PhoneNumberUtil.CountryAwareFormatTextWatcher mPhoneFormatter;
    private final CountryAwarePhoneNumberValidator mPhoneValidator;
    @Nullable
    private AutofillProfileBridge mAutofillProfileBridge;
    @Nullable
    private EditorFieldModel mCountryField;
    @Nullable
    private EditorFieldModel mHonorificField;
    @Nullable
    private EditorFieldModel mPhoneField;
    @Nullable
    private EditorFieldModel mEmailField;
    @Nullable
    private EditorFieldModel mNicknameField;
    @Nullable
    private List<AddressUiComponent> mAddressUiComponents;
    @Nullable
    private String mCustomDoneButtonText;
    private AutofillProfile mProfile;
    private EditorModel mEditor;

    /**
     * Builds an address editor.
     *
     * @param saveToDisk Whether to save changes to disk after editing.
     */
    public AddressEditor(boolean saveToDisk) {
        mSaveToDisk = saveToDisk;
        mPhoneFormatter = new PhoneNumberUtil.CountryAwareFormatTextWatcher();
        mPhoneValidator = new CountryAwarePhoneNumberValidator(true);
    }

    /**
     * Adds the given phone number to the autocomplete set, if it's valid.
     * Note that here we consider all non-null and non-empty numbers as valid
     * since we are doing strict validation of Autofill data.
     *
     * @param phoneNumber The phone number to possibly add.
     */
    public void addPhoneNumberIfValid(@Nullable CharSequence phoneNumber) {
        if (!TextUtils.isEmpty(phoneNumber)) mPhoneNumbers.add(phoneNumber.toString());
    }

    /**
     * Sets the custom text to be shown on the done button.
     *
     * @param customDoneButtonText The text to display on the done button. If null, the default
     *        value will be used.
     */
    public void setCustomDoneButtonText(@Nullable String customDoneButtonText) {
        mCustomDoneButtonText = customDoneButtonText;
    }

    private boolean isUIForHonorificPrefixesEnabled() {
        return ChromeFeatureList.isEnabled(
                ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HONORIFIC_PREFIXES);
    }

    /**
     * Allows calling |edit| with a single callback used for both 'done' and 'cancel'.
     * @see #edit(AutofillAddress, Callback, Callback)
     */
    public void edit(
            @Nullable final AutofillAddress toEdit, final Callback<AutofillAddress> callback) {
        edit(toEdit, callback, callback);
    }

    /**
     * Builds and shows an editor model with the following fields.
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
    @Override
    public void edit(@Nullable final AutofillAddress toEdit,
            final Callback<AutofillAddress> doneCallback,
            final Callback<AutofillAddress> cancelCallback) {
        super.edit(toEdit, doneCallback, cancelCallback);

        if (mAutofillProfileBridge == null) mAutofillProfileBridge = new AutofillProfileBridge();

        // If |toEdit| is null, we're creating a new autofill profile with the country code of the
        // default locale on this device.
        final String editTitle;
        final AutofillAddress address;
        if (toEdit == null) {
            address = new AutofillAddress(
                    mContext, new AutofillProfile(), CompletenessCheckType.NORMAL);
            editTitle = mContext.getString(R.string.autofill_create_profile);
        } else {
            address = toEdit;
            editTitle = mContext.getString(R.string.autofill_edit_address_dialog_title);
        }

        mEditor = new EditorModel(editTitle, mCustomDoneButtonText);
        mProfile = address.getProfile();

        // The country dropdown is always present on the editor.
        if (mCountryField == null) {
            mCountryField = EditorFieldModel.createDropdown(
                    mContext.getString(R.string.autofill_profile_editor_country),
                    AutofillProfileBridge.getSupportedCountries(), null /* hint */);
        }

        // Changing the country will update which fields are in the model. The actual fields are not
        // discarded, so their contents are preserved.
        mCountryField.setDropdownCallback(new Callback<Pair<String, Runnable>>() {
            /**
             * If the selected country on the country dropdown list is changed,
             * the first element of eventData is the recently selected dropdown key,
             * the second element is the callback to invoke for when the dropdown
             * change has been processed.
             */
            @Override
            public void onResult(Pair<String, Runnable> eventData) {
                mEditor.removeAllFields();
                mPhoneFormatter.setCountryCode(eventData.first);
                mPhoneValidator.setCountryCode(eventData.first);
                addAddressFieldsToEditor(eventData.first, Locale.getDefault().getLanguage());
                // Notify EditorDialog that the fields in the model have changed. EditorDialog
                // should re-read the model and update the UI accordingly.
                mHandler.post(eventData.second);
            }
        });

        // Country dropdown is cached, so the selected item needs to be updated for the new profile
        // that's being edited. This will not fire the dropdown callback.
        mCountryField.setValue(AutofillAddress.getCountryCode(mProfile));

        // Phone number validator and formatter are cached, so their country code needs to be
        // updated for the new profile that's being edited.
        assert mCountryField.getValue() != null;
        mPhoneValidator.setCountryCode(mCountryField.getValue().toString());
        mPhoneFormatter.setCountryCode(mCountryField.getValue().toString());

        // Honorific prefix is present only for autofill settings.
        if (isUIForHonorificPrefixesEnabled()) {
            if (mHonorificField == null) {
                mHonorificField = EditorFieldModel.createTextInput();
                mHonorificField.setLabel(
                        mContext.getString(R.string.autofill_profile_editor_honorific_prefix));
            }
            // Retrieve and set the honorific prefix value.
            mHonorificField.setValue(mProfile.getHonorificPrefix());
        }

        // There's a finite number of fields for address editing. Changing the country will re-order
        // and relabel the fields. The meaning of each field remains the same.
        if (mAddressFields.isEmpty()) {
            // Not use INPUT_TYPE_HINT_REGION to avoid capitalizing all characters.
            mAddressFields.put(AddressField.ADMIN_AREA, EditorFieldModel.createTextInput());

            // City, dependent locality, and organization don't have any special formatting hints.
            mAddressFields.put(AddressField.LOCALITY, EditorFieldModel.createTextInput());
            mAddressFields.put(AddressField.DEPENDENT_LOCALITY, EditorFieldModel.createTextInput());
            mAddressFields.put(AddressField.ORGANIZATION, EditorFieldModel.createTextInput());

            // Sorting code and postal code (a.k.a. ZIP code) should show both letters and digits on
            // the keyboard, if possible.
            mAddressFields.put(AddressField.SORTING_CODE,
                    EditorFieldModel.createTextInput(
                            EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC));
            mAddressFields.put(AddressField.POSTAL_CODE,
                    EditorFieldModel.createTextInput(
                            EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC));

            // Street line field can contain \n to indicate line breaks.
            mAddressFields.put(AddressField.STREET_ADDRESS,
                    EditorFieldModel.createTextInput(
                            EditorFieldModel.INPUT_TYPE_HINT_STREET_LINES));

            // Android has special formatting rules for names.
            mAddressFields.put(AddressField.RECIPIENT,
                    EditorFieldModel.createTextInput(EditorFieldModel.INPUT_TYPE_HINT_PERSON_NAME));
        }

        // Phone number is present for all countries.
        if (mPhoneField == null) {
            mPhoneField = EditorFieldModel.createTextInput(EditorFieldModel.INPUT_TYPE_HINT_PHONE,
                    mContext.getString(R.string.autofill_profile_editor_phone_number),
                    mPhoneNumbers, mPhoneFormatter, mPhoneValidator, null /* valueIconGenerator */,
                    /* requiredErrorMessage */ null,
                    mContext.getString(R.string.payments_phone_invalid_validation_message),
                    EditorFieldModel.LENGTH_COUNTER_LIMIT_NONE, null /* value */);
        }
        // Phone number field is cached, so its value needs to be updated for every new profile
        // that's being edited.
        mPhoneField.setValue(mProfile.getPhoneNumber());

        if (mEmailField == null) {
            mEmailField = EditorFieldModel.createTextInput(EditorFieldModel.INPUT_TYPE_HINT_EMAIL,
                    mContext.getString(R.string.autofill_profile_editor_email_address),
                    null /* suggestions */, null /* formatter */, null /* validator */,
                    null /* valueIconGenerator */, null /* requiredErrorMessage */,
                    mContext.getString(R.string.payments_email_invalid_validation_message),
                    EditorFieldModel.LENGTH_COUNTER_LIMIT_NONE, null /* value */);
        }
        // Retrieve and set the email address field.
        mEmailField.setValue(mProfile.getEmailAddress());

        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.AUTOFILL_ADDRESS_PROFILE_SAVE_PROMPT_NICKNAME_SUPPORT)) {
            if (mNicknameField == null) {
                mNicknameField = EditorFieldModel.createTextInput();
                // TODO(crbug.com/1167061): Use localized string.
                mNicknameField.setLabel("Label");
            }
        }

        // If the user clicks [Cancel], send |toEdit| address back to the caller, which was the
        // original state (could be null, a complete address, a partial address).
        mEditor.setCancelCallback(() -> cancelCallback.onResult(toEdit));

        // If the user clicks [Done], save changes on disk, mark the address "complete" if possible,
        // and send it back to the caller.
        mEditor.setDoneCallback(() -> {
            commitChanges(mProfile);

            // The address cannot be marked "complete" because it has not been checked
            // for all required fields.
            address.updateAddress(mProfile);

            doneCallback.onResult(address);
        });

        // This should be called when all required fields are put in mAddressField.
        setAddressFieldValuesFromCache();
        addAddressFieldsToEditor(mCountryField.getValue().toString(), mProfile.getLanguageCode());
        mEditorDialog.show(mEditor);
    }

    /** Saves the edited profile on disk. */
    private void commitChanges(AutofillProfile profile) {
        // Country code and phone number are always required and are always collected from the
        // editor model.
        profile.setCountryCode(mCountryField.getValue().toString());
        if (mPhoneField != null) profile.setPhoneNumber(mPhoneField.getValue().toString());
        if (mEmailField != null) profile.setEmailAddress(mEmailField.getValue().toString());
        if (mHonorificField != null) {
            profile.setHonorificPrefix(mHonorificField.getValue().toString());
        }

        // Autofill profile bridge normalizes the language code for the autofill profile.
        profile.setLanguageCode(mAutofillProfileBridge.getCurrentBestLanguageCode());

        // Collect data from all visible fields and store it in the autofill profile.
        Set<Integer> visibleFields = new HashSet<>();
        for (int i = 0; i < mAddressUiComponents.size(); i++) {
            AddressUiComponent component = mAddressUiComponents.get(i);
            visibleFields.add(component.id);
            if (component.id != AddressField.COUNTRY) {
                setProfileField(profile, component.id, mAddressFields.get(component.id).getValue());
            }
        }

        // Clear the fields that are hidden from the user interface, so
        // AutofillAddress.toPaymentAddress() will send them to the renderer as empty strings.
        for (Map.Entry<Integer, EditorFieldModel> entry : mAddressFields.entrySet()) {
            if (!visibleFields.contains(entry.getKey())) {
                setProfileField(profile, entry.getKey(), "");
            }
        }

        // Save the edited autofill profile locally.
        if (mSaveToDisk) {
            profile.setGUID(PersonalDataManager.getInstance().setProfileToLocal(mProfile));
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
            AutofillProfile profile, int field, @Nullable CharSequence value) {
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

    private static String ensureNotNull(@Nullable CharSequence value) {
        return value == null ? "" : value.toString();
    }

    private void setAddressFieldValuesFromCache() {
        // Address fields are cached, so their values need to be updated for every new profile
        // that's being edited.
        for (Map.Entry<Integer, EditorFieldModel> entry : mAddressFields.entrySet()) {
            entry.getValue().setValue(AutofillAddress.getProfileField(mProfile, entry.getKey()));
        }
    }

    /**
     * Adds fields to the editor model based on the country and language code of
     * the profile that's being edited.
     *
     * For example, "US" will not add dependent locality to the editor. A "JP" address will start
     * with a person's full name or with a prefecture name, depending on whether the language code
     * is "ja-Latn" or "ja".
     */
    private void addAddressFieldsToEditor(String countryCode, String languageCode) {
        mAddressUiComponents =
                mAutofillProfileBridge.getAddressUiComponents(countryCode, languageCode);
        // In terms of order, country must be the first field.
        mEditor.addField(mCountryField);
        for (int i = 0; i < mAddressUiComponents.size(); i++) {
            AddressUiComponent component = mAddressUiComponents.get(i);

            // Honorific prefix should go before name.
            if (component.id == AddressField.RECIPIENT && mHonorificField != null) {
                mEditor.addField(mHonorificField);
            }

            EditorFieldModel field = mAddressFields.get(component.id);

            // Labels depend on country, e.g., state is called province in some countries. These are
            // already localized.
            field.setLabel(component.label);
            field.setIsFullLine(component.isFullLine || component.id == AddressField.LOCALITY
                    || component.id == AddressField.DEPENDENT_LOCALITY);

            mEditor.addField(field);
        }
        // Phone number (and email/nickname if applicable) are the last fields of the address.
        if (mPhoneField != null) mEditor.addField(mPhoneField);
        if (mEmailField != null) mEditor.addField(mEmailField);
        if (mNicknameField != null) mEditor.addField(mNicknameField);
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
        public boolean isValid(@Nullable CharSequence value) {
            // Note that isPossibleNumber is used since the metadata in libphonenumber has to be
            // updated frequently (daily) to do more strict validation.
            return TextUtils.isEmpty(value)
                    ? mAllowEmptyValue
                    : PhoneNumberUtil.isPossibleNumber(value.toString(), mCountryCode);
        }

        @Override
        public boolean isLengthMaximum(@Nullable CharSequence value) {
            return false;
        }
    }
}

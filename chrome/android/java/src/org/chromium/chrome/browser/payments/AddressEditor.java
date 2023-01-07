// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.app.ProgressDialog;
import android.os.Handler;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.GetSubKeysRequestDelegate;
import org.chromium.chrome.browser.autofill.PhoneNumberUtil;
import org.chromium.chrome.browser.autofill.prefeditor.EditorBase;
import org.chromium.chrome.browser.autofill.prefeditor.EditorModel;
import org.chromium.chrome.browser.autofill.settings.AutofillProfileBridge;
import org.chromium.chrome.browser.autofill.settings.AutofillProfileBridge.AddressField;
import org.chromium.chrome.browser.autofill.settings.AutofillProfileBridge.AddressUiComponent;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.autofill.prefeditor.EditorFieldModel;
import org.chromium.components.autofill.prefeditor.EditorFieldModel.EditorFieldValidator;
import org.chromium.payments.mojom.AddressErrors;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.UUID;

/**
 * An address editor. Can be used for either shipping or billing address editing.
 *
 * Note that this class is used by PaymentRequest only and will be removed when not needed any more.
 * Please use {@link org.chromium.chrome.browser.autofill.settings.AddressEditor} instead.
 */
@Deprecated
public class AddressEditor
        extends EditorBase<AutofillAddress> implements GetSubKeysRequestDelegate {
    @IntDef({Purpose.PAYMENT_REQUEST, Purpose.AUTOFILL_SETTINGS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Purpose {
        int PAYMENT_REQUEST = 1;
        int AUTOFILL_SETTINGS = 2;
    }

    private final Handler mHandler = new Handler();
    private final Map<Integer, EditorFieldModel> mAddressFields = new HashMap<>();
    private final Set<CharSequence> mPhoneNumbers = new HashSet<>();
    @Purpose
    private final int mPurpose;
    private final boolean mCheckRequiredFields;
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
    private List<AddressUiComponent> mAddressUiComponents;
    private boolean mAdminAreasLoaded;
    private String mRecentlySelectedCountry;
    private Runnable mCountryChangeCallback;
    private AutofillProfile mProfile;
    private EditorModel mEditor;
    private ProgressDialog mProgressDialog;
    @Nullable
    private AddressErrors mAddressErrors;

    /**
     * Builds an address editor.
     *
     * @param purpose    The purpose of this address editor. One of
     *                   Purpose.PAYMENT_REQUEST or Purpose.AUTOFILL_SETTINGS.
     * @param saveToDisk Whether to save changes to disk after editing.
     */
    public AddressEditor(@Purpose int purpose, boolean saveToDisk) {
        mPurpose = purpose;
        mCheckRequiredFields = mPurpose != Purpose.AUTOFILL_SETTINGS;
        mSaveToDisk = saveToDisk;
        mPhoneFormatter = new PhoneNumberUtil.CountryAwareFormatTextWatcher();
        mPhoneValidator = new CountryAwarePhoneNumberValidator(!mCheckRequiredFields);
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
     * Sets the address errors to indicate error messages from merchant's retry() call.
     *
     * @param errors The address errors from merchant's retry() call.
     */
    public void setAddressErrors(@Nullable AddressErrors errors) {
        mAddressErrors = errors;
    }

    private boolean isUIForHonorificPrefixesEnabled() {
        return ChromeFeatureList.isEnabled(
                ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HONORIFIC_PREFIXES);
    }

    private String getAddressError(int field) {
        if (mAddressErrors == null) return null;

        switch (field) {
            case AddressField.COUNTRY:
                return mAddressErrors.country;
            case AddressField.ADMIN_AREA:
                return mAddressErrors.region;
            case AddressField.LOCALITY:
                return mAddressErrors.city;
            case AddressField.DEPENDENT_LOCALITY:
                return mAddressErrors.dependentLocality;
            case AddressField.SORTING_CODE:
                return mAddressErrors.sortingCode;
            case AddressField.POSTAL_CODE:
                return mAddressErrors.postalCode;
            case AddressField.STREET_ADDRESS:
                return mAddressErrors.addressLine;
            case AddressField.ORGANIZATION:
                return mAddressErrors.organization;
            case AddressField.RECIPIENT:
                return mAddressErrors.recipient;
        }
        assert false : "Unrecognized address field code";
        return null;
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
            address = new AutofillAddress(mContext, new AutofillProfile());
            editTitle = mContext.getString(R.string.autofill_create_profile);
        } else {
            address = toEdit;
            editTitle = toEdit.getEditTitle();
        }

        mEditor = new EditorModel(editTitle);
        mProfile = address.getProfile();

        // When edit is called, a new form is started, so the country on the
        // dropdown list is not changed. => mRecentlySelectedCountry should be null.
        mRecentlySelectedCountry = null;

        // The country dropdown is always present on the editor.
        if (mCountryField == null) {
            mCountryField = EditorFieldModel.createDropdown(
                    mContext.getString(R.string.autofill_profile_editor_country),
                    AutofillProfileBridge.getSupportedCountries(),
                    null /* hint */);
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
                showProgressDialog();
                mRecentlySelectedCountry = eventData.first;
                mPhoneFormatter.setCountryCode(mRecentlySelectedCountry);
                mPhoneValidator.setCountryCode(mRecentlySelectedCountry);
                mCountryChangeCallback = eventData.second;
                loadAdminAreasForCountry(mRecentlySelectedCountry);
            }
        });

        // Country dropdown is cached, so the selected item needs to be updated for the new profile
        // that's being edited. This will not fire the dropdown callback.
        mCountryField.setValue(AutofillAddress.getCountryCode(mProfile));

        // Phone number validator and formatter are cached, so their contry code needs to be updated
        // for the new profile that's being edited.
        assert mCountryField.getValue() != null;
        mPhoneValidator.setCountryCode(mCountryField.getValue().toString());
        mPhoneFormatter.setCountryCode(mCountryField.getValue().toString());

        // Honorific prefix is present only for autofill settings.
        if (mPurpose == Purpose.AUTOFILL_SETTINGS && isUIForHonorificPrefixesEnabled()) {
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
            // City, dependent locality, and organization don't have any special formatting hints.
            mAddressFields.put(AddressField.LOCALITY, EditorFieldModel.createTextInput());
            mAddressFields.put(AddressField.DEPENDENT_LOCALITY, EditorFieldModel.createTextInput());
            mAddressFields.put(AddressField.ORGANIZATION, EditorFieldModel.createTextInput());

            // Sorting code and postal code (a.k.a. ZIP code) should show both letters and digits on
            // the keyboard, if possible.
            mAddressFields.put(AddressField.SORTING_CODE, EditorFieldModel.createTextInput(
                    EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC));
            mAddressFields.put(AddressField.POSTAL_CODE, EditorFieldModel.createTextInput(
                    EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC));

            // Street line field can contain \n to indicate line breaks.
            mAddressFields.put(AddressField.STREET_ADDRESS, EditorFieldModel.createTextInput(
                    EditorFieldModel.INPUT_TYPE_HINT_STREET_LINES));

            // Android has special formatting rules for names.
            mAddressFields.put(AddressField.RECIPIENT, EditorFieldModel.createTextInput(
                    EditorFieldModel.INPUT_TYPE_HINT_PERSON_NAME));
        }

        // Phone number is present for all countries.
        if (mPhoneField == null) {
            String requiredErrorMessage = mCheckRequiredFields
                    ? mContext.getString(
                            R.string.pref_edit_dialog_field_required_validation_message)
                    : null;
            mPhoneField = EditorFieldModel.createTextInput(EditorFieldModel.INPUT_TYPE_HINT_PHONE,
                    mContext.getString(R.string.autofill_profile_editor_phone_number),
                    mPhoneNumbers, mPhoneFormatter, mPhoneValidator, null /* valueIconGenerator */,
                    requiredErrorMessage,
                    mContext.getString(R.string.payments_phone_invalid_validation_message),
                    EditorFieldModel.LENGTH_COUNTER_LIMIT_NONE, null /* value */);
        }

        // Phone number field is cached, so its value needs to be updated for every new profile
        // that's being edited.
        mPhoneField.setValue(mProfile.getPhoneNumber());

        // Email address is present only for autofill settings.
        if (mPurpose == Purpose.AUTOFILL_SETTINGS) {
            if (mEmailField == null) {
                mEmailField = EditorFieldModel.createTextInput(
                        EditorFieldModel.INPUT_TYPE_HINT_EMAIL,
                        mContext.getString(R.string.autofill_profile_editor_email_address),
                        null /* suggestions */, null /* formatter */, null /* validator */,
                        null /* valueIconGenerator */, null /* requiredErrorMessage */,
                        mContext.getString(R.string.payments_email_invalid_validation_message),
                        EditorFieldModel.LENGTH_COUNTER_LIMIT_NONE, null /* value */);
            }
            // Retrieve and set the email address field.
            mEmailField.setValue(mProfile.getEmailAddress());
        }

        // If the user clicks [Cancel], send |toEdit| address back to the caller, which was the
        // original state (could be null, a complete address, a partial address).
        mEditor.setCancelCallback(() -> {
            // This makes sure that onSubKeysReceived returns early if it's
            // ever called when Cancel has already occurred.
            mAdminAreasLoaded = true;
            PersonalDataManager.getInstance().cancelPendingGetSubKeys();
            cancelCallback.onResult(toEdit);
        });

        // If the user clicks [Done], save changes on disk, mark the address "complete" if possible,
        // and send it back to the caller.
        mEditor.setDoneCallback(() -> {
            mAdminAreasLoaded = true;
            PersonalDataManager.getInstance().cancelPendingGetSubKeys();
            commitChanges(mProfile);
            if (mCheckRequiredFields) {
                address.completeAddress(mProfile);
            } else {
                // The address cannot be marked "complete" because it has not been checked
                // for all required fields.
                address.updateAddress(mProfile);
            }
            doneCallback.onResult(address);
        });

        loadAdminAreasForCountry(mCountryField.getValue().toString());
        if (mAddressErrors != null) mEditorDialog.validateForm();
    }

    private void showProgressDialog() {
        mProgressDialog = new ProgressDialog(mContext);
        mProgressDialog.setMessage(mContext.getText(R.string.payments_loading_message));
        mProgressDialog.show();
    }

    private void dismissProgressDialog() {
        if (mProgressDialog != null && mProgressDialog.isShowing()) {
            mProgressDialog.dismiss();
        }
        mProgressDialog = null;
    }

    /** Saves the edited profile on disk. */
    private void commitChanges(AutofillProfile profile) {
        // Country code and phone number are always required and are always collected from the
        // editor model.
        profile.setCountryCode(mCountryField.getValue().toString());
        profile.setPhoneNumber(mPhoneField.getValue().toString());
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

    @Override
    public void onSubKeysReceived(
            @Nullable String[] adminAreaCodes, @Nullable String[] adminAreaNames) {
        if (mAdminAreasLoaded) return;
        mAdminAreasLoaded = true;
        // If the dialog is already dismissed, it doesn't make sense to show it.
        // This can happen if the dialog is dismissed while we are getting the
        // subkeys.
        if (mEditorDialog.isDismissed()) return;

        // When there is a timeout in the subkey request process, the admin area codes/names will be
        // null.
        mAddressFields.put(AddressField.ADMIN_AREA,
                (adminAreaCodes != null && adminAreaNames != null && adminAreaCodes.length != 0
                        && adminAreaCodes.length == adminAreaNames.length)
                        ? EditorFieldModel.createDropdown(null /* label */,
                                  AutofillProfileBridge.getAdminAreaDropdownList(
                                          adminAreaCodes, adminAreaNames),
                                  mContext.getString(R.string.select))
                        : EditorFieldModel.createTextInput(
                                  EditorFieldModel.INPUT_TYPE_HINT_REGION));

        // Admin areas need to be fetched in two cases:
        // 1. Initial loading of the form.
        // 2. When the selected country is changed in the form.
        // mRecentlySelectedCountry is not null if and only if it's the second case
        if (mRecentlySelectedCountry != null) {
            dismissProgressDialog();
            // Both country code and language code dictate which fields should be added to the
            // editor.
            // For example, "US" will not add dependent locality to the editor. A "JP" address will
            // start with a person's full name or a with a prefecture name, depending on whether the
            // language code is "ja-Latn" or "ja".
            addAddressFieldsToEditor(mRecentlySelectedCountry, Locale.getDefault().getLanguage());
            // Notify EditorDialog that the fields in the model have changed. EditorDialog should
            // re-read the model and update the UI accordingly.
            mHandler.post(mCountryChangeCallback);
        } else {
            // This should be called when all required fields are put in mAddressField.
            setAddressFieldValuesFromCache();
            addAddressFieldsToEditor(
                    mCountryField.getValue().toString(), mProfile.getLanguageCode());
            mEditorDialog.show(mEditor);
        }
    }

    private static boolean contains(String[] haystack, String needle) {
        if (haystack == null || haystack.length == 0) return false;
        if (TextUtils.isEmpty(needle)) return true;
        for (int i = 0; i < haystack.length; ++i) {
            if (TextUtils.equals(haystack[i], needle)) return true;
        }
        return false;
    }

    /** Requests the list of admin areas. */
    private void loadAdminAreasForCountry(String countryCode) {
        // Used to check if the callback is called (for the cancellation).
        mAdminAreasLoaded = false;
        mEditorDialog.setAsNotDismissed();

        // For tests, the time-out is set to 0. In this case, we should not
        // fetch the admin-areas, and show a text-field instead.
        // This is to have the tests independent of the network status.
        if (PersonalDataManager.getRequestTimeoutMS() == 0) {
            onSubKeysReceived(null, null);
            return;
        }

        // In each rule, admin area keys are saved under sub-keys of country.
        PersonalDataManager.getInstance().loadRulesForSubKeys(countryCode);
        PersonalDataManager.getInstance().getRegionSubKeys(countryCode, this);
    }

    /**
     * Adds fields to the editor model based on the country and language code of
     * the profile that's being edited.
     */
    private void addAddressFieldsToEditor(String countryCode, String languageCode) {
        mAddressUiComponents =
                mAutofillProfileBridge.getAddressUiComponents(countryCode, languageCode);
        // In terms of order, country must be the first field.
        mCountryField.setCustomErrorMessage(getAddressError(AddressField.COUNTRY));
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

            // Libaddressinput formats do not always require the full name (RECIPIENT), but
            // PaymentRequest does.
            field.setRequiredErrorMessage(mCheckRequiredFields
                                    && (component.isRequired
                                            || component.id == AddressField.RECIPIENT)
                            ? mContext.getString(
                                    R.string.pref_edit_dialog_field_required_validation_message)
                            : null);

            field.setCustomErrorMessage(getAddressError(component.id));
            mEditor.addField(field);
        }
        // Phone number (and email if applicable) are the last fields of the address.
        mPhoneField.setCustomErrorMessage(mAddressErrors != null ? mAddressErrors.phone : null);
        mEditor.addField(mPhoneField);
        if (mEmailField != null) mEditor.addField(mEmailField);
    }

    /** Country based phone number validator. */
    private static class CountryAwarePhoneNumberValidator implements EditorFieldValidator {
        @Nullable
        private String mCountryCode;
        private boolean mAllowEmptyValue;

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
            // TODO(gogerald): Warn users when the phone number is a possible number but may be
            // invalid, crbug.com/736387.
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

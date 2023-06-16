// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DONE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_CALLBACK;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_HINT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_FIELDS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_TITLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.CUSTOM_ERROR_MESSAGE;
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
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.LENGTH_COUNTER_LIMIT_NONE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_FORMATTER;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_INPUT_TYPE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_LENGTH_COUNTER_LIMIT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_SUGGESTIONS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.ALPHA_NUMERIC_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.PERSON_NAME_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.PHONE_NUMBER_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.PLAIN_TEXT_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.REGION_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType.STREET_ADDRESS_INPUT;

import android.app.ProgressDialog;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AddressValidationType;
import org.chromium.chrome.browser.autofill.AutofillAddress;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge.AddressField;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge.AddressUiComponent;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.GetSubKeysRequestDelegate;
import org.chromium.chrome.browser.autofill.PhoneNumberUtil;
import org.chromium.chrome.browser.autofill.editors.EditorBase;
import org.chromium.chrome.browser.autofill.editors.EditorDialogViewBinder;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.EditorFieldValidator;
import org.chromium.payments.mojom.AddressErrors;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
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
    private final Map<Integer, ListItem> mAddressFields = new HashMap<>();
    private final Set<String> mPhoneNumbers = new HashSet<>();
    private final boolean mSaveToDisk;
    private final PhoneNumberUtil.CountryAwareFormatTextWatcher mPhoneFormatter;
    private final CountryAwarePhoneNumberValidator mPhoneValidator;
    @Nullable
    private AutofillProfileBridge mAutofillProfileBridge;
    @Nullable
    private PropertyModel mCountryField;
    @Nullable
    private PropertyModel mPhoneField;
    @Nullable
    private List<AddressUiComponent> mAddressUiComponents;
    private boolean mAdminAreasLoaded;
    private String mRecentlySelectedCountry;
    private AutofillProfile mProfile;
    private ProgressDialog mProgressDialog;
    @Nullable
    private AddressErrors mAddressErrors;

    /**
     * Builds an address editor.
     *
     * @param saveToDisk Whether to save changes to disk after editing.
     */
    public AddressEditor(boolean saveToDisk) {
        mSaveToDisk = saveToDisk;
        mPhoneFormatter = new PhoneNumberUtil.CountryAwareFormatTextWatcher();
        mPhoneValidator = new CountryAwarePhoneNumberValidator();
    }

    /**
     * Adds the given phone number to the autocomplete set, if it's valid.
     * Note that here we consider all non-null and non-empty numbers as valid
     * since we are doing strict validation of Autofill data.
     *
     * @param phoneNumber The phone number to possibly add.
     */
    public void addPhoneNumberIfValid(@Nullable String phoneNumber) {
        if (!TextUtils.isEmpty(phoneNumber)) mPhoneNumbers.add(phoneNumber);
    }

    /**
     * Sets the address errors to indicate error messages from merchant's retry() call.
     *
     * @param errors The address errors from merchant's retry() call.
     */
    public void setAddressErrors(@Nullable AddressErrors errors) {
        mAddressErrors = errors;
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
     * [ an address field    ] \
     * [ an address field    ]  \
     *         ...                <-- field order, presence, required, and labels depend on country.
     * [ an address field    ]  /
     * [ an address field    ] /
     * [ phone number field  ] <----- phone is always present.
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
            address = new AutofillAddress(mContext, AutofillProfile.builder().build());
            editTitle = mContext.getString(R.string.autofill_create_profile);
        } else {
            address = toEdit;
            editTitle = toEdit.getEditTitle();
        }

        mProfile = address.getProfile();

        // When edit is called, a new form is started, so the country on the
        // dropdown list is not changed. => mRecentlySelectedCountry should be null.
        mRecentlySelectedCountry = null;

        // The country dropdown is always present on the editor.
        if (mCountryField == null) {
            mCountryField =
                    new PropertyModel.Builder(DROPDOWN_ALL_KEYS)
                            .with(LABEL,
                                    mContext.getString(R.string.autofill_profile_editor_country))
                            .with(DROPDOWN_KEY_VALUE_LIST,
                                    AutofillProfileBridge.getSupportedCountries())
                            .with(IS_FULL_LINE, true)
                            .build();
        }

        mCountryField.set(DROPDOWN_CALLBACK, new Callback<String>() {
            /**
             * Load admin areas for the selected country.
             */
            @Override
            public void onResult(String countryCode) {
                showProgressDialog();
                mRecentlySelectedCountry = countryCode;
                mPhoneFormatter.setCountryCode(mRecentlySelectedCountry);
                mPhoneValidator.setCountryCode(mRecentlySelectedCountry);
                loadAdminAreasForCountry(mRecentlySelectedCountry);
            }
        });

        // Country dropdown is cached, so the selected item needs to be updated for the new profile
        // that's being edited. This will not fire the dropdown callback.
        mCountryField.set(VALUE, AutofillAddress.getCountryCode(mProfile));

        // Phone number validator and formatter are cached, so their contry code needs to be updated
        // for the new profile that's being edited.
        final String countryValue = mCountryField.get(VALUE);
        assert countryValue != null;
        mPhoneValidator.setCountryCode(countryValue);
        mPhoneFormatter.setCountryCode(countryValue);

        // There's a finite number of fields for address editing. Changing the country will re-order
        // and relabel the fields. The meaning of each field remains the same.
        if (mAddressFields.isEmpty()) {
            // City, dependent locality, and organization don't have any special formatting hints.
            mAddressFields.put(AddressField.LOCALITY,
                    new ListItem(TEXT_INPUT,
                            new PropertyModel.Builder(TEXT_ALL_KEYS)
                                    .with(TEXT_INPUT_TYPE, PLAIN_TEXT_INPUT)
                                    .build()));
            mAddressFields.put(AddressField.DEPENDENT_LOCALITY,
                    new ListItem(TEXT_INPUT,
                            new PropertyModel.Builder(TEXT_ALL_KEYS)
                                    .with(TEXT_INPUT_TYPE, PLAIN_TEXT_INPUT)
                                    .build()));
            mAddressFields.put(AddressField.ORGANIZATION,
                    new ListItem(TEXT_INPUT,
                            new PropertyModel.Builder(TEXT_ALL_KEYS)
                                    .with(TEXT_INPUT_TYPE, PLAIN_TEXT_INPUT)
                                    .build()));

            // Sorting code and postal code (a.k.a. ZIP code) should show both letters and digits on
            // the keyboard, if possible.
            mAddressFields.put(AddressField.SORTING_CODE,
                    new ListItem(TEXT_INPUT,
                            new PropertyModel.Builder(TEXT_ALL_KEYS)
                                    .with(TEXT_INPUT_TYPE, ALPHA_NUMERIC_INPUT)
                                    .build()));
            mAddressFields.put(AddressField.POSTAL_CODE,
                    new ListItem(TEXT_INPUT,
                            new PropertyModel.Builder(TEXT_ALL_KEYS)
                                    .with(TEXT_INPUT_TYPE, ALPHA_NUMERIC_INPUT)
                                    .build()));

            // Street line field can contain \n to indicate line breaks.
            mAddressFields.put(AddressField.STREET_ADDRESS,
                    new ListItem(TEXT_INPUT,
                            new PropertyModel.Builder(TEXT_ALL_KEYS)
                                    .with(TEXT_INPUT_TYPE, STREET_ADDRESS_INPUT)
                                    .build()));

            // Android has special formatting rules for names.
            mAddressFields.put(AddressField.RECIPIENT,
                    new ListItem(TEXT_INPUT,
                            new PropertyModel.Builder(TEXT_ALL_KEYS)
                                    .with(TEXT_INPUT_TYPE, PERSON_NAME_INPUT)
                                    .build()));
        }

        // Phone number is present for all countries.
        if (mPhoneField == null) {
            String requiredErrorMessage =
                    mContext.getString(R.string.pref_edit_dialog_field_required_validation_message);
            mPhoneField =
                    new PropertyModel.Builder(TEXT_ALL_KEYS)
                            .with(TEXT_INPUT_TYPE, PHONE_NUMBER_INPUT)
                            .with(LABEL,
                                    mContext.getString(
                                            R.string.autofill_profile_editor_phone_number))
                            .with(TEXT_SUGGESTIONS, new ArrayList<>(mPhoneNumbers))
                            .with(TEXT_FORMATTER, mPhoneFormatter)
                            .with(VALIDATOR, mPhoneValidator)
                            .with(IS_REQUIRED, true)
                            .with(REQUIRED_ERROR_MESSAGE, requiredErrorMessage)
                            .with(INVALID_ERROR_MESSAGE,
                                    mContext.getString(
                                            R.string.payments_phone_invalid_validation_message))
                            .with(IS_FULL_LINE, true)
                            .with(TEXT_LENGTH_COUNTER_LIMIT, LENGTH_COUNTER_LIMIT_NONE)
                            .build();
        }

        // Phone number field is cached, so its value needs to be updated for every new profile
        // that's being edited.
        mPhoneField.set(VALUE, mProfile.getPhoneNumber());

        // If the user clicks [Cancel], send |toEdit| address back to the caller, which was the
        // original state (could be null, a complete address, a partial address).
        Runnable onCancel = () -> {
            // This makes sure that onSubKeysReceived returns early if it's
            // ever called when Cancel has already occurred.
            mAdminAreasLoaded = true;
            PersonalDataManager.getInstance().cancelPendingGetSubKeys();
            cancelCallback.onResult(toEdit);

            // Clean up the state of this editor.
            reset();
        };

        // If the user clicks [Done], save changes on disk, mark the address "complete" if possible,
        // and send it back to the caller.
        Runnable onDone = () -> {
            mAdminAreasLoaded = true;
            PersonalDataManager.getInstance().cancelPendingGetSubKeys();
            commitChanges(mProfile);
            address.completeAddress(mProfile);
            doneCallback.onResult(address);

            // Clean up the state of this editor.
            reset();
        };

        mEditorModel = new PropertyModel.Builder(ALL_KEYS)
                               .with(EDITOR_TITLE, editTitle)
                               .with(SHOW_REQUIRED_INDICATOR, true)
                               .with(EDITOR_FIELDS, new ListModel())
                               .with(DONE_RUNNABLE, onDone)
                               .with(CANCEL_RUNNABLE, onCancel)
                               .build();
        mEditorMCP = PropertyModelChangeProcessor.create(
                mEditorModel, mEditorDialog, EditorDialogViewBinder::bindEditorDialogView, false);

        loadAdminAreasForCountry(mCountryField.get(VALUE));
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
        profile.setCountryCode(mCountryField.get(VALUE));
        profile.setPhoneNumber(mPhoneField.get(VALUE));

        // Autofill profile bridge normalizes the language code for the autofill profile.
        profile.setLanguageCode(mAutofillProfileBridge.getCurrentBestLanguageCode());

        // Collect data from all visible fields and store it in the autofill profile.
        Set<Integer> visibleFields = new HashSet<>();
        for (int i = 0; i < mAddressUiComponents.size(); i++) {
            AddressUiComponent component = mAddressUiComponents.get(i);
            visibleFields.add(component.id);
            if (component.id != AddressField.COUNTRY) {
                setProfileField(
                        profile, component.id, mAddressFields.get(component.id).model.get(VALUE));
            }
        }

        // Clear the fields that are hidden from the user interface, so
        // AutofillAddress.toPaymentAddress() will send them to the renderer as empty strings.
        for (Map.Entry<Integer, ListItem> entry : mAddressFields.entrySet()) {
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

    private void setAddressFieldValuesFromCache() {
        // Address fields are cached, so their values need to be updated for every new profile
        // that's being edited.
        for (Map.Entry<Integer, ListItem> entry : mAddressFields.entrySet()) {
            entry.getValue().model.set(
                    VALUE, AutofillAddress.getProfileField(mProfile, entry.getKey()));
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
                        ? new ListItem(DROPDOWN,
                                new PropertyModel.Builder(DROPDOWN_ALL_KEYS)
                                        .with(DROPDOWN_KEY_VALUE_LIST,
                                                AutofillProfileBridge.getAdminAreaDropdownList(
                                                        adminAreaCodes, adminAreaNames))
                                        .with(DROPDOWN_HINT, mContext.getString(R.string.select))
                                        .build())
                        : new ListItem(TEXT_INPUT,
                                new PropertyModel.Builder(TEXT_ALL_KEYS)
                                        .with(TEXT_INPUT_TYPE, REGION_INPUT)
                                        .build()));

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
        } else {
            // This should be called when all required fields are put in mAddressField.
            setAddressFieldValuesFromCache();
            addAddressFieldsToEditor(mCountryField.get(VALUE), mProfile.getLanguageCode());
            mEditorDialog.show(mEditorModel);
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
        ListModel<ListItem> editorFields = new ListModel<>();
        mAddressUiComponents = mAutofillProfileBridge.getAddressUiComponents(
                countryCode, languageCode, AddressValidationType.PAYMENT_REQUEST);
        // In terms of order, country must be the first field.
        mCountryField.set(CUSTOM_ERROR_MESSAGE, getAddressError(AddressField.COUNTRY));
        editorFields.add(new ListItem(DROPDOWN, mCountryField));
        for (int i = 0; i < mAddressUiComponents.size(); i++) {
            AddressUiComponent component = mAddressUiComponents.get(i);

            ListItem fieldItem = mAddressFields.get(component.id);
            PropertyModel field = fieldItem.model;

            // Labels depend on country, e.g., state is called province in some countries. These are
            // already localized.
            field.set(LABEL, component.label);
            field.set(IS_FULL_LINE,
                    component.isFullLine || component.id == AddressField.LOCALITY
                            || component.id == AddressField.DEPENDENT_LOCALITY);

            // Libaddressinput formats do not always require the full name (RECIPIENT), but
            // PaymentRequest does.
            if (component.isRequired || component.id == AddressField.RECIPIENT) {
                field.set(IS_REQUIRED, true);
                field.set(REQUIRED_ERROR_MESSAGE,
                        mContext.getString(
                                R.string.pref_edit_dialog_field_required_validation_message));
            }

            field.set(CUSTOM_ERROR_MESSAGE, getAddressError(component.id));
            editorFields.add(fieldItem);
        }
        // Phone number (and email if applicable) are the last fields of the address.
        mPhoneField.set(CUSTOM_ERROR_MESSAGE, mAddressErrors != null ? mAddressErrors.phone : null);
        editorFields.add(new ListItem(TEXT_INPUT, mPhoneField));
        mEditorModel.set(EDITOR_FIELDS, editorFields);
    }

    /** Country based phone number validator. */
    private static class CountryAwarePhoneNumberValidator implements EditorFieldValidator {
        @Nullable
        private String mCountryCode;

        /**
         * Builds a country based phone number validator.
         */
        CountryAwarePhoneNumberValidator() {}

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
            // TODO(gogerald): Warn users when the phone number is a possible number but may be
            // invalid, crbug.com/736387.
            // Note that isPossibleNumber is used since the metadata in libphonenumber has to be
            // updated frequently (daily) to do more strict validation.
            return !TextUtils.isEmpty(value)
                    && PhoneNumberUtil.isPossibleNumber(value, mCountryCode);
        }

        @Override
        public boolean isLengthMaximum(@Nullable String value) {
            return false;
        }
    }
}

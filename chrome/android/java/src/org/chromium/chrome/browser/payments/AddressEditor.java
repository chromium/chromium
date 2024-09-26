// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ALLOW_DELETE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DONE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_CALLBACK;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_HINT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_FIELDS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_TITLE;
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
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_SUGGESTIONS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.VALIDATE_ON_SHOW;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.VISIBLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.scrollToFieldWithErrorMessage;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.validateForm;

import android.app.ProgressDialog;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AddressValidationType;
import org.chromium.chrome.browser.autofill.AutofillAddress;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge.AutofillAddressUiComponent;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PhoneNumberUtil;
import org.chromium.chrome.browser.autofill.SubKeyRequesterFactory;
import org.chromium.chrome.browser.autofill.editors.EditorBase;
import org.chromium.chrome.browser.autofill.editors.EditorDialogViewBinder;
import org.chromium.chrome.browser.autofill.editors.EditorFieldValidator;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldItem;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.FieldType;
import org.chromium.components.autofill.SubKeyRequester;
import org.chromium.components.autofill.SubKeyRequester.GetSubKeysRequestDelegate;
import org.chromium.payments.mojom.AddressErrors;
import org.chromium.ui.modelutil.ListModel;
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
import java.util.function.Predicate;

/**
 * An address editor. Can be used for either shipping or billing address editing.
 *
 * Note that this class is used by PaymentRequest only and will be removed when not needed any more.
 * Please use {@link org.chromium.chrome.browser.autofill.settings.AddressEditor} instead.
 */
@Deprecated
public class AddressEditor extends EditorBase<AutofillAddress>
        implements GetSubKeysRequestDelegate {
    private final Map<Integer, PropertyModel> mAddressFields = new HashMap<>();
    private final Set<String> mPhoneNumbers = new HashSet<>();
    private final PersonalDataManager mPersonalDataManager;
    private final boolean mSaveToDisk;
    private final PhoneNumberUtil.CountryAwareFormatTextWatcher mPhoneFormatter;
    @Nullable private AutofillProfileBridge mAutofillProfileBridge;
    @Nullable private PropertyModel mCountryField;
    @Nullable private PropertyModel mPhoneField;
    @Nullable private PropertyModel mAdminAreaField;
    private @ItemType int mAdminAreaFieldType;
    @Nullable private List<AutofillAddressUiComponent> mAddressUiComponents;
    private boolean mAdminAreasLoaded;
    private String mRecentlySelectedCountry;
    private Callback<AutofillAddress> mDoneCallback;
    private Callback<AutofillAddress> mCancelCallback;
    private boolean mAddressNew;
    private AutofillAddress mAddress;
    private AutofillProfile mProfile;
    private ProgressDialog mProgressDialog;
    @Nullable private AddressErrors mAddressErrors;

    private PropertyModel getFieldForFieldType(@FieldType int fieldType) {
        if (!mAddressFields.containsKey(fieldType)) {
            mAddressFields.put(
                    fieldType,
                    new PropertyModel.Builder(TEXT_ALL_KEYS)
                            .with(TEXT_FIELD_TYPE, fieldType)
                            .build());
        }

        return mAddressFields.get(fieldType);
    }

    /**
     * Builds an address editor.
     *
     * @param saveToDisk Whether to save changes to disk after editing.
     */
    public AddressEditor(PersonalDataManager personalDataManager, boolean saveToDisk) {
        mPersonalDataManager = personalDataManager;
        mSaveToDisk = saveToDisk;
        mPhoneFormatter = new PhoneNumberUtil.CountryAwareFormatTextWatcher();
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
            case FieldType.ADDRESS_HOME_COUNTRY:
                return mAddressErrors.country;
            case FieldType.ADDRESS_HOME_STATE:
                return mAddressErrors.region;
            case FieldType.ADDRESS_HOME_CITY:
                return mAddressErrors.city;
            case FieldType.ADDRESS_HOME_DEPENDENT_LOCALITY:
                return mAddressErrors.dependentLocality;
            case FieldType.ADDRESS_HOME_SORTING_CODE:
                return mAddressErrors.sortingCode;
            case FieldType.ADDRESS_HOME_ZIP:
                return mAddressErrors.postalCode;
            case FieldType.ADDRESS_HOME_STREET_ADDRESS:
                return mAddressErrors.addressLine;
            case FieldType.COMPANY_NAME:
                return mAddressErrors.organization;
            case FieldType.NAME_FULL:
                return mAddressErrors.recipient;
            default:
                assert false : "Unrecognized server field type: " + field;
                return null;
        }
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
    public void edit(
            @Nullable final AutofillAddress toEdit,
            final Callback<AutofillAddress> doneCallback,
            final Callback<AutofillAddress> cancelCallback) {
        super.edit(toEdit, doneCallback, cancelCallback);
        if (mAutofillProfileBridge == null) mAutofillProfileBridge = new AutofillProfileBridge();
        mDoneCallback = doneCallback;
        mCancelCallback = cancelCallback;

        // If |toEdit| is null, we're creating a new autofill profile with the country code of the
        // default locale on this device.
        final String editTitle;
        mAddressNew = toEdit == null;
        if (mAddressNew) {
            mAddress =
                    new AutofillAddress(
                            mContext, AutofillProfile.builder().build(), mPersonalDataManager);
            editTitle = mContext.getString(R.string.autofill_create_profile);
        } else {
            mAddress = toEdit;
            editTitle = toEdit.getEditTitle();
        }

        mProfile = mAddress.getProfile();

        // When edit is called, a new form is started, so the country on the
        // dropdown list is not changed. => mRecentlySelectedCountry should be null.
        mRecentlySelectedCountry = null;

        // The country dropdown is always present on the editor.
        if (mCountryField == null) {
            mCountryField =
                    new PropertyModel.Builder(DROPDOWN_ALL_KEYS)
                            .with(
                                    LABEL,
                                    mContext.getString(R.string.autofill_profile_editor_country))
                            .with(
                                    DROPDOWN_KEY_VALUE_LIST,
                                    AutofillProfileBridge.getSupportedCountries())
                            .with(IS_REQUIRED, false)
                            .build();
        }

        mCountryField.set(
                DROPDOWN_CALLBACK,
                new Callback<String>() {
                    /** Load admin areas for the selected country. */
                    @Override
                    public void onResult(String countryCode) {
                        showProgressDialog();
                        mRecentlySelectedCountry = countryCode;
                        mPhoneFormatter.setCountryCode(mRecentlySelectedCountry);
                        loadAdminAreasForCountry(mRecentlySelectedCountry);
                    }
                });

        // Country dropdown is cached, so the selected item needs to be updated for the new profile
        // that's being edited. This will not fire the dropdown callback.
        mCountryField.set(VALUE, AutofillAddress.getCountryCode(mProfile, mPersonalDataManager));

        // Phone number validator and formatter are cached, so their contry code needs to be updated
        // for the new profile that's being edited.
        final String countryValue = mCountryField.get(VALUE);
        assert countryValue != null;
        mPhoneFormatter.setCountryCode(countryValue);

        // Phone number is present for all countries.
        if (mPhoneField == null) {
            mPhoneField =
                    new PropertyModel.Builder(TEXT_ALL_KEYS)
                            .with(TEXT_FIELD_TYPE, FieldType.PHONE_HOME_WHOLE_NUMBER)
                            .with(
                                    LABEL,
                                    mContext.getString(
                                            R.string.autofill_profile_editor_phone_number))
                            .with(TEXT_SUGGESTIONS, new ArrayList<>(mPhoneNumbers))
                            .with(TEXT_FORMATTER, mPhoneFormatter)
                            .with(IS_REQUIRED, true)
                            .build();
        }

        // Phone number field is cached, so its value needs to be updated for every new profile
        // that's being edited.
        mPhoneField.set(VALUE, mProfile.getInfo(FieldType.PHONE_HOME_WHOLE_NUMBER));

        mEditorModel =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(EDITOR_TITLE, editTitle)
                        .with(SHOW_REQUIRED_INDICATOR, true)
                        .with(EDITOR_FIELDS, new ListModel())
                        .with(DONE_RUNNABLE, this::onDone)
                        .with(CANCEL_RUNNABLE, this::onCancel)
                        .with(ALLOW_DELETE, false)
                        // Form validation must be performed only for non-empty address profiles.
                        .with(VALIDATE_ON_SHOW, !mAddressNew)
                        .build();
        mEditorMCP =
                PropertyModelChangeProcessor.create(
                        mEditorModel, mEditorDialog, EditorDialogViewBinder::bindEditorDialogView);

        loadAdminAreasForCountry(mCountryField.get(VALUE));
    }

    private void onDone() {
        if (!validateForm(mEditorModel)) {
            scrollToFieldWithErrorMessage(mEditorModel);
            return;
        }
        mEditorModel.set(VISIBLE, false);

        // This makes sure that onSubKeysReceived returns early if it's
        // ever called when Done has already occurred.
        mAdminAreasLoaded = true;
        SubKeyRequesterFactory.getInstance().cancelPendingGetSubKeys();

        // Commit changes to the address and send modified address to the caller.
        commitChanges(mProfile);
        mAddress.completeAddress(mProfile);
        mDoneCallback.onResult(mAddress);

        // Clean up the state of this editor.
        reset();
    }

    private void onCancel() {
        mEditorModel.set(VISIBLE, false);

        // This makes sure that onSubKeysReceived returns early if it's
        // ever called when Cancel has already occurred.
        mAdminAreasLoaded = true;
        SubKeyRequesterFactory.getInstance().cancelPendingGetSubKeys();

        // Send unchanged address to the caller.
        mCancelCallback.onResult(mAddressNew ? null : mAddress);

        // Clean up the state of this editor.
        reset();
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
        profile.setInfo(FieldType.ADDRESS_HOME_COUNTRY, mCountryField.get(VALUE));
        profile.setInfo(FieldType.PHONE_HOME_WHOLE_NUMBER, mPhoneField.get(VALUE));

        // Autofill profile bridge normalizes the language code for the autofill profile.
        profile.setLanguageCode(mAutofillProfileBridge.getCurrentBestLanguageCode());

        // Collect data from all visible fields and store it in the autofill profile.
        for (AutofillAddressUiComponent component : mAddressUiComponents) {
            PropertyModel fieldModel =
                    component.id == FieldType.ADDRESS_HOME_STATE
                            ? mAdminAreaField
                            : mAddressFields.get(component.id);
            if (component.id != FieldType.ADDRESS_HOME_COUNTRY) {
                profile.setInfo(component.id, fieldModel.get(VALUE));
            }
        }

        // Save the edited autofill profile locally.
        if (mSaveToDisk) {
            profile.setGUID(mPersonalDataManager.setProfileToLocal(mProfile));
        }

        if (profile.getGUID().isEmpty()) {
            assert !mSaveToDisk;

            // Set a fake guid for a new temp AutofillProfile to be used in CardEditor. Note that
            // this temp AutofillProfile should not be saved to disk.
            profile.setGUID(UUID.randomUUID().toString());
        }
    }

    private void setAddressFieldValuesFromCache() {
        // Address fields are cached, so their values need to be updated for every new profile
        // that's being edited.
        for (Map.Entry<Integer, PropertyModel> entry : mAddressFields.entrySet()) {
            entry.getValue().set(VALUE, mProfile.getInfo(entry.getKey()));
        }
        mAdminAreaField.set(VALUE, mProfile.getInfo(FieldType.ADDRESS_HOME_STATE));
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

        initializeAdminAreaField(adminAreaCodes, adminAreaNames);

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
            addAddressFieldsToEditor(mCountryField.get(VALUE), mProfile.getLanguageCode());
            // Populate fields with values once they've been created.
            setAddressFieldValuesFromCache();
            mEditorModel.set(VISIBLE, true);
        }
    }

    private void initializeAdminAreaField(
            @Nullable String[] adminAreaCodes, @Nullable String[] adminAreaNames) {
        // When there is a timeout in the subkey request process, the admin area codes/names will be
        // null.
        if (adminAreaCodes == null
                || adminAreaNames == null
                || adminAreaCodes.length == 0
                || adminAreaCodes.length != adminAreaNames.length) {
            mAdminAreaField =
                    new PropertyModel.Builder(TEXT_ALL_KEYS)
                            .with(TEXT_FIELD_TYPE, FieldType.ADDRESS_HOME_STATE)
                            .build();
            mAdminAreaFieldType = TEXT_INPUT;
            return;
        }
        mAdminAreaField =
                new PropertyModel.Builder(DROPDOWN_ALL_KEYS)
                        .with(
                                DROPDOWN_KEY_VALUE_LIST,
                                AutofillProfileBridge.getAdminAreaDropdownList(
                                        adminAreaCodes, adminAreaNames))
                        .with(DROPDOWN_HINT, mContext.getString(R.string.select))
                        .build();
        mAdminAreaFieldType = DROPDOWN;
    }

    /** Requests the list of admin areas. */
    private void loadAdminAreasForCountry(String countryCode) {
        // Used to check if the callback is called (for the cancellation).
        mAdminAreasLoaded = false;
        mEditorDialog.setAsNotDismissed();

        // For tests, the time-out is set to 0. In this case, we should not
        // fetch the admin-areas, and show a text-field instead.
        // This is to have the tests independent of the network status.
        if (SubKeyRequester.getRequestTimeoutMS() == 0) {
            onSubKeysReceived(null, null);
            return;
        }

        // In each rule, admin area keys are saved under sub-keys of country.
        SubKeyRequesterFactory.getInstance().loadRulesForSubKeys(countryCode);
        SubKeyRequesterFactory.getInstance().getRegionSubKeys(countryCode, this);
    }

    /**
     * Adds fields to the editor model based on the country and language code of
     * the profile that's being edited.
     */
    private void addAddressFieldsToEditor(String countryCode, String languageCode) {
        ListModel<FieldItem> editorFields = new ListModel<>();
        mAddressUiComponents =
                mAutofillProfileBridge.getAddressUiComponents(
                        countryCode, languageCode, AddressValidationType.PAYMENT_REQUEST);
        // In terms of order, country must be the first field.
        editorFields.add(new FieldItem(DROPDOWN, mCountryField, /* isFullLine= */ true));
        for (AutofillAddressUiComponent component : mAddressUiComponents) {
            final PropertyModel field;
            final @ItemType int fieldType;
            if (component.id == FieldType.ADDRESS_HOME_STATE) {
                field = mAdminAreaField;
                fieldType = mAdminAreaFieldType;
            } else {
                field = getFieldForFieldType(component.id);
                fieldType = TEXT_INPUT;
            }

            // Labels depend on country, e.g., state is called province in some countries. These are
            // already localized.
            field.set(LABEL, component.label);

            field.set(
                    VALIDATOR,
                    EditorFieldValidator.builder()
                            .withInitialErrorMessage(getAddressError(component.id))
                            .build());
            // Libaddressinput formats do not always require the full name (RECIPIENT), but
            // PaymentRequest does.
            if (component.isRequired || component.id == FieldType.NAME_FULL) {
                field.set(IS_REQUIRED, true);
                field.get(VALIDATOR)
                        .setRequiredErrorMessage(
                                mContext.getString(
                                        R.string
                                                .pref_edit_dialog_field_required_validation_message));
            } else {
                field.set(IS_REQUIRED, false);
            }

            boolean isFullLine =
                    component.isFullLine
                            || component.id == FieldType.ADDRESS_HOME_CITY
                            || component.id == FieldType.ADDRESS_HOME_DEPENDENT_LOCALITY;
            editorFields.add(new FieldItem(fieldType, field, isFullLine));
        }
        // Phone number (and email if applicable) are the last fields of the address.
        mPhoneField.set(VALIDATOR, getPhoneValidator(countryCode));
        editorFields.add(new FieldItem(TEXT_INPUT, mPhoneField, /* isFullLine= */ true));
        mEditorModel.set(EDITOR_FIELDS, editorFields);
    }

    private EditorFieldValidator getPhoneValidator(String countryCode) {
        // TODO(crbug.com/40527351): Warn users when the phone number is a possible number but may
        // be
        // invalid.
        // Note that isPossibleNumber is used since the metadata in libphonenumber has to be
        // updated frequently (daily) to do more strict validation.
        Predicate<String> validationPredicate =
                value ->
                        !TextUtils.isEmpty(value)
                                && PhoneNumberUtil.isPossibleNumber(value, countryCode);

        return EditorFieldValidator.builder()
                .withRequiredErrorMessage(
                        mContext.getString(
                                R.string.pref_edit_dialog_field_required_validation_message))
                .withValidationPredicate(
                        validationPredicate,
                        mContext.getString(R.string.payments_phone_invalid_validation_message))
                .withInitialErrorMessage(mAddressErrors != null ? mAddressErrors.phone : null)
                .build();
    }
}

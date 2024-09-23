// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.anyOf;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.isEmptyString;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.UserFlow.MIGRATE_EXISTING_ADDRESS_PROFILE;
import static org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.UserFlow.SAVE_NEW_ADDRESS_PROFILE;
import static org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.UserFlow.UPDATE_EXISTING_ADDRESS_PROFILE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CUSTOM_DONE_BUTTON_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_CONFIRMATION_TEXT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DELETE_CONFIRMATION_TITLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DONE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_FIELDS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FOOTER_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.TEXT_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.SHOW_REQUIRED_INDICATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_FIELD_TYPE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.setDropdownKey;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillAddress;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge.AutofillAddressUiComponent;
import org.chromium.chrome.browser.autofill.AutofillProfileBridgeJni;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.autofill.PhoneNumberUtil;
import org.chromium.chrome.browser.autofill.PhoneNumberUtilJni;
import org.chromium.chrome.browser.autofill.editors.AddressEditorCoordinator.Delegate;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownKeyValue;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldItem;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.FieldType;
import org.chromium.components.autofill.RecordType;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Spliterator;
import java.util.Spliterators;
import java.util.stream.Collectors;
import java.util.stream.StreamSupport;

/** Unit tests for autofill address editor. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AddressEditorTest {
    private static final String USER_EMAIL = "example@gmail.com";
    private static final Locale DEFAULT_LOCALE = Locale.getDefault();
    private static final List<AutofillAddressUiComponent> SUPPORTED_ADDRESS_FIELDS =
            List.of(
                    new AutofillAddressUiComponent(
                            FieldType.NAME_FULL, "full name label", true, true),
                    new AutofillAddressUiComponent(
                            FieldType.ADDRESS_HOME_STATE, "admin area label", false, true),
                    new AutofillAddressUiComponent(
                            FieldType.ADDRESS_HOME_CITY, "locality label", true, false),
                    new AutofillAddressUiComponent(
                            FieldType.ADDRESS_HOME_DEPENDENT_LOCALITY,
                            "dependent locality label",
                            true,
                            false),
                    new AutofillAddressUiComponent(
                            FieldType.COMPANY_NAME, "organization label", false, true),
                    new AutofillAddressUiComponent(
                            FieldType.ADDRESS_HOME_SORTING_CODE,
                            "sorting code label",
                            false,
                            false),
                    new AutofillAddressUiComponent(
                            FieldType.ADDRESS_HOME_ZIP, "postal code label", true, false),
                    new AutofillAddressUiComponent(
                            FieldType.ADDRESS_HOME_STREET_ADDRESS,
                            "street address label",
                            true,
                            true));

    private static final AutofillProfile sLocalProfile =
            AutofillProfile.builder()
                    .setFullName("Seb Doe")
                    .setCompanyName("Google")
                    .setStreetAddress("111 First St")
                    .setRegion("CA")
                    .setLocality("Los Angeles")
                    .setPostalCode("90291")
                    .setCountryCode("US")
                    .setPhoneNumber("650-253-0000")
                    .setEmailAddress("first@gmail.com")
                    .setLanguageCode("en-US")
                    .build();
    private static final AutofillProfile sAccountProfile =
            AutofillProfile.builder()
                    .setRecordType(RecordType.ACCOUNT)
                    .setFullName("Seb Doe")
                    .setCompanyName("Google")
                    .setStreetAddress("111 First St")
                    .setRegion("CA")
                    .setLocality("Los Angeles")
                    .setPostalCode("90291")
                    .setCountryCode("US")
                    .setPhoneNumber("650-253-0000")
                    .setEmailAddress("first@gmail.com")
                    .setLanguageCode("en-US")
                    .build();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private AutofillProfileBridge.Natives mAutofillProfileBridgeJni;
    @Mock private PhoneNumberUtil.Natives mPhoneNumberUtilJni;

    @Mock private EditorDialogView mEditorDialog;
    @Mock private SyncService mSyncService;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private PersonalDataManager mPersonalDataManager;
    @Mock private Profile mProfile;
    @Mock private Delegate mDelegate;

    @Captor private ArgumentCaptor<AutofillAddress> mAddressCapture;

    private final CoreAccountInfo mAccountInfo =
            CoreAccountInfo.createFromEmailAndGaiaId(USER_EMAIL, "gaia_id");
    // Note: can't initialize this list statically because of how Robolectric
    // initializes Android library dependencies.
    private final List<DropdownKeyValue> mSupportedCountries =
            List.of(
                    new DropdownKeyValue("US", "United States"),
                    new DropdownKeyValue("DE", "Germany"),
                    new DropdownKeyValue("CU", "Cuba"));

    @Nullable private AutofillAddress mEditedAutofillAddress;

    private Activity mActivity;
    private AddressEditorCoordinator mAddressEditor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Locale.setDefault(Locale.US);
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        mJniMocker.mock(AutofillProfileBridgeJni.TEST_HOOKS, mAutofillProfileBridgeJni);
        doAnswer(
                        invocation -> {
                            List<Integer> requiredFields =
                                    (List<Integer>) invocation.getArguments()[1];
                            requiredFields.addAll(
                                    List.of(
                                            FieldType.NAME_FULL,
                                            FieldType.ADDRESS_HOME_CITY,
                                            FieldType.ADDRESS_HOME_DEPENDENT_LOCALITY,
                                            FieldType.ADDRESS_HOME_ZIP));
                            return null;
                        })
                .when(mAutofillProfileBridgeJni)
                .getRequiredFields(anyString(), anyList());
        mJniMocker.mock(PhoneNumberUtilJni.TEST_HOOKS, mPhoneNumberUtilJni);
        when(mPhoneNumberUtilJni.isPossibleNumber(anyString(), anyString())).thenReturn(true);

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(mAccountInfo);

        when(mSyncService.getSelectedTypes()).thenReturn(new HashSet());
        SyncServiceFactory.setInstanceForTesting(mSyncService);

        when(mPersonalDataManager.isCountryEligibleForAccountStorage(anyString())).thenReturn(true);
        when(mPersonalDataManager.getDefaultCountryCodeForNewAddress()).thenReturn("US");
        PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManager);

        setUpSupportedCountries(mSupportedCountries);

        when(mEditorDialog.getContext()).thenReturn(mActivity);
    }

    @After
    public void tearDown() {
        // Reset default locale to avoid changing it for other tests.
        Locale.setDefault(DEFAULT_LOCALE);
    }

    private void setUpSupportedCountries(List<DropdownKeyValue> supportedCountries) {
        doAnswer(
                        invocation -> {
                            List<String> contryCodes = (List<String>) invocation.getArguments()[0];
                            List<String> contryNames = (List<String>) invocation.getArguments()[1];

                            for (DropdownKeyValue keyValue : supportedCountries) {
                                contryCodes.add(keyValue.getKey());
                                contryNames.add(keyValue.getValue().toString());
                            }

                            return null;
                        })
                .when(mAutofillProfileBridgeJni)
                .getSupportedCountries(anyList(), anyList());
    }

    private void setUpAddressUiComponents(
            List<AutofillAddressUiComponent> addressUiComponents, String countryCode) {
        doAnswer(
                        invocation -> {
                            List<Integer> componentIds =
                                    (List<Integer>) invocation.getArguments()[3];
                            List<String> componentNames =
                                    (List<String>) invocation.getArguments()[4];
                            List<Integer> componentRequired =
                                    (List<Integer>) invocation.getArguments()[5];
                            List<Integer> componentLength =
                                    (List<Integer>) invocation.getArguments()[6];

                            for (AutofillAddressUiComponent component : addressUiComponents) {
                                componentIds.add(component.id);
                                componentNames.add(component.label);
                                componentRequired.add(component.isRequired ? 1 : 0);
                                componentLength.add(component.isFullLine ? 1 : 0);
                            }
                            return "EN";
                        })
                .when(mAutofillProfileBridgeJni)
                .getAddressUiComponents(
                        eq(countryCode),
                        anyString(),
                        anyInt(),
                        anyList(),
                        anyList(),
                        anyList(),
                        anyList());
    }

    private void setUpAddressUiComponents(List<AutofillAddressUiComponent> addressUiComponents) {
        setUpAddressUiComponents(addressUiComponents, "US");
    }

    private static void validateTextField(
            FieldItem fieldItem,
            String value,
            int textFieldType,
            String label,
            boolean isRequired,
            boolean isFullLine) {
        assertEquals(TEXT_INPUT, fieldItem.type);
        assertEquals(isFullLine, fieldItem.isFullLine);

        PropertyModel field = fieldItem.model;
        assertEquals(value, field.get(VALUE));
        assertEquals(textFieldType, field.get(TEXT_FIELD_TYPE));
        assertEquals(label, field.get(LABEL));
        assertEquals(isRequired, field.get(IS_REQUIRED));
    }

    private static void checkModelHasExpectedValues(
            PropertyModel editorModel,
            String expectedDeleteTitle,
            String expectedDeleteText,
            @Nullable String expectedRecordTypeNotice) {
        assertNotNull(editorModel);

        assertFalse(editorModel.get(SHOW_REQUIRED_INDICATOR));
        assertEquals(expectedDeleteTitle, editorModel.get(DELETE_CONFIRMATION_TITLE));
        assertEquals(expectedDeleteText, editorModel.get(DELETE_CONFIRMATION_TEXT));
        assertEquals(expectedRecordTypeNotice, editorModel.get(FOOTER_MESSAGE));
    }

    private void validateShownFields(
            PropertyModel editorModel, AutofillProfile profile, boolean shouldMarkFieldsRequired) {
        validateShownFields(
                editorModel,
                profile,
                shouldMarkFieldsRequired,
                /* shouldMarkFieldsRequiredWhenAddressFieldEmpty= */ false);
    }

    private void validateShownFields(
            PropertyModel editorModel,
            AutofillProfile profile,
            boolean shouldMarkFieldsRequired,
            boolean shouldMarkFieldsRequiredWhenAddressFieldEmpty) {
        assertNotNull(editorModel);
        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        // editorFields[0] - country dropdown.
        // editorFields[1] - full name field.
        // editorFields[2] - admin area field.
        // editorFields[3] - locality field.
        // editorFields[4] - dependent locality field.
        // editorFields[5] - organization field.
        // editorFields[6] - sorting code field.
        // editorFields[7] - postal code field.
        // editorFields[8] - street address field.
        // editorFields[9] - phone number field.
        // editorFields[10] - email field.
        assertEquals(11, editorFields.size());

        // Fields obtained from backend must be placed after the country dropdown.
        validateTextField(
                editorFields.get(1),
                profile.getFullName(),
                FieldType.NAME_FULL,
                /* label= */ "full name label",
                shouldMarkFieldsRequired,
                /* isFullLine= */ true);
        validateTextField(
                editorFields.get(2),
                profile.getRegion(),
                FieldType.ADDRESS_HOME_STATE,
                /* label= */ "admin area label",
                /* isRequired= */ false,
                /* isFullLine= */ true);
        // Locality field is forced to occupy full line.
        validateTextField(
                editorFields.get(3),
                profile.getLocality(),
                FieldType.ADDRESS_HOME_CITY,
                /* label= */ "locality label",
                shouldMarkFieldsRequired,
                /* isFullLine= */ true);

        // Note: dependent locality is a required field for address profiles stored in Google
        // account, but it's still marked as optional by the editor when the corresponding field in
        // the existing address profile is empty. It is considered required for new address
        // profiles.
        validateTextField(
                editorFields.get(4),
                profile.getDependentLocality(),
                FieldType.ADDRESS_HOME_DEPENDENT_LOCALITY,
                /* label= */ "dependent locality label",
                shouldMarkFieldsRequiredWhenAddressFieldEmpty,
                /* isFullLine= */ true);

        validateTextField(
                editorFields.get(5),
                profile.getCompanyName(),
                FieldType.COMPANY_NAME,
                /* label= */ "organization label",
                /* isRequired= */ false,
                /* isFullLine= */ true);

        validateTextField(
                editorFields.get(6),
                profile.getSortingCode(),
                FieldType.ADDRESS_HOME_SORTING_CODE,
                /* label= */ "sorting code label",
                /* isRequired= */ false,
                /* isFullLine= */ false);
        validateTextField(
                editorFields.get(7),
                profile.getPostalCode(),
                FieldType.ADDRESS_HOME_ZIP,
                /* label= */ "postal code label",
                shouldMarkFieldsRequired,
                /* isFullLine= */ false);
        validateTextField(
                editorFields.get(8),
                profile.getStreetAddress(),
                FieldType.ADDRESS_HOME_STREET_ADDRESS,
                /* label= */ "street address label",
                shouldMarkFieldsRequired,
                /* isFullLine= */ true);
    }

    private void validateErrorMessages(PropertyModel editorModel, boolean errorsPresent) {
        assertNotNull(editorModel);
        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(11, editorFields.size());

        Matcher<String> requiredFieldMatcher =
                errorsPresent ? not(isEmptyString()) : anyOf(nullValue(), isEmptyString());
        assertThat(editorFields.get(0).model.get(ERROR_MESSAGE), requiredFieldMatcher);
        assertThat(editorFields.get(1).model.get(ERROR_MESSAGE), requiredFieldMatcher);
        assertThat(
                editorFields.get(2).model.get(ERROR_MESSAGE), anyOf(nullValue(), isEmptyString()));
        assertThat(editorFields.get(3).model.get(ERROR_MESSAGE), requiredFieldMatcher);
        assertThat(editorFields.get(4).model.get(ERROR_MESSAGE), requiredFieldMatcher);
        assertThat(
                editorFields.get(5).model.get(ERROR_MESSAGE), anyOf(nullValue(), isEmptyString()));
        assertThat(
                editorFields.get(6).model.get(ERROR_MESSAGE), anyOf(nullValue(), isEmptyString()));
        assertThat(editorFields.get(7).model.get(ERROR_MESSAGE), requiredFieldMatcher);
        assertThat(editorFields.get(8).model.get(ERROR_MESSAGE), requiredFieldMatcher);
        assertThat(
                editorFields.get(9).model.get(ERROR_MESSAGE), anyOf(nullValue(), isEmptyString()));
        assertThat(
                editorFields.get(10).model.get(ERROR_MESSAGE), anyOf(nullValue(), isEmptyString()));
    }

    @Test
    @SmallTest
    public void validateCustomDoneButtonText() {
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity, mDelegate, mProfile, /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.setCustomDoneButtonText("Custom done");
        mAddressEditor.showEditorDialog();

        PropertyModel editorModel = mAddressEditor.getEditorModelForTesting();
        assertNotNull(editorModel);

        assertEquals("Custom done", editorModel.get(CUSTOM_DONE_BUTTON_TEXT));
    }

    @Test
    @SmallTest
    public void validateUIStrings_NewAddressProfile() {
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity, mDelegate, mProfile, /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity.getString(R.string.autofill_delete_local_address_record_type_notice);
        final String recordTypeNotice = null;

        checkModelHasExpectedValues(
                mAddressEditor.getEditorModelForTesting(),
                deleteTitle,
                deleteText,
                recordTypeNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_NewAddressProfile_EligibleForAddressAccountStorage() {
        setUpAddressUiComponents(new ArrayList());
        when(mPersonalDataManager.isEligibleForAddressAccountStorage()).thenReturn(true);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity, mDelegate, mProfile, /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity
                        .getString(R.string.autofill_delete_account_address_record_type_notice)
                        .replace("$1", USER_EMAIL);
        final String recordTypeNotice =
                mActivity
                        .getString(
                                R.string
                                        .autofill_address_will_be_saved_in_account_record_type_notice)
                        .replace("$1", USER_EMAIL);

        checkModelHasExpectedValues(
                mAddressEditor.getEditorModelForTesting(),
                deleteTitle,
                deleteText,
                recordTypeNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_LocalOrSyncAddressProfile_AddressSyncDisabled() {
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, sLocalProfile, mPersonalDataManager),
                        SAVE_NEW_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity.getString(R.string.autofill_delete_local_address_record_type_notice);
        final String recordTypeNotice = null;

        checkModelHasExpectedValues(
                mAddressEditor.getEditorModelForTesting(),
                deleteTitle,
                deleteText,
                recordTypeNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_LocalOrSyncAddressProfile_AddressSyncEnabled() {
        setUpAddressUiComponents(new ArrayList());
        when(mSyncService.getSelectedTypes())
                .thenReturn(Collections.singleton(UserSelectableType.AUTOFILL));
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, sLocalProfile, mPersonalDataManager),
                        SAVE_NEW_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);

        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity.getString(R.string.autofill_delete_sync_address_record_type_notice);
        final String recordTypeNotice = null;

        checkModelHasExpectedValues(
                mAddressEditor.getEditorModelForTesting(),
                deleteTitle,
                deleteText,
                recordTypeNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_UpdateLocalOrSyncAddressProfile_AddressSyncDisabled() {
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, sLocalProfile, mPersonalDataManager),
                        UPDATE_EXISTING_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity.getString(R.string.autofill_delete_local_address_record_type_notice);
        final String recordTypeNotice = null;

        checkModelHasExpectedValues(
                mAddressEditor.getEditorModelForTesting(),
                deleteTitle,
                deleteText,
                recordTypeNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_UpdateLocalOrSyncAddressProfile_AddressSyncEnabled() {
        setUpAddressUiComponents(new ArrayList());
        when(mSyncService.getSelectedTypes())
                .thenReturn(Collections.singleton(UserSelectableType.AUTOFILL));
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, sLocalProfile, mPersonalDataManager),
                        UPDATE_EXISTING_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);

        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity.getString(R.string.autofill_delete_sync_address_record_type_notice);
        final String recordTypeNotice = null;

        checkModelHasExpectedValues(
                mAddressEditor.getEditorModelForTesting(),
                deleteTitle,
                deleteText,
                recordTypeNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_LocalAddressProfile_MigrationToAccount() {
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, sLocalProfile, mPersonalDataManager),
                        MIGRATE_EXISTING_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);

        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity
                        .getString(R.string.autofill_delete_account_address_record_type_notice)
                        .replace("$1", USER_EMAIL);
        final String recordTypeNotice =
                mActivity
                        .getString(
                                R.string
                                        .autofill_address_will_be_saved_in_account_record_type_notice)
                        .replace("$1", USER_EMAIL);

        checkModelHasExpectedValues(
                mAddressEditor.getEditorModelForTesting(),
                deleteTitle,
                deleteText,
                recordTypeNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_SyncAddressProfile_MigrationToAccount() {
        setUpAddressUiComponents(new ArrayList());
        when(mSyncService.getSelectedTypes())
                .thenReturn(Collections.singleton(UserSelectableType.AUTOFILL));
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, sLocalProfile, mPersonalDataManager),
                        MIGRATE_EXISTING_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);

        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity
                        .getString(R.string.autofill_delete_account_address_record_type_notice)
                        .replace("$1", USER_EMAIL);
        final String recordTypeNotice =
                mActivity
                        .getString(
                                R.string
                                        .autofill_address_will_be_saved_in_account_record_type_notice)
                        .replace("$1", USER_EMAIL);

        checkModelHasExpectedValues(
                mAddressEditor.getEditorModelForTesting(),
                deleteTitle,
                deleteText,
                recordTypeNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_AccountAddressProfile_SaveInAccountFlow() {
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, sAccountProfile, mPersonalDataManager),
                        SAVE_NEW_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity
                        .getString(R.string.autofill_delete_account_address_record_type_notice)
                        .replace("$1", USER_EMAIL);
        final String recordTypeNotice =
                mActivity
                        .getString(
                                R.string
                                        .autofill_address_will_be_saved_in_account_record_type_notice)
                        .replace("$1", USER_EMAIL);

        checkModelHasExpectedValues(
                mAddressEditor.getEditorModelForTesting(),
                deleteTitle,
                deleteText,
                recordTypeNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_AccountAddressProfile_UpdateAccountProfileFlow() {
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, sAccountProfile, mPersonalDataManager),
                        UPDATE_EXISTING_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity
                        .getString(R.string.autofill_delete_account_address_record_type_notice)
                        .replace("$1", USER_EMAIL);
        final String recordTypeNotice =
                mActivity
                        .getString(
                                R.string
                                        .autofill_address_already_saved_in_account_record_type_notice)
                        .replace("$1", USER_EMAIL);

        checkModelHasExpectedValues(
                mAddressEditor.getEditorModelForTesting(),
                deleteTitle,
                deleteText,
                recordTypeNotice);
    }

    @Test
    @SmallTest
    public void validateDefaultFields() {
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, sLocalProfile, mPersonalDataManager),
                        SAVE_NEW_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        assertNotNull(mAddressEditor.getEditorModelForTesting());
        ListModel<FieldItem> editorFields =
                mAddressEditor.getEditorModelForTesting().get(EDITOR_FIELDS);
        // Following values are set regardless of the UI components list
        // received from backend:
        // editorFields[0] - country dropdown.
        // editorFields[1] - phone field.
        // editorFields[2] - email field.
        assertEquals(3, editorFields.size());
    }

    @Test
    @SmallTest
    public void validateShownFields_NewAddressProfile() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity, mDelegate, mProfile, /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);

        mAddressEditor.showEditorDialog();
        validateShownFields(
                mAddressEditor.getEditorModelForTesting(),
                AutofillProfile.builder().build(),
                /* shouldMarkFieldsRequired= */ false);
    }

    @Test
    @SmallTest
    public void validateShownFields_NewAddressProfile_EligibleForAddressAccountStorage() {
        when(mPersonalDataManager.isEligibleForAddressAccountStorage()).thenReturn(true);
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity, mDelegate, mProfile, /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);

        mAddressEditor.showEditorDialog();
        validateShownFields(
                mAddressEditor.getEditorModelForTesting(),
                AutofillProfile.builder().build(),
                /* shouldMarkFieldsRequired= */ true,
                /* shouldMarkFieldsRequiredWhenAddressFieldEmpty= */ true);
    }

    @Test
    @SmallTest
    public void validateShownFields_LocalOrSyncAddressProfile_SaveLocally() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, sLocalProfile, mPersonalDataManager),
                        SAVE_NEW_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);

        mAddressEditor.showEditorDialog();
        validateShownFields(
                mAddressEditor.getEditorModelForTesting(),
                sLocalProfile,
                /* shouldMarkFieldsRequired= */ false);
    }

    @Test
    @SmallTest
    public void validateShownFields_LocalOrSyncAddressProfile_UpdateLocally() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, sLocalProfile, mPersonalDataManager),
                        UPDATE_EXISTING_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);

        mAddressEditor.showEditorDialog();
        validateShownFields(
                mAddressEditor.getEditorModelForTesting(),
                sLocalProfile,
                /* shouldMarkFieldsRequired= */ false);
    }

    @Test
    @SmallTest
    public void validateShownFields_LocalOrSyncAddressProfile_MigrationToAccount() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, sLocalProfile, mPersonalDataManager),
                        MIGRATE_EXISTING_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);

        mAddressEditor.showEditorDialog();
        validateShownFields(
                mAddressEditor.getEditorModelForTesting(),
                sLocalProfile,
                /* shouldMarkFieldsRequired= */ true);
    }

    @Test
    @SmallTest
    public void validateShownFields_AccountProfile_SaveInAccountFlow() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, sAccountProfile, mPersonalDataManager),
                        SAVE_NEW_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);

        mAddressEditor.showEditorDialog();
        validateShownFields(
                mAddressEditor.getEditorModelForTesting(),
                sAccountProfile,
                /* shouldMarkFieldsRequired= */ true);
    }

    @Test
    @SmallTest
    public void validateShownFields_AccountProfile_UpdateAlreadySaved() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, sAccountProfile, mPersonalDataManager),
                        UPDATE_EXISTING_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);

        mAddressEditor.showEditorDialog();
        validateShownFields(
                mAddressEditor.getEditorModelForTesting(),
                sAccountProfile,
                /* shouldMarkFieldsRequired= */ true);
    }

    @Test
    @SmallTest
    public void edit_ChangeCountry_FieldsSetChanges() {
        setUpAddressUiComponents(
                List.of(
                        new AutofillAddressUiComponent(
                                FieldType.ADDRESS_HOME_SORTING_CODE,
                                "sorting code label",
                                false,
                                true)),
                "US");
        setUpAddressUiComponents(
                List.of(
                        new AutofillAddressUiComponent(
                                FieldType.ADDRESS_HOME_STREET_ADDRESS,
                                "street address label",
                                true,
                                true)),
                "DE");
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, sLocalProfile, mPersonalDataManager),
                        SAVE_NEW_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        assertNotNull(mAddressEditor.getEditorModelForTesting());
        ListModel<FieldItem> editorFields =
                mAddressEditor.getEditorModelForTesting().get(EDITOR_FIELDS);

        // editorFields[0] - country dropdown.
        // editorFields[1] - sorting code field.
        // editorFields[2] - phone number field.
        // editorFields[3] - email field.
        assertEquals(4, editorFields.size());
        assertThat(
                StreamSupport.stream(
                                Spliterators.spliteratorUnknownSize(
                                        editorFields.iterator(), Spliterator.ORDERED),
                                false)
                        .skip(1)
                        .map(item -> item.model.get(TEXT_FIELD_TYPE))
                        .collect(Collectors.toList()),
                containsInAnyOrder(
                        FieldType.ADDRESS_HOME_SORTING_CODE,
                        FieldType.PHONE_HOME_WHOLE_NUMBER,
                        FieldType.EMAIL_ADDRESS));
        PropertyModel countryDropdown = editorFields.get(0).model;

        setDropdownKey(countryDropdown, "DE");
        ListModel<FieldItem> editorFieldsGermany =
                mAddressEditor.getEditorModelForTesting().get(EDITOR_FIELDS);
        // editorFields[0] - country dropdown.
        // editorFields[1] - street address field.
        // editorFields[2] - phone number field.
        // editorFields[3] - email field.
        assertEquals(4, editorFieldsGermany.size());
        assertThat(
                StreamSupport.stream(
                                Spliterators.spliteratorUnknownSize(
                                        editorFieldsGermany.iterator(), Spliterator.ORDERED),
                                false)
                        .skip(1)
                        .map(item -> item.model.get(TEXT_FIELD_TYPE))
                        .collect(Collectors.toList()),
                containsInAnyOrder(
                        FieldType.ADDRESS_HOME_STREET_ADDRESS,
                        FieldType.PHONE_HOME_WHOLE_NUMBER,
                        FieldType.EMAIL_ADDRESS));
    }

    @Test
    @SmallTest
    public void edit_NewAddressProfile_EligibleForAddressAccountStorage() {
        when(mPersonalDataManager.isEligibleForAddressAccountStorage()).thenReturn(true);
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity, mDelegate, mProfile, /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        PropertyModel editorModel = mAddressEditor.getEditorModelForTesting();
        assertNotNull(editorModel);
        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(11, editorFields.size());

        // Set values of the required fields.
        editorFields.get(1).model.set(VALUE, "New Name");
        editorFields.get(3).model.set(VALUE, "Locality");
        editorFields.get(4).model.set(VALUE, "Dependent locality");
        editorFields.get(7).model.set(VALUE, "Postal code");
        editorFields.get(8).model.set(VALUE, "Street address");
        editorModel.get(DONE_RUNNABLE).run();

        verify(mDelegate, times(1)).onDone(mAddressCapture.capture());
        verify(mDelegate, times(0)).onCancel();
        AutofillAddress address = mAddressCapture.getValue();
        assertNotNull(address);
        assertEquals(RecordType.ACCOUNT, address.getProfile().getRecordType());
    }

    @Test
    @SmallTest
    public void edit_AlterAddressProfile_Cancel() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(
                                mActivity,
                                new AutofillProfile(sLocalProfile),
                                mPersonalDataManager),
                        SAVE_NEW_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        PropertyModel editorModel = mAddressEditor.getEditorModelForTesting();
        assertNotNull(editorModel);
        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(11, editorFields.size());

        // Verify behaviour only on the relevant subset of fields.
        editorFields.get(1).model.set(VALUE, "New Name");
        editorFields.get(2).model.set(VALUE, "New admin area");
        editorModel.get(CANCEL_RUNNABLE).run();

        verify(mDelegate, times(0)).onDone(any());
        verify(mDelegate, times(1)).onCancel();
    }

    @Test
    @SmallTest
    public void edit_AlterAddressProfile_CommitChanges() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(
                                mActivity,
                                new AutofillProfile(sLocalProfile),
                                mPersonalDataManager),
                        SAVE_NEW_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        assertNotNull(mAddressEditor.getEditorModelForTesting());
        PropertyModel editorModel = mAddressEditor.getEditorModelForTesting();
        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(11, editorFields.size());

        // Verify behaviour only on the relevant subset of fields.
        editorFields.get(3).model.set(VALUE, "New locality");
        editorFields.get(4).model.set(VALUE, "New dependent locality");
        editorFields.get(5).model.set(VALUE, "New organization");
        editorModel.get(DONE_RUNNABLE).run();

        verify(mDelegate, times(1)).onDone(mAddressCapture.capture());
        verify(mDelegate, times(0)).onCancel();
        AutofillAddress address = mAddressCapture.getValue();
        assertNotNull(address);
        assertEquals("New locality", address.getProfile().getLocality());
        assertEquals("New dependent locality", address.getProfile().getDependentLocality());
        assertEquals("New organization", address.getProfile().getCompanyName());
    }

    @Test
    @SmallTest
    public void edit_AlterAddressProfile_CommitChanges_InvisibleFieldsNotReset() {
        // Whitelist only full name, admin area and locality.
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS.subList(0, 3));
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(
                                mActivity,
                                new AutofillProfile(sLocalProfile),
                                mPersonalDataManager),
                        SAVE_NEW_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);

        mAddressEditor.showEditorDialog();

        PropertyModel editorModel = mAddressEditor.getEditorModelForTesting();
        assertNotNull(editorModel);
        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        // editorFields[0] - country dropdown.
        // editorFields[1] - full name field.
        // editorFields[2] - admin area field.
        // editorFields[3] - locality field.
        // editorFields[4] - phone number field.
        // editorFields[5] - email field.
        assertEquals(6, editorFields.size());

        editorModel.get(DONE_RUNNABLE).run();
        verify(mDelegate, times(1)).onDone(mAddressCapture.capture());
        verify(mDelegate, times(0)).onCancel();

        AutofillAddress address = mAddressCapture.getValue();
        assertNotNull(address);
        AutofillProfile profile = address.getProfile();
        assertEquals(profile.getStreetAddress(), "111 First St");
        assertEquals(profile.getDependentLocality(), "");
        assertEquals(profile.getCompanyName(), "Google");
        assertEquals(profile.getPostalCode(), "90291");
        assertEquals(profile.getSortingCode(), "");
    }

    @Test
    @SmallTest
    public void accountSavingDisallowedForUnsupportedCountry() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS, "US");
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS, "CU");
        when(mPersonalDataManager.isEligibleForAddressAccountStorage()).thenReturn(true);
        when(mPersonalDataManager.isCountryEligibleForAccountStorage(eq("CU"))).thenReturn(false);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity, mDelegate, mProfile, /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        PropertyModel editorModel = mAddressEditor.getEditorModelForTesting();
        assertNotNull(editorModel);
        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(11, editorFields.size());

        PropertyModel countryDropdown = editorFields.get(0).model;
        setDropdownKey(countryDropdown, "CU");

        // Set values of the required fields.
        editorFields.get(1).model.set(VALUE, "New Name");
        editorFields.get(3).model.set(VALUE, "Locality");
        editorFields.get(4).model.set(VALUE, "Dependent locality");
        editorFields.get(7).model.set(VALUE, "Postal code");
        editorFields.get(8).model.set(VALUE, "Street address");
        editorModel.get(DONE_RUNNABLE).run();

        verify(mDelegate, times(1)).onDone(mAddressCapture.capture());
        verify(mDelegate, times(0)).onCancel();
        AutofillAddress address = mAddressCapture.getValue();
        assertNotNull(address);
        assertEquals(RecordType.LOCAL_OR_SYNCABLE, address.getProfile().getRecordType());
    }

    @Test
    @SmallTest
    public void countryDropDownExcludesUnsupportedCountries_saveInAccountFlow() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        when(mPersonalDataManager.isCountryEligibleForAccountStorage(eq("CU"))).thenReturn(false);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(
                                mActivity,
                                new AutofillProfile(sAccountProfile),
                                mPersonalDataManager),
                        SAVE_NEW_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        PropertyModel editorModel = mAddressEditor.getEditorModelForTesting();
        assertNotNull(editorModel);
        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(11, editorFields.size());

        assertThat(
                editorFields.get(0).model.get(DROPDOWN_KEY_VALUE_LIST).stream()
                        .map(DropdownKeyValue::getKey)
                        .collect(Collectors.toList()),
                containsInAnyOrder("US", "DE"));
    }

    @Test
    @SmallTest
    public void countryDropDownExcludesUnsupportedCountries_MigrationFlow() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        when(mPersonalDataManager.isCountryEligibleForAccountStorage(eq("CU"))).thenReturn(false);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(
                                mActivity,
                                new AutofillProfile(sLocalProfile),
                                mPersonalDataManager),
                        MIGRATE_EXISTING_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        PropertyModel editorModel = mAddressEditor.getEditorModelForTesting();
        assertNotNull(editorModel);
        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(11, editorFields.size());

        assertThat(
                editorFields.get(0).model.get(DROPDOWN_KEY_VALUE_LIST).stream()
                        .map(DropdownKeyValue::getKey)
                        .collect(Collectors.toList()),
                containsInAnyOrder("US", "DE"));
    }

    @Test
    @SmallTest
    public void countryDropDownExcludesUnsupportedCountries_editExistingAccountProfile() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        when(mPersonalDataManager.isCountryEligibleForAccountStorage(eq("CU"))).thenReturn(false);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(
                                mActivity,
                                new AutofillProfile(sAccountProfile),
                                mPersonalDataManager),
                        UPDATE_EXISTING_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        PropertyModel editorModel = mAddressEditor.getEditorModelForTesting();
        assertNotNull(editorModel);
        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(11, editorFields.size());

        assertThat(
                editorFields.get(0).model.get(DROPDOWN_KEY_VALUE_LIST).stream()
                        .map(DropdownKeyValue::getKey)
                        .collect(Collectors.toList()),
                containsInAnyOrder("US", "DE"));
    }

    @Test
    @SmallTest
    public void edit_NewAddressProfile_NoInitialValidation() {
        when(mPersonalDataManager.isEligibleForAddressAccountStorage()).thenReturn(true);
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity, mDelegate, mProfile, /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        validateErrorMessages(
                mAddressEditor.getEditorModelForTesting(), /* errorsPresent= */ false);
    }

    @Test
    @SmallTest
    public void edit_NewAddressProfile_FieldsAreValidatedAfterSave() {
        when(mPersonalDataManager.isEligibleForAddressAccountStorage()).thenReturn(true);
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity, mDelegate, mProfile, /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        PropertyModel editorModel = mAddressEditor.getEditorModelForTesting();
        assertNotNull(editorModel);
        editorModel.get(DONE_RUNNABLE).run();
        validateErrorMessages(mAddressEditor.getEditorModelForTesting(), /* errorsPresent= */ true);
    }

    @Test
    @SmallTest
    public void edit_AccountAddressProfile_FieldsAreImmediatelyValidated() {
        AutofillProfile accountProfile = new AutofillProfile(sAccountProfile);
        accountProfile.setInfo(FieldType.NAME_FULL, "");
        accountProfile.setInfo(FieldType.ADDRESS_HOME_CITY, "");
        accountProfile.setInfo(FieldType.ADDRESS_HOME_ZIP, "");

        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, accountProfile, mPersonalDataManager),
                        UPDATE_EXISTING_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        validateErrorMessages(mAddressEditor.getEditorModelForTesting(), /* errorsPresent= */ true);
    }

    @Test
    @SmallTest
    public void edit_AccountAddressProfile_FieldsAreValidatedAfterSave() {
        AutofillProfile accountProfile = new AutofillProfile(sAccountProfile);
        accountProfile.setInfo(FieldType.NAME_FULL, "");
        accountProfile.setInfo(FieldType.ADDRESS_HOME_CITY, "");
        accountProfile.setInfo(FieldType.ADDRESS_HOME_ZIP, "");

        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, accountProfile, mPersonalDataManager),
                        UPDATE_EXISTING_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        PropertyModel editorModel = mAddressEditor.getEditorModelForTesting();
        assertNotNull(editorModel);
        editorModel.get(DONE_RUNNABLE).run();
        validateErrorMessages(mAddressEditor.getEditorModelForTesting(), /* errorsPresent= */ true);
    }

    @Test
    @SmallTest
    public void edit_AccountAddressProfile_EmptyFieldsAreValidatedAfterSave() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);

        AutofillProfile accountProfile = new AutofillProfile(sAccountProfile);
        mAddressEditor =
                new AddressEditorCoordinator(
                        mActivity,
                        mDelegate,
                        mProfile,
                        new AutofillAddress(mActivity, accountProfile, mPersonalDataManager),
                        UPDATE_EXISTING_ADDRESS_PROFILE,
                        /* saveToDisk= */ false);
        mAddressEditor.setEditorDialogForTesting(mEditorDialog);
        mAddressEditor.showEditorDialog();

        PropertyModel editorModel = mAddressEditor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<FieldItem> model = editorModel.get(EDITOR_FIELDS);
        assertEquals(11, model.size());
        for (FieldItem item : model) {
            if (item.model.get(IS_REQUIRED)) {
                item.model.set(VALUE, "");
            }
        }

        editorModel.get(DONE_RUNNABLE).run();
        validateErrorMessages(mAddressEditor.getEditorModelForTesting(), /* errorsPresent= */ true);
    }
}

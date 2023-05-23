// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.isEmptyString;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.Source;
import org.chromium.chrome.browser.autofill.prefeditor.EditorDialog;
import org.chromium.chrome.browser.autofill.prefeditor.EditorModel;
import org.chromium.chrome.browser.autofill.settings.AutofillProfileBridge.AddressField;
import org.chromium.chrome.browser.autofill.settings.AutofillProfileBridge.AddressUiComponent;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.autofill.prefeditor.EditorFieldModel;
import org.chromium.components.autofill.prefeditor.EditorFieldModel.DropdownKeyValue;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.stream.Collectors;

/** Unit tests for {@link AddressEditor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.AUTOFILL_ADDRESS_PROFILE_SAVE_PROMPT_NICKNAME_SUPPORT,
        ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HONORIFIC_PREFIXES})
public class AddressEditorTest {
    private static final String USER_EMAIL = "example@gmail.com";
    private static final Locale DEFAULT_LOCALE = Locale.getDefault();
    private static final List<AddressUiComponent> SUPPORTED_ADDRESS_FIELDS = List.of(
            new AddressUiComponent(AddressField.RECIPIENT, "full name label", true, true),
            new AddressUiComponent(AddressField.ADMIN_AREA, "admin area label", false, true),
            new AddressUiComponent(AddressField.LOCALITY, "locality label", true, false),
            new AddressUiComponent(
                    AddressField.DEPENDENT_LOCALITY, "dependent locality label", true, false),
            new AddressUiComponent(AddressField.ORGANIZATION, "organization label", false, true),
            new AddressUiComponent(AddressField.SORTING_CODE, "sorting code label", false, false),
            new AddressUiComponent(AddressField.POSTAL_CODE, "postal code label", true, false),
            new AddressUiComponent(
                    AddressField.STREET_ADDRESS, "street address label", true, true));

    private static final AutofillProfile sLocalProfile = AutofillProfile.builder()
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
                    .setSource(Source.ACCOUNT)
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

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private AutofillProfileBridge.Natives mAutofillProfileBridgeJni;

    @Mock
    private EditorDialog mEditorDialog;
    @Mock
    private SyncService mSyncService;
    @Mock
    private IdentityServicesProvider mIdentityServicesProvider;
    @Mock
    private IdentityManager mIdentityManager;
    @Mock
    private PersonalDataManager mPersonalDataManager;
    @Mock
    private Profile mProfile;
    @Mock
    private AddressEditor.Delegate mDelegate;

    @Captor
    private ArgumentCaptor<EditorModel> mEditorModelCapture;
    @Captor
    private ArgumentCaptor<AutofillAddress> mAddressCapture;

    private final CoreAccountInfo mAccountInfo =
            CoreAccountInfo.createFromEmailAndGaiaId(USER_EMAIL, "gaia_id");
    // Note: can't initialize this list statically because of how Robolectric
    // initializes Android library dependencies.
    private final List<DropdownKeyValue> mSupportedCountries =
            List.of(new DropdownKeyValue("US", "United States"),
                    new DropdownKeyValue("DE", "Germany"), new DropdownKeyValue("CU", "Cuba"));

    @Nullable
    private AutofillAddress mEditedAutofillAddress;

    private Activity mActivity;
    private AddressEditor mAddressEditor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Locale.setDefault(Locale.US);

        mJniMocker.mock(AutofillProfileBridgeJni.TEST_HOOKS, mAutofillProfileBridgeJni);
        doAnswer(invocation -> {
            List<Integer> requiredFields = (List<Integer>) invocation.getArguments()[1];
            requiredFields.addAll(List.of(AddressField.RECIPIENT, AddressField.LOCALITY,
                    AddressField.DEPENDENT_LOCALITY, AddressField.POSTAL_CODE));
            return null;
        })
                .when(mAutofillProfileBridgeJni)
                .getRequiredFields(anyString(), anyList());

        mActivity = Robolectric.setupActivity(TestActivity.class);

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(mAccountInfo);

        when(mSyncService.isSyncFeatureEnabled()).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(new HashSet());
        SyncService.overrideForTests(mSyncService);

        when(mPersonalDataManager.isCountryEligibleForAccountStorage(anyString())).thenReturn(true);
        PersonalDataManager.setInstanceForTesting(mPersonalDataManager);

        setUpSupportedCountries(mSupportedCountries);

        when(mEditorDialog.getContext()).thenReturn(mActivity);
        when(mEditorDialog.getProfile()).thenReturn(mProfile);
        doNothing().when(mEditorDialog).show(mEditorModelCapture.capture());
    }

    @After
    public void tearDown() {
        // Reset default locale to avoid changing it for other tests.
        Locale.setDefault(DEFAULT_LOCALE);
        PersonalDataManager.setInstanceForTesting(null);
    }

    private void setUpSupportedCountries(List<DropdownKeyValue> supportedCountries) {
        doAnswer(invocation -> {
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
            List<AddressUiComponent> addressUiComponents, String countryCode) {
        doAnswer(invocation -> {
            List<Integer> componentIds = (List<Integer>) invocation.getArguments()[3];
            List<String> componentNames = (List<String>) invocation.getArguments()[4];
            List<Integer> componentRequired = (List<Integer>) invocation.getArguments()[5];
            List<Integer> componentLength = (List<Integer>) invocation.getArguments()[6];

            for (AddressUiComponent component : addressUiComponents) {
                componentIds.add(component.id);
                componentNames.add(component.label);
                componentRequired.add(component.isRequired ? 1 : 0);
                componentLength.add(component.isFullLine ? 1 : 0);
            }
            return "EN";
        })
                .when(mAutofillProfileBridgeJni)
                .getAddressUiComponents(eq(countryCode), anyString(), anyInt(), anyList(),
                        anyList(), anyList(), anyList());
    }

    private void setUpAddressUiComponents(List<AddressUiComponent> addressUiComponents) {
        setUpAddressUiComponents(addressUiComponents, "US");
    }

    private static void validateTextField(EditorFieldModel field, String value, int inputTypeHint,
            String label, boolean isRequired, boolean isFullLine, boolean hasLengthCounter) {
        Assert.assertTrue(field.isTextField());
        Assert.assertEquals(field.getValue(), value);
        Assert.assertEquals(inputTypeHint, field.getInputTypeHint());
        Assert.assertEquals(label, field.getLabel());
        Assert.assertEquals(isRequired, field.isRequired());
        Assert.assertEquals(isFullLine, field.isFullLine());
        Assert.assertEquals(hasLengthCounter, field.hasLengthCounter());
    }

    private static void checkUiStringsHaveExpectedValues(EditorModel editorModel,
            String expectedDeleteTitle, String expectedDeleteText,
            @Nullable String expectedSourceNotice) {
        Assert.assertNotNull(editorModel);

        Assert.assertEquals(expectedDeleteTitle, editorModel.getDeleteConfirmationTitle());
        Assert.assertEquals(expectedDeleteText, editorModel.getDeleteConfirmationText());
        Assert.assertEquals(expectedSourceNotice, editorModel.getFooterMessageText());
    }

    private void validateShownFields(
            EditorModel editorModel, AutofillProfile profile, boolean shouldMarkFieldsRequired) {
        validateShownFields(editorModel, profile, shouldMarkFieldsRequired,
                /*shouldMarkFieldsRequiredWhenAddressFieldEmpty=*/false);
    }

    private void validateShownFields(EditorModel editorModel, AutofillProfile profile,
            boolean shouldMarkFieldsRequired,
            boolean shouldMarkFieldsRequiredWhenAddressFieldEmpty) {
        Assert.assertNotNull(editorModel);
        List<EditorFieldModel> editorFields = editorModel.getFields();
        // editorFields[0] - country dropdown.
        // editorFields[1] - honorific field.
        // editorFields[2] - full name field.
        // editorFields[3] - admin area field.
        // editorFields[4] - locality field.
        // editorFields[5] - dependent locality field.
        // editorFields[6] - organization field.
        // editorFields[7] - sorting code field.
        // editorFields[8] - postal code field.
        // editorFields[9] - street address field.
        // editorFields[10] - phone number field.
        // editorFields[11] - email field.
        // editorFields[12] - nickname field.
        Assert.assertEquals(13, editorFields.size());

        // Fields obtained from backend must be placed after the country dropdown.
        // Note: honorific prefix always comes before the full name field.
        validateTextField(editorFields.get(1), profile.getHonorificPrefix(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE,
                mActivity.getString(R.string.autofill_profile_editor_honorific_prefix), false, true,
                false);
        validateTextField(editorFields.get(2), profile.getFullName(),
                EditorFieldModel.INPUT_TYPE_HINT_PERSON_NAME, "full name label",
                shouldMarkFieldsRequired, true, false);
        validateTextField(editorFields.get(3), profile.getRegion(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, "admin area label", false, true, false);
        // Locality field is forced to occupy full line.
        validateTextField(editorFields.get(4), profile.getLocality(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, "locality label", shouldMarkFieldsRequired,
                true, false);

        // Note: dependent locality is a required field for address profiles stored in Google
        // account, but it's still marked as optional by the editor when the corresponding field in
        // the existing address profile is empty. It is considered required for new address
        // profiles.
        validateTextField(editorFields.get(5), profile.getDependentLocality(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, "dependent locality label",
                shouldMarkFieldsRequiredWhenAddressFieldEmpty, true, false);

        validateTextField(editorFields.get(6), profile.getCompanyName(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, "organization label", false, true, false);

        validateTextField(editorFields.get(7), profile.getSortingCode(),
                EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC, "sorting code label", false, false,
                false);
        validateTextField(editorFields.get(8), profile.getPostalCode(),
                EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC, "postal code label",
                shouldMarkFieldsRequired, false, false);
        validateTextField(editorFields.get(9), profile.getStreetAddress(),
                EditorFieldModel.INPUT_TYPE_HINT_STREET_LINES, "street address label",
                shouldMarkFieldsRequired, true, false);
    }

    @Test
    @SmallTest
    public void validateCustomDoneButtonText() {
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate, /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.setCustomDoneButtonText("Custom done");
        mAddressEditor.showEditorDialog();

        EditorModel editorModel = mEditorModelCapture.getValue();
        Assert.assertNotNull(editorModel);

        Assert.assertEquals("Custom done", editorModel.getCustomDoneButtonText());
    }

    @Test
    @SmallTest
    public void validateUIStrings_NewAddressProfile() {
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate, /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity.getString(R.string.autofill_delete_local_address_source_notice);
        final String sourceNotice = null;

        checkUiStringsHaveExpectedValues(
                mEditorModelCapture.getValue(), deleteTitle, deleteText, sourceNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_NewAddressProfile_EligibleForAddressAccountStorage() {
        when(mPersonalDataManager.isEligibleForAddressAccountStorage()).thenReturn(true);
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate, /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity.getString(R.string.autofill_delete_account_address_source_notice)
                        .replace("$1", USER_EMAIL);
        final String sourceNotice =
                mActivity
                        .getString(R.string.autofill_address_will_be_saved_in_account_source_notice)
                        .replace("$1", USER_EMAIL);

        checkUiStringsHaveExpectedValues(
                mEditorModelCapture.getValue(), deleteTitle, deleteText, sourceNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_LocalOrSyncAddressProfile_AddressSyncDisabled() {
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, sLocalProfile), /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity.getString(R.string.autofill_delete_local_address_source_notice);
        final String sourceNotice = null;

        checkUiStringsHaveExpectedValues(
                mEditorModelCapture.getValue(), deleteTitle, deleteText, sourceNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_LocalOrSyncAddressProfile_AddressSyncEnabled() {
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, sLocalProfile), /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncService.getSelectedTypes())
                .thenReturn(Collections.singleton(UserSelectableType.AUTOFILL));

        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity.getString(R.string.autofill_delete_sync_address_source_notice);
        final String sourceNotice = null;

        checkUiStringsHaveExpectedValues(
                mEditorModelCapture.getValue(), deleteTitle, deleteText, sourceNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_UpdateLocalOrSyncAddressProfile_AddressSyncDisabled() {
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, sLocalProfile), /*saveToDisk=*/false,
                /*isUpdate=*/true,
                /*isMigrationToAccount=*/false);
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity.getString(R.string.autofill_delete_local_address_source_notice);
        final String sourceNotice = null;

        checkUiStringsHaveExpectedValues(
                mEditorModelCapture.getValue(), deleteTitle, deleteText, sourceNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_UpdateLocalOrSyncAddressProfile_AddressSyncEnabled() {
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, sLocalProfile), /*saveToDisk=*/false,
                /*isUpdate=*/true,
                /*isMigrationToAccount=*/false);
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncService.getSelectedTypes())
                .thenReturn(Collections.singleton(UserSelectableType.AUTOFILL));

        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity.getString(R.string.autofill_delete_sync_address_source_notice);
        final String sourceNotice = null;

        checkUiStringsHaveExpectedValues(
                mEditorModelCapture.getValue(), deleteTitle, deleteText, sourceNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_LocalAddressProfile_MigrationToAccount() {
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, sLocalProfile), /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/true);

        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity.getString(R.string.autofill_delete_account_address_source_notice)
                        .replace("$1", USER_EMAIL);
        final String sourceNotice =
                mActivity
                        .getString(R.string.autofill_address_will_be_saved_in_account_source_notice)
                        .replace("$1", USER_EMAIL);

        checkUiStringsHaveExpectedValues(
                mEditorModelCapture.getValue(), deleteTitle, deleteText, sourceNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_SyncAddressProfile_MigrationToAccount() {
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, sLocalProfile), /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/true);
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncService.getSelectedTypes())
                .thenReturn(Collections.singleton(UserSelectableType.AUTOFILL));

        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity.getString(R.string.autofill_delete_account_address_source_notice)
                        .replace("$1", USER_EMAIL);
        final String sourceNotice =
                mActivity
                        .getString(R.string.autofill_address_will_be_saved_in_account_source_notice)
                        .replace("$1", USER_EMAIL);

        checkUiStringsHaveExpectedValues(
                mEditorModelCapture.getValue(), deleteTitle, deleteText, sourceNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_AccountAddressProfile_SaveInAccountFlow() {
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, sAccountProfile), /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity.getString(R.string.autofill_delete_account_address_source_notice)
                        .replace("$1", USER_EMAIL);
        final String sourceNotice =
                mActivity
                        .getString(R.string.autofill_address_will_be_saved_in_account_source_notice)
                        .replace("$1", USER_EMAIL);

        checkUiStringsHaveExpectedValues(
                mEditorModelCapture.getValue(), deleteTitle, deleteText, sourceNotice);
    }

    @Test
    @SmallTest
    public void validateUIStrings_AccountAddressProfile_UpdateAccountProfileFlow() {
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, sAccountProfile), /*saveToDisk=*/false,
                /*isUpdate=*/true,
                /*isMigrationToAccount=*/false);
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.showEditorDialog();

        final String deleteTitle =
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title);
        final String deleteText =
                mActivity.getString(R.string.autofill_delete_account_address_source_notice)
                        .replace("$1", USER_EMAIL);
        final String sourceNotice =
                mActivity
                        .getString(R.string.autofill_address_already_saved_in_account_source_notice)
                        .replace("$1", USER_EMAIL);

        checkUiStringsHaveExpectedValues(
                mEditorModelCapture.getValue(), deleteTitle, deleteText, sourceNotice);
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ADDRESS_PROFILE_SAVE_PROMPT_NICKNAME_SUPPORT,
            ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HONORIFIC_PREFIXES})
    public void
    validateDefaultFields_NicknamesDisabled_HonorificDisabled() {
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, sLocalProfile), /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.showEditorDialog();

        Assert.assertNotNull(mEditorModelCapture.getValue());
        List<EditorFieldModel> editorFields = mEditorModelCapture.getValue().getFields();
        // Following values are set regardless of the UI components list
        // received from backend when nicknames are disabled:
        // editorFields[0] - country dropdown.
        // editorFields[1] - phone field.
        // editorFields[2] - email field.
        Assert.assertEquals(3, editorFields.size());

        EditorFieldModel countryDropdown = editorFields.get(0);
        Assert.assertTrue(countryDropdown.isDropdownField());
        Assert.assertTrue(countryDropdown.isFullLine());
        Assert.assertEquals(
                countryDropdown.getValue(), AutofillAddress.getCountryCode(sLocalProfile));
        Assert.assertEquals(countryDropdown.getLabel(),
                mActivity.getString(R.string.autofill_profile_editor_country));
        Assert.assertEquals(
                mSupportedCountries.size(), countryDropdown.getDropdownKeyValues().size());
        assertThat(mSupportedCountries,
                containsInAnyOrder(countryDropdown.getDropdownKeyValues().toArray()));

        validateTextField(editorFields.get(1), sLocalProfile.getPhoneNumber(),
                EditorFieldModel.INPUT_TYPE_HINT_PHONE,
                mActivity.getString(R.string.autofill_profile_editor_phone_number), false, true,
                false);
        validateTextField(editorFields.get(2), sLocalProfile.getEmailAddress(),
                EditorFieldModel.INPUT_TYPE_HINT_EMAIL,
                mActivity.getString(R.string.autofill_profile_editor_email_address), false, true,
                false);
    }

    @Test
    @SmallTest
    public void validateDefaultFields() {
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, sLocalProfile), /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.showEditorDialog();

        Assert.assertNotNull(mEditorModelCapture.getValue());
        List<EditorFieldModel> editorFields = mEditorModelCapture.getValue().getFields();
        // Following values are set regardless of the UI components list
        // received from backend:
        // editorFields[0] - country dropdown.
        // editorFields[1] - phone field.
        // editorFields[2] - email field.
        // editorFields[3] - nickname field.
        Assert.assertEquals(4, editorFields.size());

        validateTextField(editorFields.get(3), null, EditorFieldModel.INPUT_TYPE_HINT_NONE, "Label",
                false, true, false);
    }

    @Test
    @SmallTest
    public void validateShownFields_NewAddressProfile() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate, /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);

        mAddressEditor.showEditorDialog();
        validateShownFields(mEditorModelCapture.getValue(), AutofillProfile.builder().build(),
                /*shouldMarkFieldsRequired=*/false);
    }

    @Test
    @SmallTest
    public void validateShownFields_NewAddressProfile_EligibleForAddressAccountStorage() {
        when(mPersonalDataManager.isEligibleForAddressAccountStorage()).thenReturn(true);
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate, /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);

        mAddressEditor.showEditorDialog();
        validateShownFields(mEditorModelCapture.getValue(), AutofillProfile.builder().build(),
                /*shouldMarkFieldsRequired=*/true,
                /*shouldMarkFieldsRequiredWhenAddressFieldEmpty=*/true);
    }

    @Test
    @SmallTest
    public void validateShownFields_LocalOrSyncAddressProfile_SaveLocally() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, sLocalProfile), /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);

        mAddressEditor.showEditorDialog();
        validateShownFields(
                mEditorModelCapture.getValue(), sLocalProfile, /*shouldMarkFieldsRequired=*/false);
    }

    @Test
    @SmallTest
    public void validateShownFields_LocalOrSyncAddressProfile_UpdateLocally() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, sLocalProfile), /*saveToDisk=*/false,
                /*isUpdate=*/true,
                /*isMigrationToAccount=*/false);

        mAddressEditor.showEditorDialog();
        validateShownFields(
                mEditorModelCapture.getValue(), sLocalProfile, /*shouldMarkFieldsRequired=*/false);
    }

    @Test
    @SmallTest
    public void validateShownFields_LocalOrSyncAddressProfile_MigrationToAccount() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, sLocalProfile), /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/true);

        mAddressEditor.showEditorDialog();
        validateShownFields(
                mEditorModelCapture.getValue(), sLocalProfile, /*shouldMarkFieldsRequired=*/true);
    }

    @Test
    @SmallTest
    public void validateShownFields_AccountProfile_SaveInAccountFlow() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, sAccountProfile), /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);

        mAddressEditor.showEditorDialog();
        validateShownFields(
                mEditorModelCapture.getValue(), sAccountProfile, /*shouldMarkFieldsRequired=*/true);
    }

    @Test
    @SmallTest
    public void validateShownFields_AccountProfile_UpdateAlreadySaved() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, sAccountProfile), /*saveToDisk=*/false,
                /*isUpdate=*/true,
                /*isMigrationToAccount=*/false);

        mAddressEditor.showEditorDialog();
        validateShownFields(
                mEditorModelCapture.getValue(), sAccountProfile, /*shouldMarkFieldsRequired=*/true);
    }

    @Test
    @SmallTest
    public void edit_ChangeCountry_FieldsSetChanges() {
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, sLocalProfile), /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);
        setUpAddressUiComponents(List.of(new AddressUiComponent(AddressField.SORTING_CODE,
                                         "sorting code label", false, true)),
                "US");
        setUpAddressUiComponents(List.of(new AddressUiComponent(AddressField.STREET_ADDRESS,
                                         "street address label", true, true)),
                "DE");
        mAddressEditor.showEditorDialog();

        Assert.assertNotNull(mEditorModelCapture.getValue());
        List<EditorFieldModel> editorFields = mEditorModelCapture.getValue().getFields();

        // editorFields[0] - country dropdown.
        // editorFields[1] - sorting code field.
        // editorFields[2] - phone number field.
        // editorFields[3] - email field.
        // editorFields[4] - nickname field.
        Assert.assertEquals(5, editorFields.size());
        assertThat(editorFields.stream()
                           .map(EditorFieldModel::getInputTypeHint)
                           .collect(Collectors.toList()),
                containsInAnyOrder(EditorFieldModel.INPUT_TYPE_HINT_DROPDOWN,
                        EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC,
                        EditorFieldModel.INPUT_TYPE_HINT_PHONE,
                        EditorFieldModel.INPUT_TYPE_HINT_EMAIL,
                        EditorFieldModel.INPUT_TYPE_HINT_NONE));
        EditorFieldModel countryDropdown = editorFields.get(0);

        countryDropdown.setDropdownKey("DE", () -> {});
        // editorFields[0] - country dropdown.
        // editorFields[1] - street address field.
        // editorFields[2] - phone number field.
        // editorFields[3] - email field.
        // editorFields[4] - nickname field.
        Assert.assertEquals(5, editorFields.size());
        assertThat(editorFields.stream()
                           .map(EditorFieldModel::getInputTypeHint)
                           .collect(Collectors.toList()),
                containsInAnyOrder(EditorFieldModel.INPUT_TYPE_HINT_DROPDOWN,
                        EditorFieldModel.INPUT_TYPE_HINT_STREET_LINES,
                        EditorFieldModel.INPUT_TYPE_HINT_PHONE,
                        EditorFieldModel.INPUT_TYPE_HINT_EMAIL,
                        EditorFieldModel.INPUT_TYPE_HINT_NONE));
    }

    @Test
    @SmallTest
    public void edit_NewAddressProfile_EligibleForAddressAccountStorage() {
        when(mPersonalDataManager.isEligibleForAddressAccountStorage()).thenReturn(true);
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate, /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor.showEditorDialog();

        EditorModel editorModel = mEditorModelCapture.getValue();
        Assert.assertNotNull(editorModel);
        List<EditorFieldModel> editorFields = editorModel.getFields();
        Assert.assertEquals(13, editorFields.size());

        // Set values of the required fields.
        editorFields.get(2).setValue("New Name");
        editorFields.get(4).setValue("Locality");
        editorFields.get(5).setValue("Dependent locality");
        editorFields.get(8).setValue("Postal code");
        editorFields.get(9).setValue("Street address");
        editorModel.done();

        verify(mDelegate, times(1)).onDone(mAddressCapture.capture());
        verify(mDelegate, times(0)).onCancel();
        AutofillAddress address = mAddressCapture.getValue();
        Assert.assertNotNull(address);
        Assert.assertEquals(Source.ACCOUNT, address.getProfile().getSource());
    }

    @Test
    @SmallTest
    public void edit_AlterAddressProfile_Cancel() {
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, new AutofillProfile(sLocalProfile)),
                /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor.showEditorDialog();

        EditorModel editorModel = mEditorModelCapture.getValue();
        Assert.assertNotNull(editorModel);
        List<EditorFieldModel> editorFields = editorModel.getFields();
        Assert.assertEquals(13, editorFields.size());

        // Verify behaviour only on the relevant subset of fields.
        editorFields.get(1).setValue("New honorific prefix");
        editorFields.get(2).setValue("New Name");
        editorFields.get(3).setValue("New admin area");
        editorModel.cancel();

        verify(mDelegate, times(0)).onDone(any());
        verify(mDelegate, times(1)).onCancel();
    }

    @Test
    @SmallTest
    public void edit_AlterAddressProfile_CommitChanges() {
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, new AutofillProfile(sLocalProfile)),
                /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor.showEditorDialog();

        Assert.assertNotNull(mEditorModelCapture.getValue());
        EditorModel editorModel = mEditorModelCapture.getValue();
        List<EditorFieldModel> editorFields = editorModel.getFields();
        Assert.assertEquals(13, editorFields.size());

        // Verify behaviour only on the relevant subset of fields.
        editorFields.get(4).setValue("New locality");
        editorFields.get(5).setValue("New dependent locality");
        editorFields.get(6).setValue("New organization");
        editorModel.done();

        verify(mDelegate, times(1)).onDone(mAddressCapture.capture());
        verify(mDelegate, times(0)).onCancel();
        AutofillAddress address = mAddressCapture.getValue();
        Assert.assertNotNull(address);
        Assert.assertEquals("New locality", address.getProfile().getLocality());
        Assert.assertEquals("New dependent locality", address.getProfile().getDependentLocality());
        Assert.assertEquals("New organization", address.getProfile().getCompanyName());
    }

    @Test
    @SmallTest
    public void edit_AlterAddressProfile_CommitChanges_InvisibleFieldsGetReset() {
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, new AutofillProfile(sLocalProfile)),
                /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);

        // Whitelist only full name, admin area and locality.
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS.subList(0, 3));
        mAddressEditor.showEditorDialog();

        EditorModel editorModel = mEditorModelCapture.getValue();
        Assert.assertNotNull(editorModel);
        List<EditorFieldModel> editorFields = editorModel.getFields();
        // editorFields[0] - country dropdown.
        // editorFields[1] - honorific prefix field.
        // editorFields[2] - full name field.
        // editorFields[3] - admin area field.
        // editorFields[4] - locality field.
        // editorFields[5] - phone number field.
        // editorFields[6] - email field.
        // editorFields[7] - nickname field.
        Assert.assertEquals(8, editorFields.size());

        editorModel.done();
        verify(mDelegate, times(1)).onDone(mAddressCapture.capture());
        verify(mDelegate, times(0)).onCancel();

        AutofillAddress address = mAddressCapture.getValue();
        Assert.assertNotNull(address);
        AutofillProfile profile = address.getProfile();
        assertThat(profile.getStreetAddress(), isEmptyString());
        assertThat(profile.getDependentLocality(), isEmptyString());
        assertThat(profile.getCompanyName(), isEmptyString());
        assertThat(profile.getPostalCode(), isEmptyString());
        assertThat(profile.getSortingCode(), isEmptyString());
    }

    @Test
    @SmallTest
    public void accountSavingDisallowedForUnsupportedCountry() {
        when(mPersonalDataManager.isEligibleForAddressAccountStorage()).thenReturn(true);
        when(mPersonalDataManager.isCountryEligibleForAccountStorage(eq("CU"))).thenReturn(false);
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate, /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS, "US");
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS, "CU");
        mAddressEditor.showEditorDialog();

        EditorModel editorModel = mEditorModelCapture.getValue();
        Assert.assertNotNull(editorModel);
        List<EditorFieldModel> editorFields = editorModel.getFields();
        Assert.assertEquals(13, editorFields.size());

        EditorFieldModel countryDropdown = editorFields.get(0);
        countryDropdown.setDropdownKey("CU", () -> {});

        // Set values of the required fields.
        editorFields.get(2).setValue("New Name");
        editorFields.get(4).setValue("Locality");
        editorFields.get(5).setValue("Dependent locality");
        editorFields.get(8).setValue("Postal code");
        editorFields.get(9).setValue("Street address");
        editorModel.done();

        verify(mDelegate, times(1)).onDone(mAddressCapture.capture());
        verify(mDelegate, times(0)).onCancel();
        AutofillAddress address = mAddressCapture.getValue();
        Assert.assertNotNull(address);
        Assert.assertEquals(Source.LOCAL_OR_SYNCABLE, address.getProfile().getSource());
    }

    @Test
    @SmallTest
    public void countryDropDownExcludesUnsupportedCountries_saveInAccountFlow() {
        when(mPersonalDataManager.isCountryEligibleForAccountStorage(eq("CU"))).thenReturn(false);
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, new AutofillProfile(sAccountProfile)),
                /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/false);
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor.showEditorDialog();

        EditorModel editorModel = mEditorModelCapture.getValue();
        Assert.assertNotNull(editorModel);
        List<EditorFieldModel> editorFields = editorModel.getFields();
        Assert.assertEquals(13, editorFields.size());

        assertThat(editorFields.get(0).getDropdownKeys(), containsInAnyOrder("US", "DE"));
    }

    @Test
    @SmallTest
    public void countryDropDownExcludesUnsupportedCountries_MigrationFlow() {
        when(mPersonalDataManager.isCountryEligibleForAccountStorage(eq("CU"))).thenReturn(false);
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, new AutofillProfile(sLocalProfile)),
                /*saveToDisk=*/false,
                /*isUpdate=*/false,
                /*isMigrationToAccount=*/true);
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor.showEditorDialog();

        EditorModel editorModel = mEditorModelCapture.getValue();
        Assert.assertNotNull(editorModel);
        List<EditorFieldModel> editorFields = editorModel.getFields();
        Assert.assertEquals(13, editorFields.size());

        assertThat(editorFields.get(0).getDropdownKeys(), containsInAnyOrder("US", "DE"));
    }

    @Test
    @SmallTest
    public void countryDropDownExcludesUnsupportedCountries_editExistingAccountProfile() {
        when(mPersonalDataManager.isCountryEligibleForAccountStorage(eq("CU"))).thenReturn(false);
        mAddressEditor = new AddressEditor(mEditorDialog, mDelegate,
                new AutofillAddress(mActivity, new AutofillProfile(sAccountProfile)),
                /*saveToDisk=*/false, /*isUpdate=*/true,
                /*isMigrationToAccount=*/false);
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS);
        mAddressEditor.showEditorDialog();

        EditorModel editorModel = mEditorModelCapture.getValue();
        Assert.assertNotNull(editorModel);
        List<EditorFieldModel> editorFields = editorModel.getFields();
        Assert.assertEquals(13, editorFields.size());

        assertThat(editorFields.get(0).getDropdownKeys(), containsInAnyOrder("US", "DE"));
    }
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
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

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
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
    private static final Locale DEFAULT_LOCALTE = Locale.getDefault();
    private static final List<AddressUiComponent> sSupportedAddressFields = List.of(
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

    private static final AutofillProfile sLocalProfile = new AutofillProfile("",
            "https://example.com", true, Source.LOCAL_OR_SYNCABLE, "" /* honorific prefix */,
            "Seb Doe", "Google", "111 First St", "CA", "Los Angeles", "", "90291", "", "US",
            "650-253-0000", "first@gmail.com", "en-US");
    private static final AutofillProfile sAccountProfile = new AutofillProfile("",
            "https://example.com", true, Source.ACCOUNT, "" /* honorific prefix */, "Seb Doe",
            "Google", "111 First St", "CA", "Los Angeles", "", "90291", "", "US", "650-253-0000",
            "first@gmail.com", "en-US");

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
    private Profile mProfile;

    @Captor
    private ArgumentCaptor<EditorModel> mEditorModelCapture;

    private final CoreAccountInfo mAccountInfo =
            CoreAccountInfo.createFromEmailAndGaiaId(USER_EMAIL, "gaia_id");
    // Note: can't initialize this list statically because of how Robolectric
    // initializes Android library dependencies.
    private final List<DropdownKeyValue> mSupportedCountries = List.of(
            new DropdownKeyValue("US", "United States"), new DropdownKeyValue("DE", "Germany"));

    private Callback<AutofillAddress> mDoneCallback;
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
        mDoneCallback = address -> {
            mEditedAutofillAddress = address;
        };

        Profile.setLastUsedProfileForTesting(mProfile);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(mAccountInfo);

        when(mSyncService.isSyncFeatureEnabled()).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(new HashSet());
        SyncService.overrideForTests(mSyncService);

        setUpSupportedCountries(mSupportedCountries);

        when(mEditorDialog.getContext()).thenReturn(mActivity);
        doNothing().when(mEditorDialog).show(mEditorModelCapture.capture());

        mAddressEditor = new AddressEditor(/*saveToDisk=*/false);
        mAddressEditor.setEditorDialog(mEditorDialog);
    }

    @After
    public void tearDown() {
        // Reset default locale to avoid changing it for other tests.
        Locale.setDefault(DEFAULT_LOCALTE);
    }

    private void setUpSupportedCountries(List<DropdownKeyValue> supportedContries) {
        doAnswer(invocation -> {
            List<String> contryCodes = (List<String>) invocation.getArguments()[0];
            List<String> contryNames = (List<String>) invocation.getArguments()[1];

            for (DropdownKeyValue keyValue : supportedContries) {
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

    private void validateTextField(EditorFieldModel field, String value, int inputTypeHint,
            String label, boolean isRequired, boolean isFullLine, boolean hasLengthCounter) {
        Assert.assertTrue(field.isTextField());
        Assert.assertEquals(inputTypeHint, field.getInputTypeHint());
        Assert.assertEquals(label, field.getLabel());
        Assert.assertEquals(isRequired, field.isRequired());
        Assert.assertEquals(isFullLine, field.isFullLine());
        Assert.assertEquals(hasLengthCounter, field.hasLengthCounter());
    }

    @Test
    @SmallTest
    public void validateUIStrings_NewAddressProfile() {
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.setCustomDoneButtonText("Custom done");
        mAddressEditor.edit(null, unused -> { return; });

        EditorModel editorModel = mEditorModelCapture.getValue();
        Assert.assertNotNull(editorModel);

        Assert.assertEquals(
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title),
                editorModel.getDeleteConfirmationTitle());
        Assert.assertEquals("Custom done", editorModel.getCustomDoneButtonText());
        Assert.assertNull(editorModel.getFooterMessageText());
        Assert.assertEquals(
                mActivity.getString(R.string.autofill_delete_local_address_source_notice),
                editorModel.getDeleteConfirmationText());
    }

    @Test
    @SmallTest
    public void validateUIStrings_LocalOrSyncAddressProfile_AddressSyncDisabled() {
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.setCustomDoneButtonText("Custom done");
        mAddressEditor.edit(new AutofillAddress(mActivity, sLocalProfile), unused -> { return; });

        EditorModel editorModel = mEditorModelCapture.getValue();
        Assert.assertNotNull(editorModel);

        Assert.assertEquals(
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title),
                editorModel.getDeleteConfirmationTitle());
        Assert.assertEquals("Custom done", editorModel.getCustomDoneButtonText());
        Assert.assertNull(editorModel.getFooterMessageText());
        Assert.assertEquals(
                mActivity.getString(R.string.autofill_delete_local_address_source_notice),
                editorModel.getDeleteConfirmationText());
    }

    @Test
    @SmallTest
    public void validateUIStrings_LocalOrSyncAddressProfile_AddressSyncEnabled() {
        setUpAddressUiComponents(new ArrayList());
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncService.getSelectedTypes())
                .thenReturn(Collections.singleton(UserSelectableType.AUTOFILL));

        mAddressEditor.edit(new AutofillAddress(mActivity, sLocalProfile), unused -> { return; });
        EditorModel editorModel = mEditorModelCapture.getValue();
        Assert.assertNotNull(editorModel);
        Assert.assertNull(editorModel.getFooterMessageText());
        Assert.assertEquals(
                mActivity.getString(R.string.autofill_delete_sync_address_source_notice),
                editorModel.getDeleteConfirmationText());
    }

    @Test
    @SmallTest
    public void validateUIStrings_AccountAddressProfile() {
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.setCustomDoneButtonText("Custom done");
        mAddressEditor.edit(new AutofillAddress(mActivity, sAccountProfile), unused -> { return; });

        EditorModel editorModel = mEditorModelCapture.getValue();
        Assert.assertNotNull(editorModel);

        Assert.assertEquals(
                mActivity.getString(R.string.autofill_delete_address_confirmation_dialog_title),
                editorModel.getDeleteConfirmationTitle());
        Assert.assertEquals("Custom done", editorModel.getCustomDoneButtonText());
        Assert.assertEquals(
                mActivity.getString(R.string.autofill_edit_account_address_source_notice)
                        .replace("$1", USER_EMAIL),
                editorModel.getFooterMessageText());
        Assert.assertEquals(
                mActivity.getString(R.string.autofill_delete_account_address_source_notice)
                        .replace("$1", USER_EMAIL),
                editorModel.getDeleteConfirmationText());
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ADDRESS_PROFILE_SAVE_PROMPT_NICKNAME_SUPPORT,
            ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HONORIFIC_PREFIXES})
    public void
    validateDefaultFields_NicknamesDisabled_HonorificDisabled() {
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.edit(new AutofillAddress(mActivity, sLocalProfile), unused -> { return; });

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
        setUpAddressUiComponents(new ArrayList());
        mAddressEditor.edit(new AutofillAddress(mActivity, sLocalProfile), unused -> { return; });

        Assert.assertNotNull(mEditorModelCapture.getValue());
        List<EditorFieldModel> editorFields = mEditorModelCapture.getValue().getFields();
        // Following values are set regardless of the UI components list
        // received from backend:
        // editorFields[0] - country dropdown.
        // editorFields[1] - phone field.
        // editorFields[2] - email field.
        // editorFields[3] - nickname field.
        Assert.assertEquals(4, editorFields.size());

        validateTextField(editorFields.get(3), "", EditorFieldModel.INPUT_TYPE_HINT_NONE, "Label",
                false, true, false);
    }

    @Test
    @SmallTest
    public void validateShownFields_NewAddressProfile() {
        AutofillProfile emptyProfile = new AutofillProfile();
        setUpAddressUiComponents(sSupportedAddressFields);
        mAddressEditor.edit(null, unused -> { return; });

        Assert.assertNotNull(mEditorModelCapture.getValue());
        List<EditorFieldModel> editorFields = mEditorModelCapture.getValue().getFields();
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
        validateTextField(editorFields.get(1), emptyProfile.getHonorificPrefix(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE,
                mActivity.getString(R.string.autofill_profile_editor_honorific_prefix), false, true,
                false);
        validateTextField(editorFields.get(2), emptyProfile.getFullName(),
                EditorFieldModel.INPUT_TYPE_HINT_PERSON_NAME, "full name label", false, true,
                false);
        validateTextField(editorFields.get(3), emptyProfile.getRegion(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, "admin area label", false, true, false);
        // Locality field is forced to occupy full line.
        validateTextField(editorFields.get(4), emptyProfile.getLocality(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, "locality label", false, true, false);

        // Dependent locality is empty in address profile stored in account, this way it is still
        // considered optional by the editor, although it is required for newly created address
        // profiles. Dependenent locality field is forced to occupy full line.
        validateTextField(editorFields.get(5), emptyProfile.getDependentLocality(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, "dependent locality label", false, true,
                false);

        validateTextField(editorFields.get(6), emptyProfile.getSortingCode(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, "organization label", false, true, false);

        validateTextField(editorFields.get(7), emptyProfile.getCompanyName(),
                EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC, "sorting code label", false, false,
                false);
        validateTextField(editorFields.get(8), emptyProfile.getPostalCode(),
                EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC, "postal code label", false, false,
                false);
        validateTextField(editorFields.get(9), emptyProfile.getStreetAddress(),
                EditorFieldModel.INPUT_TYPE_HINT_STREET_LINES, "street address label", false, true,
                false);
    }

    @Test
    @SmallTest
    public void validateShownFields_LocalOrSyncAddressProfile() {
        setUpAddressUiComponents(sSupportedAddressFields);
        mAddressEditor.edit(new AutofillAddress(mActivity, sLocalProfile), unused -> { return; });

        Assert.assertNotNull(mEditorModelCapture.getValue());
        List<EditorFieldModel> editorFields = mEditorModelCapture.getValue().getFields();
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
        validateTextField(editorFields.get(1), sLocalProfile.getHonorificPrefix(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE,
                mActivity.getString(R.string.autofill_profile_editor_honorific_prefix), false, true,
                false);
        validateTextField(editorFields.get(2), sLocalProfile.getFullName(),
                EditorFieldModel.INPUT_TYPE_HINT_PERSON_NAME, "full name label", false, true,
                false);
        validateTextField(editorFields.get(3), sLocalProfile.getRegion(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, "admin area label", false, true, false);
        // Locality field is forced to occupy full line.
        validateTextField(editorFields.get(4), sLocalProfile.getLocality(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, "locality label", false, true, false);

        // Dependent locality is empty in address profile stored in account, this way it is still
        // considered optional by the editor, although it is required for newly created address
        // profiles. Dependenent locality field is forced to occupy full line.
        validateTextField(editorFields.get(5), sLocalProfile.getDependentLocality(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, "dependent locality label", false, true,
                false);

        validateTextField(editorFields.get(6), sLocalProfile.getSortingCode(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, "organization label", false, true, false);

        validateTextField(editorFields.get(7), sLocalProfile.getCompanyName(),
                EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC, "sorting code label", false, false,
                false);
        validateTextField(editorFields.get(8), sLocalProfile.getPostalCode(),
                EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC, "postal code label", false, false,
                false);
        validateTextField(editorFields.get(9), sLocalProfile.getStreetAddress(),
                EditorFieldModel.INPUT_TYPE_HINT_STREET_LINES, "street address label", false, true,
                false);
    }

    @Test
    @SmallTest
    public void validateShownFields() {
        setUpAddressUiComponents(sSupportedAddressFields);
        mAddressEditor.edit(new AutofillAddress(mActivity, sAccountProfile), unused -> { return; });

        Assert.assertNotNull(mEditorModelCapture.getValue());
        List<EditorFieldModel> editorFields = mEditorModelCapture.getValue().getFields();
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
        validateTextField(editorFields.get(1), sAccountProfile.getHonorificPrefix(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE,
                mActivity.getString(R.string.autofill_profile_editor_honorific_prefix), false, true,
                false);
        validateTextField(editorFields.get(2), sAccountProfile.getFullName(),
                EditorFieldModel.INPUT_TYPE_HINT_PERSON_NAME, "full name label", true, true, false);
        validateTextField(editorFields.get(3), sAccountProfile.getRegion(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, "admin area label", false, true, false);
        // Locality field is forced to occupy full line.
        validateTextField(editorFields.get(4), sAccountProfile.getLocality(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, "locality label", true, true, false);

        // Dependent locality is empty in address profile stored in account, this way it is still
        // considered optional by the editor, although it is required for newly created address
        // profiles. Dependenent locality field is forced to occupy full line.
        validateTextField(editorFields.get(5), sAccountProfile.getDependentLocality(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, "dependent locality label", false, true,
                false);

        validateTextField(editorFields.get(6), sAccountProfile.getSortingCode(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, "organization label", false, true, false);

        validateTextField(editorFields.get(7), sAccountProfile.getCompanyName(),
                EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC, "sorting code label", false, false,
                false);
        validateTextField(editorFields.get(8), sAccountProfile.getPostalCode(),
                EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC, "postal code label", true, false,
                false);
        validateTextField(editorFields.get(9), sAccountProfile.getStreetAddress(),
                EditorFieldModel.INPUT_TYPE_HINT_STREET_LINES, "street address label", true, true,
                false);
    }

    @Test
    @SmallTest
    public void edit_ChangeCountry_FieldsSetChanges() {
        setUpAddressUiComponents(List.of(new AddressUiComponent(AddressField.SORTING_CODE,
                                         "sorting code label", false, true)),
                "US");
        setUpAddressUiComponents(List.of(new AddressUiComponent(AddressField.STREET_ADDRESS,
                                         "street address label", true, true)),
                "DE");
        mAddressEditor.edit(new AutofillAddress(mActivity, sLocalProfile), unused -> { return; });

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
    public void edit_AlterProfile_Cancel() {
        AutofillProfile toEdit = new AutofillProfile(sLocalProfile);
        setUpAddressUiComponents(sSupportedAddressFields);
        mAddressEditor.edit(new AutofillAddress(mActivity, toEdit), mDoneCallback);

        EditorModel editorModel = mEditorModelCapture.getValue();
        Assert.assertNotNull(editorModel);
        List<EditorFieldModel> editorFields = editorModel.getFields();
        Assert.assertEquals(13, editorFields.size());

        // Verify behaviour only on the relevant subset of fields.
        editorFields.get(1).setValue("New honorific prefix");
        editorFields.get(2).setValue("New Name");
        editorFields.get(3).setValue("New admin area");
        editorModel.cancel();

        Assert.assertNotNull(mEditedAutofillAddress);
        Assert.assertEquals(
                sLocalProfile.getFullName(), mEditedAutofillAddress.getProfile().getFullName());
        Assert.assertEquals(sLocalProfile.getHonorificPrefix(),
                mEditedAutofillAddress.getProfile().getHonorificPrefix());
        Assert.assertEquals(
                sLocalProfile.getRegion(), mEditedAutofillAddress.getProfile().getRegion());
    }

    @Test
    @SmallTest
    public void edit_AlterProfile_CommitChanges() {
        AutofillProfile toEdit = new AutofillProfile(sLocalProfile);
        setUpAddressUiComponents(sSupportedAddressFields);
        mAddressEditor.edit(new AutofillAddress(mActivity, toEdit), mDoneCallback);

        Assert.assertNotNull(mEditorModelCapture.getValue());
        EditorModel editorModel = mEditorModelCapture.getValue();
        List<EditorFieldModel> editorFields = editorModel.getFields();
        Assert.assertEquals(13, editorFields.size());

        // Verify behaviour only on the relevant subset of fields.
        editorFields.get(4).setValue("New locality");
        editorFields.get(5).setValue("New dependent locality");
        editorFields.get(6).setValue("New organization");
        editorModel.done();

        Assert.assertNotNull(mEditedAutofillAddress);
        Assert.assertEquals("New locality", mEditedAutofillAddress.getProfile().getLocality());
        Assert.assertEquals("New dependent locality",
                mEditedAutofillAddress.getProfile().getDependentLocality());
        Assert.assertEquals(
                "New organization", mEditedAutofillAddress.getProfile().getCompanyName());
    }
}

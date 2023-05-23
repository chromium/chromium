// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
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
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.prefeditor.EditorDialog;
import org.chromium.chrome.browser.autofill.prefeditor.EditorModel;
import org.chromium.chrome.browser.autofill.settings.AutofillProfileBridge;
import org.chromium.chrome.browser.autofill.settings.AutofillProfileBridge.AddressField;
import org.chromium.chrome.browser.autofill.settings.AutofillProfileBridge.AddressUiComponent;
import org.chromium.chrome.browser.autofill.settings.AutofillProfileBridgeJni;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.autofill.prefeditor.EditorFieldModel;
import org.chromium.components.autofill.prefeditor.EditorFieldModel.DropdownKeyValue;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.stream.Collectors;

/** Unit tests for {@link AddressEditor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AddressEditorTest {
    private static final Locale DEFAULT_LOCALE = Locale.getDefault();
    private static final List<AddressUiComponent> SUPPORTED_ADDRESS_FIELDS = List.of(
            new AddressUiComponent(AddressField.RECIPIENT, /*label=*/"full name label",
                    /*isRequired=*/false, /*isFullLine=*/true),
            new AddressUiComponent(AddressField.ADMIN_AREA, /*label=*/"admin area label",
                    /*isRequired=*/true, /*isFullLine=*/true),
            new AddressUiComponent(AddressField.LOCALITY, /*label=*/"locality label",
                    /*isRequired=*/true, /*isFullLine=*/false),
            new AddressUiComponent(AddressField.DEPENDENT_LOCALITY,
                    /*label=*/"dependent locality label", /*isRequired=*/true,
                    /*isFullLine=*/false),
            new AddressUiComponent(AddressField.ORGANIZATION, /*label=*/"organization label",
                    /*isRequired=*/false, /*isFullLine=*/true),
            new AddressUiComponent(AddressField.SORTING_CODE, /*label=*/"sorting code label",
                    /*isRequired=*/false, /*isFullLine=*/false),
            new AddressUiComponent(AddressField.POSTAL_CODE, /*label=*/"postal code label",
                    /*isRequired=*/true, /*isFullLine=*/false),
            new AddressUiComponent(AddressField.STREET_ADDRESS, /*label=*/"street address label",
                    /*isRequired=*/true, /*isFullLine=*/true));

    private static final AutofillProfile sProfile = AutofillProfile.builder()
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
    private PersonalDataManager mPersonalDataManager;
    @Mock
    private Callback<AutofillAddress> mDoneCallback;
    @Mock
    private Callback<AutofillAddress> mCancelCallback;

    @Captor
    private ArgumentCaptor<EditorModel> mEditorModelCapture;
    @Captor
    private ArgumentCaptor<AutofillAddress> mAddressCapture;

    // Note: can't initialize this list statically because of how Robolectric
    // initializes Android library dependencies.
    private final List<DropdownKeyValue> mSupportedCountries =
            List.of(new DropdownKeyValue("US", "United States"),
                    new DropdownKeyValue("DE", "Germany"), new DropdownKeyValue("CU", "Cuba"));

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

        PersonalDataManager.setInstanceForTesting(mPersonalDataManager);

        setUpSupportedCountries(mSupportedCountries);

        when(mEditorDialog.getContext()).thenReturn(mActivity);
        doNothing().when(mEditorDialog).show(mEditorModelCapture.capture());
    }

    @After
    public void tearDown() {
        // Reset default values to avoid changing them for other batched tests.
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

    private static void validateTextField(EditorFieldModel field, String value, int inputTypeHint,
            String label, boolean isRequired, boolean isFullLine, boolean hasLengthCounter) {
        assertTrue(field.isTextField());
        assertEquals(field.getValue(), value);
        assertEquals(inputTypeHint, field.getInputTypeHint());
        assertEquals(label, field.getLabel());
        assertEquals(isRequired, field.isRequired());
        assertEquals(isFullLine, field.isFullLine());
        assertEquals(hasLengthCounter, field.hasLengthCounter());
    }

    private static void checkUiStringsHaveExpectedValues(EditorModel editorModel,
            String expectedDeleteTitle, String expectedDeleteText,
            @Nullable String expectedSourceNotice) {
        assertNotNull(editorModel);

        assertEquals(expectedDeleteTitle, editorModel.getDeleteConfirmationTitle());
        assertEquals(expectedDeleteText, editorModel.getDeleteConfirmationText());
        assertEquals(expectedSourceNotice, editorModel.getFooterMessageText());
    }

    private void validateShownFields(EditorModel editorModel, AutofillProfile profile) {
        assertNotNull(editorModel);
        List<EditorFieldModel> editorFields = editorModel.getFields();
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
        assertEquals(10, editorFields.size());

        // Fields obtained from backend must be placed after the country dropdown.
        validateTextField(editorFields.get(1), profile.getFullName(),
                EditorFieldModel.INPUT_TYPE_HINT_PERSON_NAME, /*label=*/"full name label",
                /*isRequired=*/true, /*isFullLine=*/true, /*hasLengthCounter=*/false);
        validateTextField(editorFields.get(2), profile.getRegion(),
                EditorFieldModel.INPUT_TYPE_HINT_REGION, /*label=*/"admin area label",
                /*isRequired=*/true, /*isFullLine=*/true, /*hasLengthCounter=*/false);
        // Locality field is forced to occupy full line.
        validateTextField(editorFields.get(3), profile.getLocality(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, /*label=*/"locality label",
                /*isRequired=*/true, /*isFullLine=*/true, /*hasLengthCounter=*/false);

        // Note: dependent locality is a required field for address profiles stored in Google
        // account, but it's still marked as optional by the editor when the corresponding field in
        // the existing address profile is empty. It is considered required for new address
        // profiles.
        validateTextField(editorFields.get(4), profile.getDependentLocality(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, /*label=*/"dependent locality label",
                /*isRequired=*/true, /*isFullLine=*/true,
                /*hasLengthCounter=*/false);

        validateTextField(editorFields.get(5), profile.getCompanyName(),
                EditorFieldModel.INPUT_TYPE_HINT_NONE, /*label=*/"organization label",
                /*isRequired=*/false, /*isFullLine=*/true, /*hasLengthCounter=*/false);

        validateTextField(editorFields.get(6), profile.getSortingCode(),
                EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC, /*label=*/"sorting code label",
                /*isRequired=*/false, /*isFullLine=*/false,
                /*hasLengthCounter=*/false);
        validateTextField(editorFields.get(7), profile.getPostalCode(),
                EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC, /*label=*/"postal code label",
                /*isRequired=*/true, /*isFullLine=*/false,
                /*hasLengthCounter=*/false);
        validateTextField(editorFields.get(8), profile.getStreetAddress(),
                EditorFieldModel.INPUT_TYPE_HINT_STREET_LINES, /*label=*/"street address label",
                /*isRequired=*/true, /*isFullLine=*/true,
                /*hasLengthCounter=*/false);
        validateTextField(editorFields.get(9), profile.getPhoneNumber(),
                EditorFieldModel.INPUT_TYPE_HINT_PHONE,
                mActivity.getString(R.string.autofill_profile_editor_phone_number),
                /*isRequired=*/true, /*isFullLine=*/true,
                /*hasLengthCounter=*/false);
    }

    @Test
    @SmallTest
    public void validateDefaultFields() {
        setUpAddressUiComponents(new ArrayList(), /*countryCode=*/"US");
        mAddressEditor = new AddressEditor(/*saveToDisk=*/false);
        mAddressEditor.setEditorDialog(mEditorDialog);
        doAnswer(unused -> {
            mAddressEditor.onSubKeysReceived(null, null);
            return null;
        })
                .when(mPersonalDataManager)
                .getRegionSubKeys(anyString(), any());
        mAddressEditor.edit(new AutofillAddress(mActivity, sProfile), unused -> {});

        assertNotNull(mEditorModelCapture.getValue());
        List<EditorFieldModel> editorFields = mEditorModelCapture.getValue().getFields();
        // Following values are set regardless of the UI components list
        // received from backend when nicknames are disabled:
        // editorFields[0] - country dropdown.
        // editorFields[1] - phone field.
        assertEquals(2, editorFields.size());

        EditorFieldModel countryDropdown = editorFields.get(0);
        assertTrue(countryDropdown.isDropdownField());
        assertTrue(countryDropdown.isFullLine());
        assertEquals(countryDropdown.getValue(), AutofillAddress.getCountryCode(sProfile));
        assertEquals(countryDropdown.getLabel(),
                mActivity.getString(R.string.autofill_profile_editor_country));
        assertEquals(mSupportedCountries.size(), countryDropdown.getDropdownKeyValues().size());
        assertThat(mSupportedCountries,
                containsInAnyOrder(countryDropdown.getDropdownKeyValues().toArray()));

        validateTextField(editorFields.get(1), sProfile.getPhoneNumber(),
                EditorFieldModel.INPUT_TYPE_HINT_PHONE,
                mActivity.getString(R.string.autofill_profile_editor_phone_number),
                /*isRequired=*/true, /*isFullLine=*/true,
                /*hasLengthCounter=*/false);
    }

    @Test
    @SmallTest
    public void validateAdminAreaDropdown() {
        // Configure only admin area field to keep the test focused.
        setUpAddressUiComponents(
                List.of(new AddressUiComponent(AddressField.ADMIN_AREA, "admin area label",
                        /*isRequired=*/true, /*isFullLine=*/true)),
                /*countryCode=*/"US");
        mAddressEditor = new AddressEditor(/*saveToDisk=*/false);
        mAddressEditor.setEditorDialog(mEditorDialog);
        doAnswer(unused -> {
            mAddressEditor.onSubKeysReceived(new String[] {"CA", "NY", "TX"},
                    new String[] {"California", "New York", "Texas"});
            return null;
        })
                .when(mPersonalDataManager)
                .getRegionSubKeys(anyString(), any());
        mAddressEditor.edit(new AutofillAddress(mActivity, sProfile), unused -> {});

        assertNotNull(mEditorModelCapture.getValue());
        List<EditorFieldModel> editorFields = mEditorModelCapture.getValue().getFields();
        // Following values are set regardless of the UI components list
        // received from backend when nicknames are disabled:
        // editorFields[0] - country dropdown.
        // editorFields[1] - admin area dropdown.
        // editorFields[2] - phone field.
        assertEquals(3, editorFields.size());

        EditorFieldModel adminAreaDropdown = editorFields.get(1);

        List<DropdownKeyValue> adminAreas = List.of(new DropdownKeyValue("CA", "California"),
                new DropdownKeyValue("NY", "New York"), new DropdownKeyValue("TX", "Texas"));
        assertThat(
                adminAreas, containsInAnyOrder(adminAreaDropdown.getDropdownKeyValues().toArray()));

        assertTrue(adminAreaDropdown.isDropdownField());
        assertTrue(adminAreaDropdown.isFullLine());
        assertEquals(adminAreaDropdown.getValue(), sProfile.getRegion());
        assertEquals(adminAreaDropdown.getLabel(), "admin area label");
    }

    @Test
    @SmallTest
    public void validateShownFields_NewAddressProfile() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS, /*countryCode=*/"US");
        mAddressEditor = new AddressEditor(/*saveToDisk=*/false);
        doAnswer(unused -> {
            mAddressEditor.onSubKeysReceived(null, null);
            return null;
        })
                .when(mPersonalDataManager)
                .getRegionSubKeys(anyString(), any());
        mAddressEditor.setEditorDialog(mEditorDialog);
        mAddressEditor.edit(null, unused -> {});

        validateShownFields(mEditorModelCapture.getValue(), AutofillProfile.builder().build());
    }

    @Test
    @SmallTest
    public void validateShownFields_ExistingAddressProfile() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS, /*countryCode=*/"US");
        mAddressEditor = new AddressEditor(/*saveToDisk=*/false);
        doAnswer(unused -> {
            mAddressEditor.onSubKeysReceived(null, null);
            return null;
        })
                .when(mPersonalDataManager)
                .getRegionSubKeys(anyString(), any());
        mAddressEditor.setEditorDialog(mEditorDialog);
        mAddressEditor.edit(new AutofillAddress(mActivity, sProfile), unused -> {});

        validateShownFields(mEditorModelCapture.getValue(), sProfile);
    }

    @Test
    @SmallTest
    public void edit_ChangeCountry_FieldsSetChanges() {
        mAddressEditor = new AddressEditor(/*saveToDisk=*/false);
        setUpAddressUiComponents(
                List.of(new AddressUiComponent(AddressField.SORTING_CODE, "sorting code label",
                        /*isRequired=*/false, /*isFullLine=*/true)),
                /*countryCode=*/"US");
        setUpAddressUiComponents(
                List.of(new AddressUiComponent(AddressField.STREET_ADDRESS, "street address label",
                        /*isRequired=*/true, /*isFullLine=*/true)),
                /*countryCode=*/"DE");
        doAnswer(unused -> {
            mAddressEditor.onSubKeysReceived(null, null);
            return null;
        })
                .when(mPersonalDataManager)
                .getRegionSubKeys(anyString(), any());
        mAddressEditor.setEditorDialog(mEditorDialog);
        mAddressEditor.edit(null, unused -> {});

        assertNotNull(mEditorModelCapture.getValue());
        List<EditorFieldModel> editorFields = mEditorModelCapture.getValue().getFields();

        // editorFields[0] - country dropdown.
        // editorFields[1] - sorting code field.
        // editorFields[2] - phone number field.
        assertEquals(3, editorFields.size());
        assertThat(editorFields.stream()
                           .map(EditorFieldModel::getInputTypeHint)
                           .collect(Collectors.toList()),
                containsInAnyOrder(EditorFieldModel.INPUT_TYPE_HINT_DROPDOWN,
                        EditorFieldModel.INPUT_TYPE_HINT_ALPHA_NUMERIC,
                        EditorFieldModel.INPUT_TYPE_HINT_PHONE));
        EditorFieldModel countryDropdown = editorFields.get(0);

        countryDropdown.setDropdownKey("DE", () -> {});
        // editorFields[0] - country dropdown.
        // editorFields[1] - street address field.
        // editorFields[2] - phone number field.
        assertEquals(3, editorFields.size());
        assertThat(editorFields.stream()
                           .map(EditorFieldModel::getInputTypeHint)
                           .collect(Collectors.toList()),
                containsInAnyOrder(EditorFieldModel.INPUT_TYPE_HINT_DROPDOWN,
                        EditorFieldModel.INPUT_TYPE_HINT_STREET_LINES,
                        EditorFieldModel.INPUT_TYPE_HINT_PHONE));
    }

    @Test
    @SmallTest
    public void edit_AlterAddressProfile_Cancel() {
        mAddressEditor = new AddressEditor(/*saveToDisk=*/false);
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS, /*countryCode=*/"US");
        doAnswer(unused -> {
            mAddressEditor.onSubKeysReceived(null, null);
            return null;
        })
                .when(mPersonalDataManager)
                .getRegionSubKeys(anyString(), any());
        mAddressEditor.setEditorDialog(mEditorDialog);
        mAddressEditor.edit(new AutofillAddress(mActivity, new AutofillProfile(sProfile)),
                mDoneCallback, mCancelCallback);

        EditorModel editorModel = mEditorModelCapture.getValue();
        assertNotNull(editorModel);
        List<EditorFieldModel> editorFields = editorModel.getFields();
        assertEquals(10, editorFields.size());

        // Verify behaviour only on the relevant subset of fields.
        editorFields.get(1).setValue("New Name");
        editorFields.get(2).setValue("New admin area");
        editorFields.get(3).setValue("New locality");
        editorModel.cancel();

        verify(mDoneCallback, times(0)).onResult(any());
        verify(mCancelCallback, times(1)).onResult(any());
    }

    @Test
    @SmallTest
    public void edit_AlterAddressProfile_CommitChanges() {
        mAddressEditor = new AddressEditor(/*saveToDisk=*/false);
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS, /*countryCode=*/"US");
        doAnswer(unused -> {
            mAddressEditor.onSubKeysReceived(null, null);
            return null;
        })
                .when(mPersonalDataManager)
                .getRegionSubKeys(anyString(), any());
        mAddressEditor.setEditorDialog(mEditorDialog);
        mAddressEditor.edit(new AutofillAddress(mActivity, new AutofillProfile(sProfile)),
                mDoneCallback, mCancelCallback);

        assertNotNull(mEditorModelCapture.getValue());
        EditorModel editorModel = mEditorModelCapture.getValue();
        List<EditorFieldModel> editorFields = editorModel.getFields();
        assertEquals(10, editorFields.size());

        // Verify behaviour only on the relevant subset of fields.
        editorFields.get(3).setValue("New locality");
        editorFields.get(4).setValue("New dependent locality");
        editorFields.get(5).setValue("New organization");
        editorModel.done();

        verify(mDoneCallback, times(1)).onResult(mAddressCapture.capture());
        verify(mCancelCallback, times(0)).onResult(any());
        AutofillAddress address = mAddressCapture.getValue();
        assertNotNull(address);
        assertEquals("New locality", address.getProfile().getLocality());
        assertEquals("New dependent locality", address.getProfile().getDependentLocality());
        assertEquals("New organization", address.getProfile().getCompanyName());
    }
}

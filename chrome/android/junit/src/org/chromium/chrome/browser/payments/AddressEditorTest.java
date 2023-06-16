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
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DONE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_FIELDS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_FULL_LINE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.DROPDOWN;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.TEXT_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.SHOW_REQUIRED_INDICATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.LENGTH_COUNTER_LIMIT_NONE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_INPUT_TYPE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_LENGTH_COUNTER_LIMIT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.setDropdownKey;

import android.app.Activity;

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
import org.chromium.chrome.browser.autofill.AutofillAddress;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge.AddressField;
import org.chromium.chrome.browser.autofill.AutofillProfileBridge.AddressUiComponent;
import org.chromium.chrome.browser.autofill.AutofillProfileBridgeJni;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.editors.EditorDialogView;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownKeyValue;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Spliterator;
import java.util.Spliterators;
import java.util.stream.Collectors;
import java.util.stream.StreamSupport;

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
    private EditorDialogView mEditorDialog;
    @Mock
    private PersonalDataManager mPersonalDataManager;
    @Mock
    private Callback<AutofillAddress> mDoneCallback;
    @Mock
    private Callback<AutofillAddress> mCancelCallback;

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

    private static void validateTextField(ListItem fieldItem, String value,
            @TextInputType int textInputType, String label, boolean isRequired, boolean isFullLine,
            int lengthCounter) {
        assertEquals(TEXT_INPUT, fieldItem.type);

        PropertyModel field = fieldItem.model;
        assertEquals(value, field.get(VALUE));
        assertEquals(textInputType, field.get(TEXT_INPUT_TYPE));
        assertEquals(label, field.get(LABEL));
        assertEquals(isRequired, field.get(IS_REQUIRED));
        assertEquals(isFullLine, field.get(IS_FULL_LINE));
        assertEquals(lengthCounter, field.get(TEXT_LENGTH_COUNTER_LIMIT));
    }

    private void validateShownFields(PropertyModel editorModel, AutofillProfile profile) {
        assertNotNull(editorModel);
        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
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
                TextInputType.PERSON_NAME_INPUT,
                /*label=*/"full name label",
                /*isRequired=*/true, /*isFullLine=*/true, LENGTH_COUNTER_LIMIT_NONE);
        validateTextField(editorFields.get(2), profile.getRegion(), TextInputType.REGION_INPUT,
                /*label=*/"admin area label",
                /*isRequired=*/true, /*isFullLine=*/true, LENGTH_COUNTER_LIMIT_NONE);
        // Locality field is forced to occupy full line.
        validateTextField(editorFields.get(3), profile.getLocality(),
                TextInputType.PLAIN_TEXT_INPUT,
                /*label=*/"locality label",
                /*isRequired=*/true, /*isFullLine=*/true, LENGTH_COUNTER_LIMIT_NONE);

        // Note: dependent locality is a required field for address profiles stored in Google
        // account, but it's still marked as optional by the editor when the corresponding field in
        // the existing address profile is empty. It is considered required for new address
        // profiles.
        validateTextField(editorFields.get(4), profile.getDependentLocality(),
                TextInputType.PLAIN_TEXT_INPUT, /*label=*/"dependent locality label",
                /*isRequired=*/true, /*isFullLine=*/true, LENGTH_COUNTER_LIMIT_NONE);

        validateTextField(editorFields.get(5), profile.getCompanyName(),
                TextInputType.PLAIN_TEXT_INPUT,
                /*label=*/"organization label",
                /*isRequired=*/false, /*isFullLine=*/true, LENGTH_COUNTER_LIMIT_NONE);

        validateTextField(editorFields.get(6), profile.getSortingCode(),
                TextInputType.ALPHA_NUMERIC_INPUT, /*label=*/"sorting code label",
                /*isRequired=*/false, /*isFullLine=*/false, LENGTH_COUNTER_LIMIT_NONE);
        validateTextField(editorFields.get(7), profile.getPostalCode(),
                TextInputType.ALPHA_NUMERIC_INPUT, /*label=*/"postal code label",
                /*isRequired=*/true, /*isFullLine=*/false, LENGTH_COUNTER_LIMIT_NONE);
        validateTextField(editorFields.get(8), profile.getStreetAddress(),
                TextInputType.STREET_ADDRESS_INPUT, /*label=*/"street address label",
                /*isRequired=*/true, /*isFullLine=*/true, LENGTH_COUNTER_LIMIT_NONE);
        validateTextField(editorFields.get(9), profile.getPhoneNumber(),
                TextInputType.PHONE_NUMBER_INPUT,
                mActivity.getString(R.string.autofill_profile_editor_phone_number),
                /*isRequired=*/true, /*isFullLine=*/true, LENGTH_COUNTER_LIMIT_NONE);
    }

    @Test
    @SmallTest
    public void validateRequiredFieldIndicator() {
        setUpAddressUiComponents(new ArrayList(), /*countryCode=*/"US");
        doAnswer(unused -> {
            mAddressEditor.onSubKeysReceived(null, null);
            return null;
        })
                .when(mPersonalDataManager)
                .getRegionSubKeys(anyString(), any());

        mAddressEditor = new AddressEditor(/*saveToDisk=*/false);
        mAddressEditor.setEditorDialog(mEditorDialog);
        mAddressEditor.edit(new AutofillAddress(mActivity, sProfile), unused -> {});

        assertNotNull(mAddressEditor.getEditorModelForTesting());
        assertTrue(mAddressEditor.getEditorModelForTesting().get(SHOW_REQUIRED_INDICATOR));
    }

    @Test
    @SmallTest
    public void validateDefaultFields() {
        setUpAddressUiComponents(new ArrayList(), /*countryCode=*/"US");
        doAnswer(unused -> {
            mAddressEditor.onSubKeysReceived(null, null);
            return null;
        })
                .when(mPersonalDataManager)
                .getRegionSubKeys(anyString(), any());

        mAddressEditor = new AddressEditor(/*saveToDisk=*/false);
        mAddressEditor.setEditorDialog(mEditorDialog);
        mAddressEditor.edit(new AutofillAddress(mActivity, sProfile), unused -> {});

        assertNotNull(mAddressEditor.getEditorModelForTesting());
        ListModel<ListItem> editorFields =
                mAddressEditor.getEditorModelForTesting().get(EDITOR_FIELDS);
        // Following values are set regardless of the UI components list
        // received from backend when nicknames are disabled:
        // editorFields[0] - country dropdown.
        // editorFields[1] - phone field.
        assertEquals(2, editorFields.size());

        ListItem countryDropdownItem = editorFields.get(0);
        assertEquals(countryDropdownItem.type, DROPDOWN);

        PropertyModel countryDropdown = countryDropdownItem.model;
        assertTrue(countryDropdown.get(IS_FULL_LINE));
        assertEquals(countryDropdown.get(VALUE), AutofillAddress.getCountryCode(sProfile));
        assertEquals(countryDropdown.get(LABEL),
                mActivity.getString(R.string.autofill_profile_editor_country));
        assertEquals(
                mSupportedCountries.size(), countryDropdown.get(DROPDOWN_KEY_VALUE_LIST).size());
        assertThat(mSupportedCountries,
                containsInAnyOrder(countryDropdown.get(DROPDOWN_KEY_VALUE_LIST).toArray()));

        validateTextField(editorFields.get(1), sProfile.getPhoneNumber(),
                TextInputType.PHONE_NUMBER_INPUT,
                mActivity.getString(R.string.autofill_profile_editor_phone_number),
                /*isRequired=*/true, /*isFullLine=*/true, LENGTH_COUNTER_LIMIT_NONE);
    }

    @Test
    @SmallTest
    public void validateAdminAreaDropdown() {
        // Configure only admin area field to keep the test focused.
        setUpAddressUiComponents(
                List.of(new AddressUiComponent(AddressField.ADMIN_AREA, "admin area label",
                        /*isRequired=*/true, /*isFullLine=*/true)),
                /*countryCode=*/"US");
        doAnswer(unused -> {
            mAddressEditor.onSubKeysReceived(new String[] {"CA", "NY", "TX"},
                    new String[] {"California", "New York", "Texas"});
            return null;
        })
                .when(mPersonalDataManager)
                .getRegionSubKeys(anyString(), any());
        mAddressEditor = new AddressEditor(/*saveToDisk=*/false);
        mAddressEditor.setEditorDialog(mEditorDialog);
        mAddressEditor.edit(new AutofillAddress(mActivity, sProfile), unused -> {});

        assertNotNull(mAddressEditor.getEditorModelForTesting());
        ListModel<ListItem> editorFields =
                mAddressEditor.getEditorModelForTesting().get(EDITOR_FIELDS);
        // Following values are set regardless of the UI components list
        // received from backend when nicknames are disabled:
        // editorFields[0] - country dropdown.
        // editorFields[1] - admin area dropdown.
        // editorFields[2] - phone field.
        assertEquals(3, editorFields.size());

        ListItem adminAreaDropdownItem = editorFields.get(1);
        assertEquals(DROPDOWN, adminAreaDropdownItem.type);

        PropertyModel adminAreaDropdown = adminAreaDropdownItem.model;
        List<DropdownKeyValue> adminAreas = List.of(new DropdownKeyValue("CA", "California"),
                new DropdownKeyValue("NY", "New York"), new DropdownKeyValue("TX", "Texas"));
        assertThat(adminAreas,
                containsInAnyOrder(adminAreaDropdown.get(DROPDOWN_KEY_VALUE_LIST).toArray()));

        assertTrue(adminAreaDropdown.get(IS_FULL_LINE));
        assertEquals(adminAreaDropdown.get(VALUE), sProfile.getRegion());
        assertEquals(adminAreaDropdown.get(LABEL), "admin area label");
    }

    @Test
    @SmallTest
    public void validateShownFields_NewAddressProfile() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS, /*countryCode=*/"US");
        doAnswer(unused -> {
            mAddressEditor.onSubKeysReceived(null, null);
            return null;
        })
                .when(mPersonalDataManager)
                .getRegionSubKeys(anyString(), any());

        mAddressEditor = new AddressEditor(/*saveToDisk=*/false);
        mAddressEditor.setEditorDialog(mEditorDialog);
        mAddressEditor.edit(null, unused -> {});

        validateShownFields(
                mAddressEditor.getEditorModelForTesting(), AutofillProfile.builder().build());
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

        validateShownFields(mAddressEditor.getEditorModelForTesting(), sProfile);
    }

    @Test
    @SmallTest
    public void edit_ChangeCountry_FieldsSetChanges() {
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
        mAddressEditor = new AddressEditor(/*saveToDisk=*/false);
        mAddressEditor.setEditorDialog(mEditorDialog);
        mAddressEditor.edit(null, unused -> {});

        assertNotNull(mAddressEditor.getEditorModelForTesting());
        ListModel<ListItem> editorFields =
                mAddressEditor.getEditorModelForTesting().get(EDITOR_FIELDS);

        // editorFields[0] - country dropdown.
        // editorFields[1] - sorting code field.
        // editorFields[2] - phone number field.
        assertEquals(3, editorFields.size());
        assertThat(StreamSupport
                           .stream(Spliterators.spliteratorUnknownSize(
                                           editorFields.iterator(), Spliterator.ORDERED),
                                   false)
                           .skip(1)
                           .map(item -> { return item.model.get(TEXT_INPUT_TYPE); })
                           .collect(Collectors.toList()),
                containsInAnyOrder(
                        TextInputType.ALPHA_NUMERIC_INPUT, TextInputType.PHONE_NUMBER_INPUT));
        PropertyModel countryDropdown = editorFields.get(0).model;

        setDropdownKey(countryDropdown, "DE");
        ListModel<ListItem> editorFieldsGermany =
                mAddressEditor.getEditorModelForTesting().get(EDITOR_FIELDS);
        // editorFields[0] - country dropdown.
        // editorFields[1] - street address field.
        // editorFields[2] - phone number field.
        assertEquals(3, editorFieldsGermany.size());
        assertThat(StreamSupport
                           .stream(Spliterators.spliteratorUnknownSize(
                                           editorFieldsGermany.iterator(), Spliterator.ORDERED),
                                   false)
                           .skip(1)
                           .map(item -> { return item.model.get(TEXT_INPUT_TYPE); })
                           .collect(Collectors.toList()),
                containsInAnyOrder(
                        TextInputType.STREET_ADDRESS_INPUT, TextInputType.PHONE_NUMBER_INPUT));
    }

    @Test
    @SmallTest
    public void edit_AlterAddressProfile_Cancel() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS, /*countryCode=*/"US");
        doAnswer(unused -> {
            mAddressEditor.onSubKeysReceived(null, null);
            return null;
        })
                .when(mPersonalDataManager)
                .getRegionSubKeys(anyString(), any());
        mAddressEditor = new AddressEditor(/*saveToDisk=*/false);
        mAddressEditor.setEditorDialog(mEditorDialog);
        mAddressEditor.edit(new AutofillAddress(mActivity, new AutofillProfile(sProfile)),
                mDoneCallback, mCancelCallback);

        PropertyModel editorModel = mAddressEditor.getEditorModelForTesting();
        assertNotNull(editorModel);
        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(10, editorFields.size());

        // Verify behaviour only on the relevant subset of fields.
        editorFields.get(1).model.set(VALUE, "New Name");
        editorFields.get(2).model.set(VALUE, "New admin area");
        editorFields.get(3).model.set(VALUE, "New locality");
        editorModel.get(CANCEL_RUNNABLE).run();

        verify(mDoneCallback, times(0)).onResult(any());
        verify(mCancelCallback, times(1)).onResult(any());
    }

    @Test
    @SmallTest
    public void edit_AlterAddressProfile_CommitChanges() {
        setUpAddressUiComponents(SUPPORTED_ADDRESS_FIELDS, /*countryCode=*/"US");
        doAnswer(unused -> {
            mAddressEditor.onSubKeysReceived(null, null);
            return null;
        })
                .when(mPersonalDataManager)
                .getRegionSubKeys(anyString(), any());

        mAddressEditor = new AddressEditor(/*saveToDisk=*/false);
        mAddressEditor.setEditorDialog(mEditorDialog);
        mAddressEditor.edit(new AutofillAddress(mActivity, new AutofillProfile(sProfile)),
                mDoneCallback, mCancelCallback);

        assertNotNull(mAddressEditor.getEditorModelForTesting());
        PropertyModel editorModel = mAddressEditor.getEditorModelForTesting();
        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(10, editorFields.size());

        // Verify behaviour only on the relevant subset of fields.
        editorFields.get(3).model.set(VALUE, "New locality");
        editorFields.get(4).model.set(VALUE, "New dependent locality");
        editorFields.get(5).model.set(VALUE, "New organization");
        editorModel.get(DONE_RUNNABLE).run();

        verify(mDoneCallback, times(1)).onResult(mAddressCapture.capture());
        verify(mCancelCallback, times(0)).onResult(any());
        AutofillAddress address = mAddressCapture.getValue();
        assertNotNull(address);
        assertEquals("New locality", address.getProfile().getLocality());
        assertEquals("New dependent locality", address.getProfile().getDependentLocality());
        assertEquals("New organization", address.getProfile().getCompanyName());
    }
}

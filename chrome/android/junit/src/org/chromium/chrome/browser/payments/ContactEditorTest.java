// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.anyOf;
import static org.hamcrest.Matchers.isEmptyString;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DONE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_FIELDS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.TEXT_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.SHOW_REQUIRED_INDICATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_FIELD_TYPE;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.editors.EditorDialogView;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldItem;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.FieldType;
import org.chromium.payments.mojom.PayerErrors;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ContactEditor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContactEditorTest {
    private static final AutofillProfile sProfile =
            AutofillProfile.builder()
                    .setFullName("John Doe")
                    .setCompanyName("Google")
                    .setStreetAddress("Lake Street 123")
                    .setRegion("Bayern")
                    .setLocality("Munich")
                    .setPostalCode("12345")
                    .setCountryCode("DE")
                    .setPhoneNumber("+49-000-000-00-000")
                    .setEmailAddress("email@example.com")
                    .setLanguageCode("de")
                    .build();

    @Mock private PersonalDataManager mPersonalDataManager;
    @Mock private EditorDialogView mEditorDialog;

    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.setupActivity(TestActivity.class);

        when(mEditorDialog.getContext()).thenReturn(mActivity);
    }

    private static void validateTextField(
            FieldItem fieldItem, String value, int textFieldType, String label) {
        assertEquals(TEXT_INPUT, fieldItem.type);
        assertTrue(fieldItem.isFullLine);

        PropertyModel field = fieldItem.model;
        assertEquals(value, field.get(VALUE));
        assertEquals(textFieldType, field.get(TEXT_FIELD_TYPE));
        assertEquals(label, field.get(LABEL));
        assertTrue(field.get(IS_REQUIRED));
    }

    private void validateErrorMessages(PropertyModel editorModel, boolean errorsPresent) {
        assertNotNull(editorModel);
        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(3, editorFields.size());

        Matcher<String> requiredFieldMatcher =
                errorsPresent ? not(isEmptyString()) : anyOf(nullValue(), isEmptyString());
        assertThat(editorFields.get(0).model.get(ERROR_MESSAGE), requiredFieldMatcher);
        assertThat(editorFields.get(1).model.get(ERROR_MESSAGE), requiredFieldMatcher);
        assertThat(editorFields.get(2).model.get(ERROR_MESSAGE), requiredFieldMatcher);
    }

    @Test
    @SmallTest
    public void validateRequiredFieldIndicator() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ true,
                        /* requestPayerPhone= */ false,
                        /* requestPayerEmail= */ false,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        editor.edit(null, unused -> {});

        assertNotNull(editor.getEditorModelForTesting());
        assertTrue(editor.getEditorModelForTesting().get(SHOW_REQUIRED_INDICATOR));
    }

    @Test
    @SmallTest
    public void requestName_NewContact() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ true,
                        /* requestPayerPhone= */ false,
                        /* requestPayerEmail= */ false,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        editor.edit(null, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        validateTextField(
                editorFields.get(0),
                null,
                FieldType.NAME_FULL,
                mActivity.getString(R.string.payments_name_field_in_contact_details));
    }

    @Test
    @SmallTest
    public void requestPhone_NewContact() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ false,
                        /* requestPayerPhone= */ true,
                        /* requestPayerEmail= */ false,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        editor.edit(null, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        validateTextField(
                editorFields.get(0),
                null,
                FieldType.PHONE_HOME_WHOLE_NUMBER,
                mActivity.getString(R.string.autofill_profile_editor_phone_number));
    }

    @Test
    @SmallTest
    public void requestEmail_NewContact() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ false,
                        /* requestPayerPhone= */ false,
                        /* requestPayerEmail= */ true,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        editor.edit(null, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        validateTextField(
                editorFields.get(0),
                null,
                FieldType.EMAIL_ADDRESS,
                mActivity.getString(R.string.autofill_profile_editor_email_address));
    }

    @Test
    @SmallTest
    public void requestAllFields_NewContact() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ true,
                        /* requestPayerPhone= */ true,
                        /* requestPayerEmail= */ true,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        editor.edit(null, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(3, editorFields.size());
        validateTextField(
                editorFields.get(0),
                null,
                FieldType.NAME_FULL,
                mActivity.getString(R.string.payments_name_field_in_contact_details));
        validateTextField(
                editorFields.get(1),
                null,
                FieldType.PHONE_HOME_WHOLE_NUMBER,
                mActivity.getString(R.string.autofill_profile_editor_phone_number));
        validateTextField(
                editorFields.get(2),
                null,
                FieldType.EMAIL_ADDRESS,
                mActivity.getString(R.string.autofill_profile_editor_email_address));
    }

    @Test
    @SmallTest
    public void requestName_ExistingContact() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ true,
                        /* requestPayerPhone= */ false,
                        /* requestPayerEmail= */ false,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact =
                new AutofillContact(
                        mActivity,
                        sProfile,
                        "Payer name",
                        null,
                        null,
                        ContactEditor.COMPLETE,
                        true,
                        false,
                        false);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        validateTextField(
                editorFields.get(0),
                "Payer name",
                FieldType.NAME_FULL,
                mActivity.getString(R.string.payments_name_field_in_contact_details));
    }

    @Test
    @SmallTest
    public void requestPhone_ExistingContact() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ false,
                        /* requestPayerPhone= */ true,
                        /* requestPayerEmail= */ false,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact =
                new AutofillContact(
                        mActivity,
                        sProfile,
                        null,
                        "Payer phone",
                        null,
                        ContactEditor.COMPLETE,
                        false,
                        true,
                        false);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        validateTextField(
                editorFields.get(0),
                "Payer phone",
                FieldType.PHONE_HOME_WHOLE_NUMBER,
                mActivity.getString(R.string.autofill_profile_editor_phone_number));
    }

    @Test
    @SmallTest
    public void requestEmail_ExistingContact() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ false,
                        /* requestPayerPhone= */ false,
                        /* requestPayerEmail= */ true,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact =
                new AutofillContact(
                        mActivity,
                        sProfile,
                        null,
                        null,
                        "Payer email",
                        ContactEditor.COMPLETE,
                        false,
                        false,
                        true);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        validateTextField(
                editorFields.get(0),
                "Payer email",
                FieldType.EMAIL_ADDRESS,
                mActivity.getString(R.string.autofill_profile_editor_email_address));
    }

    @Test
    @SmallTest
    public void requestAllFields_ExistingContact() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ true,
                        /* requestPayerPhone= */ true,
                        /* requestPayerEmail= */ true,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact =
                new AutofillContact(
                        mActivity,
                        sProfile,
                        "Payer name",
                        "Payer phone",
                        "Payer email",
                        ContactEditor.COMPLETE,
                        true,
                        true,
                        true);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(3, editorFields.size());
        validateTextField(
                editorFields.get(0),
                "Payer name",
                FieldType.NAME_FULL,
                mActivity.getString(R.string.payments_name_field_in_contact_details));
        validateTextField(
                editorFields.get(1),
                "Payer phone",
                FieldType.PHONE_HOME_WHOLE_NUMBER,
                mActivity.getString(R.string.autofill_profile_editor_phone_number));
        validateTextField(
                editorFields.get(2),
                "Payer email",
                FieldType.EMAIL_ADDRESS,
                mActivity.getString(R.string.autofill_profile_editor_email_address));
    }

    @Test
    @SmallTest
    public void editName_CancelEditing() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ true,
                        /* requestPayerPhone= */ false,
                        /* requestPayerEmail= */ false,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact =
                new AutofillContact(
                        mActivity,
                        new AutofillProfile(sProfile),
                        "Payer name",
                        null,
                        null,
                        ContactEditor.COMPLETE,
                        true,
                        false,
                        false);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        editorFields.get(0).model.set(VALUE, "Modified name");
        editorModel.get(CANCEL_RUNNABLE).run();

        assertEquals("Payer name", contact.getPayerName());
        assertNull(contact.getPayerPhone());
        assertNull(contact.getPayerEmail());
        assertEquals(sProfile.getFullName(), contact.getProfile().getFullName());
    }

    @Test
    @SmallTest
    public void editPhone_CancelEditing() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ false,
                        /* requestPayerPhone= */ true,
                        /* requestPayerEmail= */ false,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact =
                new AutofillContact(
                        mActivity,
                        new AutofillProfile(sProfile),
                        null,
                        "Payer phone",
                        null,
                        ContactEditor.COMPLETE,
                        false,
                        true,
                        false);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        editorFields.get(0).model.set(VALUE, "Modified phone");
        editorModel.get(CANCEL_RUNNABLE).run();

        assertNull(contact.getPayerName());
        assertEquals("Payer phone", contact.getPayerPhone());
        assertNull(contact.getPayerEmail());
        assertEquals(sProfile.getPhoneNumber(), contact.getProfile().getPhoneNumber());
    }

    @Test
    @SmallTest
    public void editEmail_CancelEditing() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ false,
                        /* requestPayerPhone= */ false,
                        /* requestPayerEmail= */ true,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact =
                new AutofillContact(
                        mActivity,
                        new AutofillProfile(sProfile),
                        null,
                        null,
                        "Payer email",
                        ContactEditor.COMPLETE,
                        false,
                        false,
                        true);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        editorFields.get(0).model.set(VALUE, "Modified email");
        editorModel.get(CANCEL_RUNNABLE).run();

        assertNull(contact.getPayerName());
        assertNull(contact.getPayerPhone());
        assertEquals("Payer email", contact.getPayerEmail());
        assertEquals(sProfile.getEmailAddress(), contact.getProfile().getEmailAddress());
    }

    @Test
    @SmallTest
    public void editName_CommitChanges() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ true,
                        /* requestPayerPhone= */ false,
                        /* requestPayerEmail= */ false,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact =
                new AutofillContact(
                        mActivity,
                        new AutofillProfile(sProfile),
                        "Payer name",
                        null,
                        null,
                        ContactEditor.COMPLETE,
                        true,
                        false,
                        false);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        editorFields.get(0).model.set(VALUE, "Modified name");
        editorModel.get(DONE_RUNNABLE).run();

        assertEquals("Modified name", contact.getPayerName());
        assertNull(contact.getPayerPhone());
        assertNull(contact.getPayerEmail());
        assertEquals("Modified name", contact.getProfile().getFullName());
    }

    @Test
    @SmallTest
    public void editPhone_CommitChanges() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ false,
                        /* requestPayerPhone= */ true,
                        /* requestPayerEmail= */ false,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact =
                new AutofillContact(
                        mActivity,
                        new AutofillProfile(sProfile),
                        null,
                        "+4900000000000",
                        null,
                        ContactEditor.COMPLETE,
                        false,
                        true,
                        false);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        editorFields.get(0).model.set(VALUE, "+490111111111");
        editorModel.get(DONE_RUNNABLE).run();

        assertNull(contact.getPayerName());
        assertEquals("+490111111111", contact.getPayerPhone());
        assertNull(contact.getPayerEmail());
        assertEquals("+490111111111", contact.getProfile().getPhoneNumber());
    }

    @Test
    @SmallTest
    public void editEmail_CommitChanges() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ false,
                        /* requestPayerPhone= */ false,
                        /* requestPayerEmail= */ true,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact =
                new AutofillContact(
                        mActivity,
                        new AutofillProfile(sProfile),
                        null,
                        null,
                        "example@gmail.com",
                        ContactEditor.COMPLETE,
                        false,
                        false,
                        true);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        editorFields.get(0).model.set(VALUE, "modified@gmail.com");
        editorModel.get(DONE_RUNNABLE).run();

        assertNull(contact.getPayerName());
        assertNull(contact.getPayerPhone());
        assertEquals("modified@gmail.com", contact.getPayerEmail());
        assertEquals("modified@gmail.com", contact.getProfile().getEmailAddress());
    }

    @Test
    @SmallTest
    public void editAllFields_CommitChanges() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ true,
                        /* requestPayerPhone= */ true,
                        /* requestPayerEmail= */ true,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact =
                new AutofillContact(
                        mActivity,
                        new AutofillProfile(sProfile),
                        "Payer name",
                        "+4900000000000",
                        "example@gmail.com",
                        ContactEditor.COMPLETE,
                        true,
                        true,
                        true);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(3, editorFields.size());
        editorFields.get(0).model.set(VALUE, "Modified name");
        editorFields.get(1).model.set(VALUE, "+490111111111");
        editorFields.get(2).model.set(VALUE, "modified@gmail.com");
        editorModel.get(DONE_RUNNABLE).run();

        assertEquals("Modified name", contact.getPayerName());
        assertEquals("+490111111111", contact.getPayerPhone());
        assertEquals("modified@gmail.com", contact.getPayerEmail());
        assertEquals("Modified name", contact.getProfile().getFullName());
        assertEquals("+490111111111", contact.getProfile().getPhoneNumber());
        assertEquals("modified@gmail.com", contact.getProfile().getEmailAddress());
    }

    @Test
    @SmallTest
    public void edit_CorrectContactInfo_NoErrors() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ true,
                        /* requestPayerPhone= */ true,
                        /* requestPayerEmail= */ true,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact =
                new AutofillContact(
                        mActivity,
                        new AutofillProfile(sProfile),
                        "Payer name",
                        "+4900000000000",
                        "example@gmail.com",
                        ContactEditor.COMPLETE,
                        true,
                        true,
                        true);
        editor.edit(contact, unused -> {});

        validateErrorMessages(editor.getEditorModelForTesting(), /* errorsPresent= */ false);
    }

    @Test
    @SmallTest
    public void edit_EditorErrorsSet_ErrorMessagesShown() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ true,
                        /* requestPayerPhone= */ true,
                        /* requestPayerEmail= */ true,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        PayerErrors errors = new PayerErrors();
        errors.email = "email error";
        errors.name = "name error";
        errors.phone = "phone error";
        editor.setPayerErrors(errors);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact =
                new AutofillContact(
                        mActivity,
                        new AutofillProfile(sProfile),
                        "Payer name",
                        "+4900000000000",
                        "example@gmail.com",
                        ContactEditor.COMPLETE,
                        true,
                        true,
                        true);
        editor.edit(contact, unused -> {});

        validateErrorMessages(editor.getEditorModelForTesting(), /* errorsPresent= */ true);
    }

    @Test
    @SmallTest
    public void edit_FieldsAreEmpty_ErrorMessagesShown() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ true,
                        /* requestPayerPhone= */ true,
                        /* requestPayerEmail= */ true,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact =
                new AutofillContact(
                        mActivity,
                        new AutofillProfile(sProfile),
                        "",
                        "",
                        "",
                        ContactEditor.COMPLETE,
                        true,
                        true,
                        true);
        editor.edit(contact, unused -> {});

        validateErrorMessages(editor.getEditorModelForTesting(), /* errorsPresent= */ true);
    }

    @Test
    @SmallTest
    public void edit_EmptyInputToFields_ErrorMessagesShown() {
        ContactEditor editor =
                new ContactEditor(
                        /* requestPayerName= */ true,
                        /* requestPayerPhone= */ true,
                        /* requestPayerEmail= */ true,
                        /* saveToDisk= */ false,
                        mPersonalDataManager);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact =
                new AutofillContact(
                        mActivity,
                        new AutofillProfile(sProfile),
                        "Payer name",
                        "+4900000000000",
                        "example@gmail.com",
                        ContactEditor.COMPLETE,
                        true,
                        true,
                        true);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<FieldItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(3, editorFields.size());
        editorFields.get(0).model.set(VALUE, "");
        editorFields.get(1).model.set(VALUE, "");
        editorFields.get(2).model.set(VALUE, "");
        editorModel.get(DONE_RUNNABLE).run();

        validateErrorMessages(editor.getEditorModelForTesting(), /* errorsPresent= */ true);
    }
}

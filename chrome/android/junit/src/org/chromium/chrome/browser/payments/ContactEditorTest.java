// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.CANCEL_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DONE_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.EDITOR_FIELDS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_FULL_LINE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.ItemType.TEXT_INPUT;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.SHOW_REQUIRED_INDICATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.LENGTH_COUNTER_LIMIT_NONE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_INPUT_TYPE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_LENGTH_COUNTER_LIMIT;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.After;
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
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.editors.EditorDialogView;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.TextInputType;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ContactEditor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContactEditorTest {
    private static final AutofillProfile sProfile = AutofillProfile.builder()
                                                            .setHonorificPrefix("Mr")
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

    @Mock
    private PersonalDataManager mPersonalDataManager;
    @Mock
    private EditorDialogView mEditorDialog;

    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PersonalDataManager.setInstanceForTesting(mPersonalDataManager);

        mActivity = Robolectric.setupActivity(TestActivity.class);

        when(mEditorDialog.getContext()).thenReturn(mActivity);
    }

    @After
    public void tearDown() {
        PersonalDataManager.setInstanceForTesting(null);
    }

    private static void validateTextField(
            ListItem fieldItem, String value, @TextInputType int textInputType, String label) {
        assertEquals(TEXT_INPUT, fieldItem.type);

        PropertyModel field = fieldItem.model;
        assertEquals(value, field.get(VALUE));
        assertEquals(textInputType, field.get(TEXT_INPUT_TYPE));
        assertEquals(label, field.get(LABEL));
        assertTrue(field.get(IS_REQUIRED));
        assertTrue(field.get(IS_FULL_LINE));
        assertEquals(LENGTH_COUNTER_LIMIT_NONE, field.get(TEXT_LENGTH_COUNTER_LIMIT));
    }

    @Test
    @SmallTest
    public void validateRequiredFieldIndicator() {
        ContactEditor editor = new ContactEditor(/*requestPayerName=*/true,
                /*requestPayerPhone=*/false,
                /*requestPayerEmail=*/false,
                /*saveToDisk=*/false);
        editor.setEditorDialog(mEditorDialog);
        editor.edit(null, unused -> {});

        assertNotNull(editor.getEditorModelForTesting());
        assertTrue(editor.getEditorModelForTesting().get(SHOW_REQUIRED_INDICATOR));
    }

    @Test
    @SmallTest
    public void requestName_NewContact() {
        ContactEditor editor = new ContactEditor(/*requestPayerName=*/true,
                /*requestPayerPhone=*/false,
                /*requestPayerEmail=*/false,
                /*saveToDisk=*/false);
        editor.setEditorDialog(mEditorDialog);
        editor.edit(null, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        validateTextField(editorFields.get(0), null, TextInputType.PERSON_NAME_INPUT,
                mActivity.getString(R.string.payments_name_field_in_contact_details));
    }

    @Test
    @SmallTest
    public void requestPhone_NewContact() {
        ContactEditor editor = new ContactEditor(/*requestPayerName=*/false,
                /*requestPayerPhone=*/true,
                /*requestPayerEmail=*/false,
                /*saveToDisk=*/false);
        editor.setEditorDialog(mEditorDialog);
        editor.edit(null, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        validateTextField(editorFields.get(0), null, TextInputType.PHONE_NUMBER_INPUT,
                mActivity.getString(R.string.autofill_profile_editor_phone_number));
    }

    @Test
    @SmallTest
    public void requestEmail_NewContact() {
        ContactEditor editor = new ContactEditor(/*requestPayerName=*/false,
                /*requestPayerPhone=*/false,
                /*requestPayerEmail=*/true,
                /*saveToDisk=*/false);
        editor.setEditorDialog(mEditorDialog);
        editor.edit(null, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        validateTextField(editorFields.get(0), null, TextInputType.EMAIL_ADDRESS_INPUT,
                mActivity.getString(R.string.autofill_profile_editor_email_address));
    }

    @Test
    @SmallTest
    public void requestAllFields_NewContact() {
        ContactEditor editor = new ContactEditor(/*requestPayerName=*/true,
                /*requestPayerPhone=*/true,
                /*requestPayerEmail=*/true,
                /*saveToDisk=*/false);
        editor.setEditorDialog(mEditorDialog);
        editor.edit(null, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(3, editorFields.size());
        validateTextField(editorFields.get(0), null, TextInputType.PERSON_NAME_INPUT,
                mActivity.getString(R.string.payments_name_field_in_contact_details));
        validateTextField(editorFields.get(1), null, TextInputType.PHONE_NUMBER_INPUT,
                mActivity.getString(R.string.autofill_profile_editor_phone_number));
        validateTextField(editorFields.get(2), null, TextInputType.EMAIL_ADDRESS_INPUT,
                mActivity.getString(R.string.autofill_profile_editor_email_address));
    }

    @Test
    @SmallTest
    public void requestName_ExistingContact() {
        ContactEditor editor = new ContactEditor(/*requestPayerName=*/true,
                /*requestPayerPhone=*/false,
                /*requestPayerEmail=*/false,
                /*saveToDisk=*/false);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact = new AutofillContact(mActivity, sProfile, "Payer name", null, null,
                ContactEditor.COMPLETE, true, false, false);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        validateTextField(editorFields.get(0), "Payer name", TextInputType.PERSON_NAME_INPUT,
                mActivity.getString(R.string.payments_name_field_in_contact_details));
    }

    @Test
    @SmallTest
    public void requestPhone_ExistingContact() {
        ContactEditor editor = new ContactEditor(/*requestPayerName=*/false,
                /*requestPayerPhone=*/true,
                /*requestPayerEmail=*/false,
                /*saveToDisk=*/false);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact = new AutofillContact(mActivity, sProfile, null, "Payer phone",
                null, ContactEditor.COMPLETE, false, true, false);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        validateTextField(editorFields.get(0), "Payer phone", TextInputType.PHONE_NUMBER_INPUT,
                mActivity.getString(R.string.autofill_profile_editor_phone_number));
    }

    @Test
    @SmallTest
    public void requestEmail_ExistingContact() {
        ContactEditor editor = new ContactEditor(/*requestPayerName=*/false,
                /*requestPayerPhone=*/false,
                /*requestPayerEmail=*/true,
                /*saveToDisk=*/false);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact = new AutofillContact(mActivity, sProfile, null, null,
                "Payer email", ContactEditor.COMPLETE, false, false, true);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        validateTextField(editorFields.get(0), "Payer email", TextInputType.EMAIL_ADDRESS_INPUT,
                mActivity.getString(R.string.autofill_profile_editor_email_address));
    }

    @Test
    @SmallTest
    public void requestAllFields_ExistingContact() {
        ContactEditor editor = new ContactEditor(/*requestPayerName=*/true,
                /*requestPayerPhone=*/true,
                /*requestPayerEmail=*/true,
                /*saveToDisk=*/false);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact = new AutofillContact(mActivity, sProfile, "Payer name",
                "Payer phone", "Payer email", ContactEditor.COMPLETE, true, true, true);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(3, editorFields.size());
        validateTextField(editorFields.get(0), "Payer name", TextInputType.PERSON_NAME_INPUT,
                mActivity.getString(R.string.payments_name_field_in_contact_details));
        validateTextField(editorFields.get(1), "Payer phone", TextInputType.PHONE_NUMBER_INPUT,
                mActivity.getString(R.string.autofill_profile_editor_phone_number));
        validateTextField(editorFields.get(2), "Payer email", TextInputType.EMAIL_ADDRESS_INPUT,
                mActivity.getString(R.string.autofill_profile_editor_email_address));
    }

    @Test
    @SmallTest
    public void editName_CancelEditing() {
        ContactEditor editor = new ContactEditor(/*requestPayerName=*/true,
                /*requestPayerPhone=*/false,
                /*requestPayerEmail=*/false,
                /*saveToDisk=*/false);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact = new AutofillContact(mActivity, new AutofillProfile(sProfile),
                "Payer name", null, null, ContactEditor.COMPLETE, true, false, false);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
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
        ContactEditor editor = new ContactEditor(/*requestPayerName=*/false,
                /*requestPayerPhone=*/true,
                /*requestPayerEmail=*/false,
                /*saveToDisk=*/false);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact = new AutofillContact(mActivity, new AutofillProfile(sProfile),
                null, "Payer phone", null, ContactEditor.COMPLETE, false, true, false);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
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
        ContactEditor editor = new ContactEditor(/*requestPayerName=*/false,
                /*requestPayerPhone=*/false,
                /*requestPayerEmail=*/true,
                /*saveToDisk=*/false);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact = new AutofillContact(mActivity, new AutofillProfile(sProfile),
                null, null, "Payer email", ContactEditor.COMPLETE, false, false, true);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
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
        ContactEditor editor = new ContactEditor(/*requestPayerName=*/true,
                /*requestPayerPhone=*/false,
                /*requestPayerEmail=*/false,
                /*saveToDisk=*/false);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact = new AutofillContact(mActivity, new AutofillProfile(sProfile),
                "Payer name", null, null, ContactEditor.COMPLETE, true, false, false);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
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
        ContactEditor editor = new ContactEditor(/*requestPayerName=*/false,
                /*requestPayerPhone=*/true,
                /*requestPayerEmail=*/false,
                /*saveToDisk=*/false);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact = new AutofillContact(mActivity, new AutofillProfile(sProfile),
                null, "Payer phone", null, ContactEditor.COMPLETE, false, true, false);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        editorFields.get(0).model.set(VALUE, "Modified phone");
        editorModel.get(DONE_RUNNABLE).run();

        assertNull(contact.getPayerName());
        assertEquals("Modified phone", contact.getPayerPhone());
        assertNull(contact.getPayerEmail());
        assertEquals("Modified phone", contact.getProfile().getPhoneNumber());
    }

    @Test
    @SmallTest
    public void editEmail_CommitChanges() {
        ContactEditor editor = new ContactEditor(/*requestPayerName=*/false,
                /*requestPayerPhone=*/false,
                /*requestPayerEmail=*/true,
                /*saveToDisk=*/false);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact = new AutofillContact(mActivity, new AutofillProfile(sProfile),
                null, null, "Payer email", ContactEditor.COMPLETE, false, false, true);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(1, editorFields.size());
        editorFields.get(0).model.set(VALUE, "Modified email");
        editorModel.get(DONE_RUNNABLE).run();

        assertNull(contact.getPayerName());
        assertNull(contact.getPayerPhone());
        assertEquals("Modified email", contact.getPayerEmail());
        assertEquals("Modified email", contact.getProfile().getEmailAddress());
    }

    @Test
    @SmallTest
    public void editAllFields_CommitChanges() {
        ContactEditor editor = new ContactEditor(/*requestPayerName=*/true,
                /*requestPayerPhone=*/true,
                /*requestPayerEmail=*/true,
                /*saveToDisk=*/false);
        editor.setEditorDialog(mEditorDialog);
        AutofillContact contact =
                new AutofillContact(mActivity, new AutofillProfile(sProfile), "Payer name",
                        "Payer phone", "Payer email", ContactEditor.COMPLETE, true, true, true);
        editor.edit(contact, unused -> {});

        PropertyModel editorModel = editor.getEditorModelForTesting();
        assertNotNull(editorModel);

        ListModel<ListItem> editorFields = editorModel.get(EDITOR_FIELDS);
        assertEquals(3, editorFields.size());
        editorFields.get(0).model.set(VALUE, "Modified name");
        editorFields.get(1).model.set(VALUE, "Modified phone");
        editorFields.get(2).model.set(VALUE, "Modified email");
        editorModel.get(DONE_RUNNABLE).run();

        assertEquals("Modified name", contact.getPayerName());
        assertEquals("Modified phone", contact.getPayerPhone());
        assertEquals("Modified email", contact.getPayerEmail());
        assertEquals("Modified name", contact.getProfile().getFullName());
        assertEquals("Modified phone", contact.getProfile().getPhoneNumber());
        assertEquals("Modified email", contact.getProfile().getEmailAddress());
    }
}

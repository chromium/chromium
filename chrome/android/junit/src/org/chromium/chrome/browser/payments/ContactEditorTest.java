// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.prefeditor.EditorDialog;
import org.chromium.chrome.browser.autofill.prefeditor.EditorModel;
import org.chromium.components.autofill.prefeditor.EditorFieldModel;
import org.chromium.ui.base.TestActivity;

import java.util.List;

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
    private EditorDialog mEditorDialog;

    @Captor
    private ArgumentCaptor<EditorModel> mEditorModelCapture;

    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PersonalDataManager.setInstanceForTesting(mPersonalDataManager);

        mActivity = Robolectric.setupActivity(TestActivity.class);

        when(mEditorDialog.getContext()).thenReturn(mActivity);
        doNothing().when(mEditorDialog).show(mEditorModelCapture.capture());
    }

    @After
    public void tearDown() {
        PersonalDataManager.setInstanceForTesting(null);
    }

    private static void validateTextField(
            EditorFieldModel field, String value, int inputTypeHint, String label) {
        assertTrue(field.isTextField());
        assertEquals(field.getValue(), value);
        assertEquals(inputTypeHint, field.getInputTypeHint());
        assertEquals(label, field.getLabel());
        assertTrue(field.isRequired());
        assertTrue(field.isFullLine());
        assertFalse(field.hasLengthCounter());
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

        EditorModel editorModel = mEditorModelCapture.getValue();
        assertNotNull(editorModel);

        List<EditorFieldModel> editorFields = editorModel.getFields();
        assertEquals(1, editorFields.size());
        validateTextField(editorFields.get(0), null, EditorFieldModel.INPUT_TYPE_HINT_PERSON_NAME,
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

        EditorModel editorModel = mEditorModelCapture.getValue();
        assertNotNull(editorModel);

        List<EditorFieldModel> editorFields = editorModel.getFields();
        assertEquals(1, editorFields.size());
        validateTextField(editorFields.get(0), null, EditorFieldModel.INPUT_TYPE_HINT_PHONE,
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

        EditorModel editorModel = mEditorModelCapture.getValue();
        assertNotNull(editorModel);

        List<EditorFieldModel> editorFields = editorModel.getFields();
        assertEquals(1, editorFields.size());
        validateTextField(editorFields.get(0), null, EditorFieldModel.INPUT_TYPE_HINT_EMAIL,
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

        EditorModel editorModel = mEditorModelCapture.getValue();
        assertNotNull(editorModel);

        List<EditorFieldModel> editorFields = editorModel.getFields();
        assertEquals(3, editorFields.size());
        validateTextField(editorFields.get(0), null, EditorFieldModel.INPUT_TYPE_HINT_PERSON_NAME,
                mActivity.getString(R.string.payments_name_field_in_contact_details));
        validateTextField(editorFields.get(1), null, EditorFieldModel.INPUT_TYPE_HINT_PHONE,
                mActivity.getString(R.string.autofill_profile_editor_phone_number));
        validateTextField(editorFields.get(2), null, EditorFieldModel.INPUT_TYPE_HINT_EMAIL,
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

        EditorModel editorModel = mEditorModelCapture.getValue();
        assertNotNull(editorModel);

        List<EditorFieldModel> editorFields = editorModel.getFields();
        assertEquals(1, editorFields.size());
        validateTextField(editorFields.get(0), "Payer name",
                EditorFieldModel.INPUT_TYPE_HINT_PERSON_NAME,
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

        EditorModel editorModel = mEditorModelCapture.getValue();
        assertNotNull(editorModel);

        List<EditorFieldModel> editorFields = editorModel.getFields();
        assertEquals(1, editorFields.size());
        validateTextField(editorFields.get(0), "Payer phone",
                EditorFieldModel.INPUT_TYPE_HINT_PHONE,
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

        EditorModel editorModel = mEditorModelCapture.getValue();
        assertNotNull(editorModel);

        List<EditorFieldModel> editorFields = editorModel.getFields();
        assertEquals(1, editorFields.size());
        validateTextField(editorFields.get(0), "Payer email",
                EditorFieldModel.INPUT_TYPE_HINT_EMAIL,
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

        EditorModel editorModel = mEditorModelCapture.getValue();
        assertNotNull(editorModel);

        List<EditorFieldModel> editorFields = editorModel.getFields();
        assertEquals(3, editorFields.size());
        validateTextField(editorFields.get(0), "Payer name",
                EditorFieldModel.INPUT_TYPE_HINT_PERSON_NAME,
                mActivity.getString(R.string.payments_name_field_in_contact_details));
        validateTextField(editorFields.get(1), "Payer phone",
                EditorFieldModel.INPUT_TYPE_HINT_PHONE,
                mActivity.getString(R.string.autofill_profile_editor_phone_number));
        validateTextField(editorFields.get(2), "Payer email",
                EditorFieldModel.INPUT_TYPE_HINT_EMAIL,
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

        EditorModel editorModel = mEditorModelCapture.getValue();
        assertNotNull(editorModel);

        List<EditorFieldModel> editorFields = editorModel.getFields();
        assertEquals(1, editorFields.size());
        editorFields.get(0).setValue("Modified name");
        editorModel.cancel();

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

        EditorModel editorModel = mEditorModelCapture.getValue();
        assertNotNull(editorModel);

        List<EditorFieldModel> editorFields = editorModel.getFields();
        assertEquals(1, editorFields.size());
        editorFields.get(0).setValue("Modified phone");
        editorModel.cancel();

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

        EditorModel editorModel = mEditorModelCapture.getValue();
        assertNotNull(editorModel);

        List<EditorFieldModel> editorFields = editorModel.getFields();
        assertEquals(1, editorFields.size());
        editorFields.get(0).setValue("Modified email");
        editorModel.cancel();

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

        EditorModel editorModel = mEditorModelCapture.getValue();
        assertNotNull(editorModel);

        List<EditorFieldModel> editorFields = editorModel.getFields();
        assertEquals(1, editorFields.size());
        editorFields.get(0).setValue("Modified name");
        editorModel.done();

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

        EditorModel editorModel = mEditorModelCapture.getValue();
        assertNotNull(editorModel);

        List<EditorFieldModel> editorFields = editorModel.getFields();
        assertEquals(1, editorFields.size());
        editorFields.get(0).setValue("Modified phone");
        editorModel.done();

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

        EditorModel editorModel = mEditorModelCapture.getValue();
        assertNotNull(editorModel);

        List<EditorFieldModel> editorFields = editorModel.getFields();
        assertEquals(1, editorFields.size());
        editorFields.get(0).setValue("Modified email");
        editorModel.done();

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

        EditorModel editorModel = mEditorModelCapture.getValue();
        assertNotNull(editorModel);

        List<EditorFieldModel> editorFields = editorModel.getFields();
        assertEquals(3, editorFields.size());
        editorFields.get(0).setValue("Modified name");
        editorFields.get(1).setValue("Modified phone");
        editorFields.get(2).setValue("Modified email");
        editorModel.done();

        assertEquals("Modified name", contact.getPayerName());
        assertEquals("Modified phone", contact.getPayerPhone());
        assertEquals("Modified email", contact.getPayerEmail());
        assertEquals("Modified name", contact.getProfile().getFullName());
        assertEquals("Modified phone", contact.getProfile().getPhoneNumber());
        assertEquals("Modified email", contact.getProfile().getEmailAddress());
    }
}

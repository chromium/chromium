// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.autofill.prefeditor.EditorDialog;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.ContactEditor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill_assistant.AssistantAutofillProfile;
import org.chromium.components.autofill_assistant.AssistantEditor.AssistantContactEditor;
import org.chromium.components.autofill_assistant.AssistantOptionModel.ContactModel;
import org.chromium.content_public.browser.WebContents;

/**
 * Editor for contact information in Chrome using Autofill as a base.
 */
class AssistantContactEditorAutofill implements AssistantContactEditor {
    private final ContactEditor mEditor;
    private final Context mContext;

    public AssistantContactEditorAutofill(WebContents webContents, Activity activity,
            boolean requestName, boolean requestPhone, boolean requestEmail,
            boolean shouldStoreChanges) {
        mEditor = new ContactEditor(requestName, requestPhone, requestEmail,
                /* saveToDisk= */ shouldStoreChanges);
        mEditor.setEditorDialog(new EditorDialog(activity,
                /*deleteRunnable =*/null, Profile.fromWebContents(webContents)));
        mContext = activity;
    }

    @Override
    public void createOrEditItem(@Nullable ContactModel oldItem,
            Callback<ContactModel> doneCallback, Callback<ContactModel> cancelCallback) {
        @Nullable
        AutofillContact contact = oldItem == null
                ? null
                : AssistantAutofillUtilChrome.assistantAutofillProfileToAutofillContact(
                        oldItem.mOption, mContext, mEditor);

        Callback<AutofillContact> editorDoneCallback = editedContact -> {
            assert (editedContact != null && editedContact.isComplete()
                    && editedContact.getProfile() != null);
            doneCallback.onResult(new ContactModel(
                    AssistantAutofillUtilChrome.autofillProfileToAssistantAutofillProfile(
                            editedContact.getProfile())));
        };

        Callback<AutofillContact> editorCancelCallback =
                editedContact -> cancelCallback.onResult(oldItem);

        mEditor.edit(contact, editorDoneCallback, editorCancelCallback);
    }

    /**
     * Adds the contact's information to the editor such that it can be auto-completed while typing.
     *
     * @param contact The {@link AssistantAutofillProfile} to add information for.
     */
    @Override
    public void addContactInformationForAutocomplete(AssistantAutofillProfile contact) {
        mEditor.addEmailAddressIfValid(contact.getEmailAddress());
        mEditor.addPayerNameIfValid(contact.getFullName());
        mEditor.addPhoneNumberIfValid(contact.getPhoneNumber());
    }
}

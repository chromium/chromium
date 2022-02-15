// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.autofill_assistant.AssistantEditor.AssistantContactEditor;
import org.chromium.chrome.browser.autofill_assistant.AssistantOptionModel.ContactModel;
import org.chromium.chrome.browser.autofill_assistant.user_data.GmsIntegrator;

/**
 * Editor for contact information in Chrome/WebLayer using a GMS intent.
 */
public class AssistantContactEditorAccount implements AssistantContactEditor {
    private GmsIntegrator mGmsIntegrator;

    @Override
    public void createOrEditItem(ContactModel oldItem, Callback<ContactModel> doneCallback,
            Callback<ContactModel> cancelCallback) {
        // TODO(b/211748133)
    }

    @Override
    public void addContactInformationForAutocomplete(AssistantAutofillProfile contact) {
        // TODO(b/211748133)
    }
}

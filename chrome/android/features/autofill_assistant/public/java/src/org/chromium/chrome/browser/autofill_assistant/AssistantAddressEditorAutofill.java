// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.prefeditor.EditorDialog;
import org.chromium.chrome.browser.autofill.settings.AddressEditor;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill_assistant.AssistantEditor.AssistantAddressEditor;
import org.chromium.components.autofill_assistant.AssistantOptionModel.AddressModel;
import org.chromium.content_public.browser.WebContents;

/**
 * Editor for addresses in Chrome using Autofill as a base.
 */
public class AssistantAddressEditorAutofill implements AssistantAddressEditor {
    private final AddressEditor mEditor;
    private final Context mContext;

    public AssistantAddressEditorAutofill(
            WebContents webContents, Activity activity, boolean shouldStoreChanges) {
        mEditor = new AddressEditor(AddressEditor.Purpose.AUTOFILL_ASSISTANT,
                /* saveToDisk= */ shouldStoreChanges);
        mEditor.setEditorDialog(new EditorDialog(activity,
                /* deleteRunnable= */ null, Profile.fromWebContents(webContents)));
        mContext = activity;
    }

    @Override
    public void createOrEditItem(@Nullable AddressModel oldItem,
            Callback<AddressModel> doneCallback, Callback<AddressModel> cancelCallback) {
        @Nullable
        AutofillAddress address = oldItem == null
                ? null
                : AssistantAutofillUtilChrome.assistantAutofillProfileToAutofillAddress(
                        oldItem.mOption, mContext);

        Callback<AutofillAddress> editorDoneCallback = editedAddress -> {
            assert (editedAddress != null && editedAddress.isComplete()
                    && editedAddress.getProfile() != null);
            String fullDescription = PersonalDataManager.getInstance()
                                             .getShippingAddressLabelWithCountryForPaymentRequest(
                                                     editedAddress.getProfile());
            String summaryDescription =
                    PersonalDataManager.getInstance()
                            .getShippingAddressLabelWithoutCountryForPaymentRequest(
                                    editedAddress.getProfile());
            doneCallback.onResult(new AddressModel(
                    AssistantAutofillUtilChrome.autofillProfileToAssistantAutofillProfile(
                            editedAddress.getProfile()),
                    fullDescription, summaryDescription));
        };

        Callback<AutofillAddress> editorCancelCallback =
                editedAddress -> cancelCallback.onResult(oldItem);

        mEditor.edit(address, editorDoneCallback, editorCancelCallback);
    }
}

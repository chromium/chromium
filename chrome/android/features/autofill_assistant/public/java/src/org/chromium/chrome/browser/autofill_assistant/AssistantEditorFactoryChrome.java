// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;

import org.chromium.components.autofill_assistant.AssistantAddressEditorGms;
import org.chromium.components.autofill_assistant.AssistantContactEditorAccount;
import org.chromium.components.autofill_assistant.AssistantEditor.AssistantAddressEditor;
import org.chromium.components.autofill_assistant.AssistantEditor.AssistantContactEditor;
import org.chromium.components.autofill_assistant.AssistantEditor.AssistantPaymentInstrumentEditor;
import org.chromium.components.autofill_assistant.AssistantEditorFactory;
import org.chromium.components.autofill_assistant.AssistantPaymentInstrumentEditorGms;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * Factory to create editors within Chrome.
 */
public class AssistantEditorFactoryChrome implements AssistantEditorFactory {
    @Override
    public AssistantContactEditor createContactEditor(WebContents webContents, Activity activity,
            boolean requestName, boolean requestPhone, boolean requestEmail,
            boolean shouldStoreChanges) {
        return new AssistantContactEditorAutofill(
                webContents, activity, requestName, requestPhone, requestEmail, shouldStoreChanges);
    }

    @Override
    public AssistantContactEditor createAccountEditor(Activity activity,
            WindowAndroid windowAndroid, String accountEmail, boolean requestEmail,
            boolean requestPhone) {
        return new AssistantContactEditorAccount(
                activity, windowAndroid, accountEmail, requestEmail, requestPhone);
    }

    @Override
    public AssistantAddressEditor createAddressEditor(
            WebContents webContents, Activity activity, boolean shouldStoreChanges) {
        return new AssistantAddressEditorAutofill(webContents, activity, shouldStoreChanges);
    }

    @Override
    public AssistantAddressEditor createGmsAddressEditor(Activity activity,
            WindowAndroid windowAndroid, String accountEmail,
            byte[] initializeAddressCollectionParams) {
        return new AssistantAddressEditorGms(
                activity, windowAndroid, accountEmail, initializeAddressCollectionParams);
    }

    @Override
    public AssistantPaymentInstrumentEditor createPaymentInstrumentEditor(WebContents webContents,
            Activity activity, List<String> supportedCardNetworks, boolean shouldStoreChanges) {
        return new AssistantPaymentInstrumentEditorAutofill(
                webContents, activity, supportedCardNetworks, shouldStoreChanges);
    }

    @Override
    public AssistantPaymentInstrumentEditor createGmsPaymentInstrumentEditor(Activity activity,
            WindowAndroid windowAndroid, String accountEmail, byte[] addInstrumentactionToken) {
        return new AssistantPaymentInstrumentEditorGms(
                activity, windowAndroid, accountEmail, addInstrumentactionToken);
    }
}

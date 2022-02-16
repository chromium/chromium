// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;

import org.chromium.chrome.browser.autofill_assistant.AssistantEditor.AssistantAddressEditor;
import org.chromium.chrome.browser.autofill_assistant.AssistantEditor.AssistantContactEditor;
import org.chromium.chrome.browser.autofill_assistant.AssistantEditor.AssistantPaymentInstrumentEditor;
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
            WindowAndroid windowAndroid, String accountEmail, boolean requestEmail) {
        return new AssistantContactEditorAccount(
                activity, windowAndroid, accountEmail, requestEmail);
    }

    @Override
    public AssistantAddressEditor createAddressEditor(
            WebContents webContents, Activity activity, boolean shouldStoreChanges) {
        return new AssistantAddressEditorAutofill(webContents, activity, shouldStoreChanges);
    }

    @Override
    public AssistantPaymentInstrumentEditor createPaymentInstrumentEditor(WebContents webContents,
            Activity activity, List<String> supportedCardNetworks, boolean shouldStoreChanges) {
        return new AssistantPaymentInstrumentEditorAutofill(
                webContents, activity, supportedCardNetworks, shouldStoreChanges);
    }
}

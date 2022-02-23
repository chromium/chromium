// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.components.browser_ui.settings.SettingsLauncher;

/**
 * Controller for the dialog shown for the Privacy Sandbox.
 */
public class PrivacySandboxDialogController {
    /**
     * Launches an appropriate dialog if necessary and returns whether that happened.
     */
    public static boolean maybeLaunchPrivacySandboxDialog(
            Context context, @NonNull SettingsLauncher settingsLauncher, boolean isIncognito) {
        if (isIncognito) {
            return false;
        }
        @DialogType
        int dialogType = PrivacySandboxBridge.getRequiredDialogType();
        switch (dialogType) {
            case DialogType.NONE:
                return false;
            case DialogType.NOTICE:
                new PrivacySandboxDialogNotice(context, settingsLauncher).show();
                return true;
            case DialogType.CONSENT:
                new PrivacySandboxDialogConsent(context).show();
                return true;
            default:
                assert false : "Unknown DialogType value.";
                // Should not be reached.
                return false;
        }
    }
}

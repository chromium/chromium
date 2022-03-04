// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.app.Dialog;
import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.settings.SettingsLauncher;

import java.lang.ref.WeakReference;

/**
 * Controller for the dialog shown for the Privacy Sandbox.
 */
public class PrivacySandboxDialogController {
    private static WeakReference<Dialog> sDialog;

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
        Dialog dialog = null;
        switch (dialogType) {
            case DialogType.NONE:
                return false;
            case DialogType.NOTICE:
                dialog = new PrivacySandboxDialogNotice(context, settingsLauncher);
                dialog.show();
                sDialog = new WeakReference<>(dialog);
                return true;
            case DialogType.CONSENT:
                dialog = new PrivacySandboxDialogConsent(context);
                dialog.show();
                sDialog = new WeakReference<>(dialog);
                return true;
            default:
                assert false : "Unknown DialogType value.";
                // Should not be reached.
                return false;
        }
    }

    @VisibleForTesting
    static Dialog getDialogForTesting() {
        return sDialog != null ? sDialog.get() : null;
    }
}

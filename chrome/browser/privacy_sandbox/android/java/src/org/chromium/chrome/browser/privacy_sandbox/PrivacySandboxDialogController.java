// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.app.Dialog;
import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

import java.lang.ref.WeakReference;

/**
 * Controller for the dialog shown for the Privacy Sandbox.
 */
public class PrivacySandboxDialogController {
    private static WeakReference<Dialog> sDialog;
    private static Boolean sShowNew;
    private static Boolean sDisableAnimations;

    /**
     * Launches an appropriate dialog if necessary and returns whether that happened.
     */
    public static boolean maybeLaunchPrivacySandboxDialog(Context context,
            @NonNull SettingsLauncher settingsLauncher, boolean isIncognito,
            @Nullable BottomSheetController bottomSheetController) {
        if (isIncognito) {
            return false;
        }
        @PromptType
        int promptType = PrivacySandboxBridge.getRequiredPromptType();
        Dialog dialog = null;
        switch (promptType) {
            case PromptType.NONE:
                return false;
            case PromptType.NOTICE:
                if (bottomSheetController == null || !showNewNotice()) return false;
                new PrivacySandboxBottomSheetNotice(
                        context, bottomSheetController, settingsLauncher)
                        .showNotice(/*animate = */ sDisableAnimations == null);
                return true;
            case PromptType.CONSENT:
                dialog = new PrivacySandboxDialogConsent(context);
                dialog.show();
                sDialog = new WeakReference<>(dialog);
                return true;
            default:
                assert false : "Unknown PromptType value.";
                // Should not be reached.
                return false;
        }
    }

    static boolean showNewNotice() {
        // Unless overridden for testing, a new notice should always be shown.
        // TODO(crbug.com/1375230) Remove this code path if the ability to
        // differentiate notice types is no longer required.
        return (sShowNew != null) ? sShowNew : true;
    }

    @VisibleForTesting
    static Dialog getDialogForTesting() {
        return sDialog != null ? sDialog.get() : null;
    }

    @VisibleForTesting
    static void resetShowNewNoticeForTesting() {
        sShowNew = null;
    }

    @VisibleForTesting
    static void setShowNewNoticeForTesting(boolean showNew) {
        sShowNew = showNew;
    }

    @VisibleForTesting
    static void disableAnimationsForTesting() {
        sDisableAnimations = true;
    }
}

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.app.Dialog;
import android.content.Context;
import android.view.LayoutInflater;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
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
    public static boolean maybeLaunchPrivacySandboxDialog(Context context,
            @NonNull SettingsLauncher settingsLauncher, boolean isIncognito,
            BottomSheetController bottomSheetController) {
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
                if (showNewNotice()) {
                    if (bottomSheetController == null) return false;
                    PrivacySandboxBottomSheetNotice bottomSheet =
                            new PrivacySandboxBottomSheetNotice(
                                    LayoutInflater.from(context).inflate(
                                            R.layout.privacy_sandbox_notice_bottom_sheet, null));
                    bottomSheetController.requestShowContent(bottomSheet, /* animate= */ true);
                } else {
                    dialog = new PrivacySandboxDialogNotice(context, settingsLauncher);
                    dialog.show();
                    sDialog = new WeakReference<>(dialog);
                }
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
        // Must match privacy_sandbox::kPrivacySandboxSettings3NewNotice.
        final String newNoticeParam = "new-notice";
        // Must match the default value for this param.
        final boolean newNoticeParamDefault = false;

        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3, newNoticeParam,
                newNoticeParamDefault);
    }

    @VisibleForTesting
    static Dialog getDialogForTesting() {
        return sDialog != null ? sDialog.get() : null;
    }
}

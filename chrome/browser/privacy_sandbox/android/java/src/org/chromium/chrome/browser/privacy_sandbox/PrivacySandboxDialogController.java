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
    // TODO(crbug.com/1330704): This variable and its usage can be removed when the PrivacySandbox
    // promo logic will be decoupled from the NewTabPage.
    private static boolean sNewNoticeShownInCurrentSession;

    /**
     * Launches an appropriate dialog if necessary and returns whether that happened.
     */
    public static boolean maybeLaunchPrivacySandboxDialog(
            @PrivacySandboxDialogLaunchContext int launchContext, Context context,
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
                boolean newNotice = showNewNotice();
                if (launchContext == PrivacySandboxDialogLaunchContext.NEW_TAB_PAGE && newNotice) {
                    // Invoked in the NTP context and the new notice should be shown; show it.
                    if (bottomSheetController == null) return false;
                    new PrivacySandboxBottomSheetNotice(
                            context, bottomSheetController, settingsLauncher)
                            .showNotice(/*animate = */ sDisableAnimations == null);
                    sNewNoticeShownInCurrentSession = true;
                } else if (launchContext == PrivacySandboxDialogLaunchContext.BROWSER_START
                        && !newNotice) {
                    // Invoked at browser start without the new notice; show it.
                    dialog = new PrivacySandboxDialogNotice(context, settingsLauncher);
                    dialog.show();
                    sDialog = new WeakReference<>(dialog);
                } else {
                    // The launch context doesn't match the notice type; do not show anything.
                    return false;
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
        // Unless overridden for testing, a new notice should always be shown.
        // TODO(crbug.com/1375230) Remove this code path if the ability to
        // differentiate notice types is no longer required.
        return (sShowNew != null) ? sShowNew : true;
    }

    /**
     * Returns true if the new notice has already been shown in the current session.
     */
    public static boolean hasNewNoticeBeenShownInCurrentSession() {
        return sNewNoticeShownInCurrentSession;
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

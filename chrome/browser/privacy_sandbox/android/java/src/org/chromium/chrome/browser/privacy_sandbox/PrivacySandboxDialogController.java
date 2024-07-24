// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.app.Dialog;
import android.content.Context;

import org.chromium.chrome.browser.profiles.Profile;

import java.lang.ref.WeakReference;

/** Controller for the dialog shown for the Privacy Sandbox. */
public class PrivacySandboxDialogController {
    private static WeakReference<Dialog> sDialog;
    private static boolean sDisableAnimations;
    private static boolean sDisableEEANoticeForTesting;

    public static boolean shouldShowPrivacySandboxDialog(Profile profile) {
        assert profile != null;
        if (profile.isOffTheRecord()) {
            return false;
        }
        @PromptType int promptType = new PrivacySandboxBridge(profile).getRequiredPromptType();
        if (promptType != PromptType.M1_CONSENT
                && promptType != PromptType.M1_NOTICE_EEA
                && promptType != PromptType.M1_NOTICE_ROW
                && promptType != PromptType.M1_NOTICE_RESTRICTED) {
            return false;
        }
        return true;
    }

    /** Launches an appropriate dialog if necessary and returns whether that happened. */
    public static boolean maybeLaunchPrivacySandboxDialog(Context context, Profile profile) {
        assert profile != null;
        if (profile.isOffTheRecord()) {
            return false;
        }
        PrivacySandboxBridge privacySandboxBridge = new PrivacySandboxBridge(profile);
        @PromptType int promptType = privacySandboxBridge.getRequiredPromptType();
        Dialog dialog = null;
        switch (promptType) {
            case PromptType.NONE:
                return false;
            case PromptType.M1_CONSENT:
                dialog =
                        new PrivacySandboxDialogConsentEEA(
                                context, privacySandboxBridge, sDisableAnimations);
                dialog.show();
                sDialog = new WeakReference<>(dialog);
                return true;
            case PromptType.M1_NOTICE_EEA:
                showNoticeEEA(context, privacySandboxBridge);
                return true;
            case PromptType.M1_NOTICE_ROW:
                dialog = new PrivacySandboxDialogNoticeROW(context, privacySandboxBridge);
                dialog.show();
                sDialog = new WeakReference<>(dialog);
                return true;
            case PromptType.M1_NOTICE_RESTRICTED:
                dialog = new PrivacySandboxDialogNoticeRestricted(context, privacySandboxBridge);
                dialog.show();
                sDialog = new WeakReference<>(dialog);
                return true;
            default:
                assert false : "Unknown PromptType value.";
                // Should not be reached.
                return false;
        }
    }

    /** Shows the NoticeEEA dialog. */
    public static void showNoticeEEA(Context context, PrivacySandboxBridge privacySandboxBridge) {
        if (!sDisableEEANoticeForTesting) {
            Dialog dialog;
            dialog = new PrivacySandboxDialogNoticeEEA(context, privacySandboxBridge);
            dialog.show();
            sDialog = new WeakReference<>(dialog);
        }
    }

    static Dialog getDialogForTesting() {
        return sDialog != null ? sDialog.get() : null;
    }

    static void disableAnimationsForTesting(boolean disable) {
        sDisableAnimations = disable;
    }

    static void disableEEANoticeForTesting(boolean disable) {
        sDisableEEANoticeForTesting = disable;
    }
}

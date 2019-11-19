// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import android.os.SystemClock;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.permissions.PermissionDialogController;
import org.chromium.chrome.browser.vr.ArConsentDialog;
import org.chromium.chrome.browser.vr.VrConsentDialog;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Utility class for interacting with permission prompts outside of the VR Browser. For interaction
 * in the VR Browser, see NativeUiUtils.
 */
public class PermissionUtils {
    public static final long DIALOG_POLLING_INTERVAL_MS = 250;
    /**
     * Blocks until a permission prompt appears.
     */
    public static void waitForPermissionPrompt() {
        CriteriaHelper.pollUiThread(() -> {
            return PermissionDialogController.getInstance().isDialogShownForTest();
        }, "Permission prompt did not appear in allotted time");
    }

    /**
     * Accepts the currently displayed permission prompt.
     */
    public static void acceptPermissionPrompt() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PermissionDialogController.getInstance().clickButtonForTest(
                    ModalDialogProperties.ButtonType.POSITIVE);
        });
    }

    /**
     * Denies the currently displayed permission prompt.
     */
    public static void denyPermissionPrompt() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PermissionDialogController.getInstance().clickButtonForTest(
                    ModalDialogProperties.ButtonType.NEGATIVE);
        });
    }

    /**
     * Blocks until the session consent prompt appears.
     */
    public static void waitForConsentPrompt(ChromeActivity activity) {
        CriteriaHelper.pollUiThread(()
                                            -> { return isConsentDialogShown(activity); },
                "Consent prompt did not appear in allotted time",
                CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL, DIALOG_POLLING_INTERVAL_MS);
    }

    /**
     * Blocks until the consent prompt is dismissed.
     */
    public static void waitForConsentPromptDismissal(ChromeActivity activity) {
        CriteriaHelper.pollUiThread(()
                                            -> { return !isConsentDialogShown(activity); },
                "Consent prompt did not get dismissed in allotted time",
                CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL, DIALOG_POLLING_INTERVAL_MS);
        // For some reason, the prompt can be dismissed, but we won't be able to show a new one for
        // a short period of time (animations?), so wait a little longer to avoid that.
        SystemClock.sleep(500);
    }

    /**
     * Accepts the currently displayed session consent prompt.
     */
    public static void acceptConsentPrompt(ChromeActivity activity) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            clickConsentDialogButton(activity, ModalDialogProperties.ButtonType.POSITIVE);
        });
    }

    /**
     * Declines the currently displayed session consent prompt.
     */
    public static void declineConsentPrompt(ChromeActivity activity) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            clickConsentDialogButton(activity, ModalDialogProperties.ButtonType.NEGATIVE);
        });
    }

    /**
     * Helper function to check if the consent dialog is being shown.
     */
    public static boolean isConsentDialogShown(ChromeActivity activity) {
        ModalDialogManager manager = activity.getModalDialogManager();
        PropertyModel model = manager.getCurrentDialogForTest();
        if (model == null) return false;
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);
        return controller instanceof ArConsentDialog || controller instanceof VrConsentDialog;
    }

    /**
     * Helper function to click a button in the consent dialog.
     */
    public static void clickConsentDialogButton(ChromeActivity activity, int buttonType) {
        ModalDialogManager manager = activity.getModalDialogManager();
        PropertyModel model = manager.getCurrentDialogForTest();
        ModalDialogProperties.Controller dialog =
                (ModalDialogProperties.Controller) (model.get(ModalDialogProperties.CONTROLLER));
        dialog.onClick(model, buttonType);
    }
}

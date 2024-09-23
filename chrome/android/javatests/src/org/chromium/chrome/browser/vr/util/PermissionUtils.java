// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.ui.modaldialog.ModalDialogProperties;

/**
 * Utility class for interacting with permission prompts outside of the VR Browser. For interaction
 * in the VR Browser, see NativeUiUtils.
 */
public class PermissionUtils {
    public static final long DIALOG_POLLING_INTERVAL_MS = 250;

    /** Blocks until a permission prompt appears. */
    public static void waitForPermissionPrompt() {
        CriteriaHelper.pollUiThread(
                () -> {
                    return PermissionDialogController.getInstance().isDialogShownForTest();
                },
                "Permission prompt did not appear in allotted time");
    }

    /** Blocks until the consent prompt is dismissed. */
    public static void waitForPermissionPromptDismissal() {
        CriteriaHelper.pollUiThread(
                () -> {
                    return !PermissionDialogController.getInstance().isDialogShownForTest();
                },
                "Consent prompt did not get dismissed in allotted time",
                CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL,
                DIALOG_POLLING_INTERVAL_MS);
    }

    /** Accepts the currently displayed permission prompt. */
    public static void acceptPermissionPrompt() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PermissionDialogController.getInstance()
                            .clickButtonForTest(ModalDialogProperties.ButtonType.POSITIVE);
                });
    }

    /** Denies the currently displayed permission prompt. */
    public static void denyPermissionPrompt() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PermissionDialogController.getInstance()
                            .clickButtonForTest(ModalDialogProperties.ButtonType.NEGATIVE);
                });
    }
}

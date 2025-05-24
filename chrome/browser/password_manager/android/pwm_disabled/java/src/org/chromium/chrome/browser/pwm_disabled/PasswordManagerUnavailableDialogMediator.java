// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator for the dialog displayed when the password manager is permanently unavailable for a
 * user. It handles interactions with the UI.
 */
@NullMarked
class PasswordManagerUnavailableDialogMediator implements ModalDialogProperties.Controller {
    private final ModalDialogManager mModalDialogManager;
    private final Runnable mLaunchGmsUpdate;
    private final boolean mIsUpdateDialog;

    public PasswordManagerUnavailableDialogMediator(
            ModalDialogManager modalDialogManager,
            Runnable launchGmsUpdate,
            boolean isUpdateDialog) {
        mModalDialogManager = modalDialogManager;
        mLaunchGmsUpdate = launchGmsUpdate;
        mIsUpdateDialog = isUpdateDialog;
        if (!isUpdateDialog) {
            PwmDeprecationDialogsMetricsRecorder.recordNoGmsNoPasswordsDialogShown();
        }
    }

    private void runPositiveButtonCallback() {
        if (mIsUpdateDialog) {
            mLaunchGmsUpdate.run();
        }
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ButtonType.POSITIVE) {
            runPositiveButtonCallback();
        }
        mModalDialogManager.dismissDialog(
                model,
                buttonType == ButtonType.POSITIVE
                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        if (!mIsUpdateDialog) {
            return;
        }
        PwmDeprecationDialogsMetricsRecorder.recordOldGmsNoPasswordsDialogDismissalReason(
                dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }
}

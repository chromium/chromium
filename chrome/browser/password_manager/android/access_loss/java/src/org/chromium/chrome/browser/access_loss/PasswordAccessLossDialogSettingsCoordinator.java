// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import androidx.annotation.NonNull;

import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Shows the warning to the user explaining why they are not able to access Google Password Manager.
 */
public class PasswordAccessLossDialogSettingsCoordinator {
    public void showPasswordAccessLossDialog(
            @NonNull ModalDialogManager modalDialogManager,
            @PasswordAccessLossWarningType int warningType) {
        // TODO(b/353285038): implement the modal dialog.
    }
}

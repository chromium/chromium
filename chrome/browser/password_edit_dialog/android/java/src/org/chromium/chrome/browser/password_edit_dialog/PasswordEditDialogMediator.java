// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import android.content.res.Resources;

import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the logic for save & update password edit dialog.
 * Handles models updates and reacts to UI events.
 */
class PasswordEditDialogMediator implements ModalDialogProperties.Controller {
    private PropertyModel mDialogViewModel;
    private PropertyModel mModalDialogModel;

    private final ModalDialogManager mModalDialogManager;
    private final Resources mResources;
    private final PasswordEditDialogCoordinator.Delegate mDialogInteractions;

    PasswordEditDialogMediator(ModalDialogManager modalDialogManager, Resources resources,
            PasswordEditDialogCoordinator.Delegate dialogInteractions) {
        mModalDialogManager = modalDialogManager;
        mResources = resources;
        mDialogInteractions = dialogInteractions;
    }

    void initialize(PropertyModel dialogViewModel, PropertyModel modalDialogModel) {
        mDialogViewModel = dialogViewModel;
        mModalDialogModel = modalDialogModel;
    }

    /**
     * Updates model's username when it's changed in UI.
     *
     * @param username Username typed by user
     */
    void handleUsernameChanged(String username) {
        mDialogViewModel.set(PasswordEditDialogProperties.USERNAME, username);
    }

    /**
     * Updates model's password when it's changed in UI.
     *
     * @param password Password typed by user
     */
    void handlePasswordChanged(String password) {
        mDialogViewModel.set(PasswordEditDialogProperties.PASSWORD, password);
        boolean isPasswordInvalid = password.isEmpty();
        mDialogViewModel.set(PasswordEditDialogProperties.PASSWORD_ERROR,
                isPasswordInvalid
                        ? mResources.getString(R.string.password_entry_edit_empty_password_error)
                        : null);
        mModalDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, isPasswordInvalid);
    }

    // ModalDialogProperties.Controller implementation.
    @Override
    public void onClick(PropertyModel model, @ButtonType int buttonType) {
        if (buttonType == ButtonType.POSITIVE) {
            mDialogInteractions.onDialogAccepted(
                    mDialogViewModel.get(PasswordEditDialogProperties.USERNAME),
                    mDialogViewModel.get(PasswordEditDialogProperties.PASSWORD));
        }
        mModalDialogManager.dismissDialog(model,
                buttonType == ButtonType.POSITIVE ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                                  : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        mDialogInteractions.onDialogDismissed(
                dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }
}

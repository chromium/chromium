// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import android.content.res.Resources;

import androidx.annotation.StringRes;

import org.chromium.build.BuildConfig;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Contains the logic for save & update password edit dialog. Handles models updates and reacts to
 * UI events.
 */
class PasswordEditDialogMediator implements ModalDialogProperties.Controller {
    private PropertyModel mDialogViewModel;
    private PropertyModel mModalDialogModel;
    private List<String> mSavedUsernames;
    private String mAccount;

    private final ModalDialogManager mModalDialogManager;
    private final Resources mResources;
    private final PasswordEditDialogCoordinator.Delegate mDialogInteractions;

    PasswordEditDialogMediator(
            ModalDialogManager modalDialogManager,
            Resources resources,
            PasswordEditDialogCoordinator.Delegate dialogInteractions) {
        mModalDialogManager = modalDialogManager;
        mResources = resources;
        mDialogInteractions = dialogInteractions;
    }

    void initialize(
            PropertyModel dialogViewModel,
            PropertyModel modalDialogModel,
            List<String> savedUsernames,
            String account) {
        mDialogViewModel = dialogViewModel;
        mModalDialogModel = modalDialogModel;
        mSavedUsernames = savedUsernames;
        mAccount = account;
    }

    /**
     * Updates model's username when it's changed in UI.
     *
     * @param username Username typed by user
     */
    void handleUsernameChanged(String username) {
        mDialogViewModel.set(PasswordEditDialogProperties.USERNAME, username);
        mDialogViewModel.set(
                PasswordEditDialogProperties.FOOTER,
                createEditPasswordDialogFooter(
                        mAccount, mDialogInteractions.isUsingAccountStorage(username), mResources));
        mModalDialogModel.set(
                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                isUpdate(mSavedUsernames, username)
                        ? mResources.getString(R.string.password_manager_update_button)
                        : mResources.getString(R.string.password_manager_save_button));
    }

    /**
     * Updates model's password when it's changed in UI.
     *
     * @param password Password typed by user
     */
    void handlePasswordChanged(String password) {
        mDialogViewModel.set(PasswordEditDialogProperties.PASSWORD, password);
        boolean isPasswordInvalid = password.isEmpty();
        mDialogViewModel.set(
                PasswordEditDialogProperties.PASSWORD_ERROR,
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
        mModalDialogManager.dismissDialog(
                model,
                buttonType == ButtonType.POSITIVE
                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        mDialogInteractions.onDialogDismissed(
                dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    /**
     * Chooses which title to show: Save password?/Update password?/Confirm your username
     *
     * @param savedUsernames the usernames stored in the Password Manager for this site
     * @param displayUsernames the usernames listed in the username dropdown
     * @param username entered in the text edit box by the user
     * @return title of the password edit dialog dialog
     */
    public static @StringRes int getTitle(
            List<String> savedUsernames, List<String> displayUsernames, String username) {
        if (isUpdate(savedUsernames, username)) {
            // If there is more than one username possible,
            // the user is asked to confirm the one to be saved.
            // Otherwise, they are just asked if they want to update the password.
            // TODO(crbug.com/40243989): Take care that confirm username dialog should
            // not be navigated through the cog button.
            return displayUsernames.size() < 2
                    ? R.string.password_update_dialog_title
                    : R.string.confirm_username_dialog_title;
        }
        return R.string.save_password;
    }

    public static String createEditPasswordDialogFooter(
            String account, boolean isUsingAccountStorage, Resources resources) {
        @StringRes int footerId;
        if (isUsingAccountStorage) {
            footerId = R.string.password_edit_dialog_synced_footer_google;
        } else {
            footerId =
                    BuildConfig.IS_CHROME_BRANDED
                            ? R.string.password_edit_dialog_unsynced_footer_google
                            : R.string.password_edit_dialog_unsynced_footer;
        }
        return resources.getString(footerId, account);
    }

    /**
     * Whether it's the update or the save dialog is defined by the fact that the entered username
     * is present/not present in the savedUsernames list.
     *
     * @param savedUsernames the usernames stored in the Password Manager for this site
     * @param username entered in the text edit box by the user
     */
    public static boolean isUpdate(List<String> savedUsernames, String username) {
        return savedUsernames.contains(username);
    }
}

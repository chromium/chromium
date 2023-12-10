// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import android.view.View;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;

import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Manages the actual showing and hiding of the full screen Incognito re-auth modal dialog. */
class IncognitoReauthDialog {
    /** The {@link ModalDialogManager} which launches the full-screen re-auth dialog. */
    private final @NonNull ModalDialogManager mModalDialogManager;

    /**
     * The modal dialog controller to detect events on the dialog but it's not needed in our
     * case.
     */
    private final ModalDialogProperties.Controller mModalDialogController =
            new ModalDialogProperties.Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {}

                @Override
                public void onDismiss(PropertyModel model, int dismissalCause) {}
            };

    /**The {@link PropertyModel} of the underlying dialog where the re-auth view would be shown.*/
    private final PropertyModel mModalDialogPropertyModel;

    /**
     * @param modalDialogManager The {@link ModalDialogManager} which is used to fire the
     *                          dialog containing the Incognito re-auth view.
     * @param incognitoReauthView The underlying Incognito re-auth {@link View} to use as custom
     * @param backPressedCallback {@link OnBackPressedCallback} which would be called when a user
     *         presses back while the fullscreen re-auth is shown.
     */
    IncognitoReauthDialog(
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull View incognitoReauthView,
            @NonNull OnBackPressedCallback backPressedCallback) {
        mModalDialogManager = modalDialogManager;
        mModalDialogPropertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, mModalDialogController)
                        .with(ModalDialogProperties.CUSTOM_VIEW, incognitoReauthView)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .with(
                                ModalDialogProperties.DIALOG_STYLES,
                                ModalDialogProperties.DialogStyles.FULLSCREEN_DARK_DIALOG)
                        .with(
                                ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER,
                                backPressedCallback)
                        .build();
    }

    /** Method to show the full-screen re-auth dialog. */
    void showIncognitoReauthDialog() {
        mModalDialogManager.showDialog(
                mModalDialogPropertyModel,
                ModalDialogManager.ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.VERY_HIGH);
    }

    /**
     * Method to hide the full-screen re-auth dialog.
     *
     * @param dismissalCause The {@link DialogDismissalCause} stating the reason why the incognito
     *                       re-auth dialog is being dismissed.
     */
    void dismissIncognitoReauthDialog(@DialogDismissalCause int dismissalCause) {
        mModalDialogManager.dismissDialog(mModalDialogPropertyModel, dismissalCause);
    }

    public PropertyModel getModalDialogPropertyModelForTesting() {
        return mModalDialogPropertyModel;
    }
}

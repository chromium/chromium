// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Dialog that informs user that the GMS Core needs to be updated and confirms whether the user
 * agrees to proceed with the update.
 */
class OutdatedGmsCoreDialog {
    private final ModalDialogManager mModalDialogManager;
    private final Context mContext;
    private final Callback<Boolean> mResultHandler;

    /**
     * Constructor for {@link OutdatedGmsCoreDialog}.
     *
     * @param modalDialogManager The {@link ModalDialogManager} which is going to display the
     *         dialog.
     * @param context The context for accessing resources.
     * @param resultHandler Handler to be called on whether the user have chosen to update or not.
     */
    OutdatedGmsCoreDialog(ModalDialogManager modalDialogManager, Context context,
            Callback<Boolean> resultHandler) {
        mModalDialogManager = modalDialogManager;
        mContext = context;
        mResultHandler = resultHandler;
    }

    /** Shows the dialog. */
    void show() {
        SimpleModalDialogController modalDialogController =
                new SimpleModalDialogController(mModalDialogManager, result -> {
                    mResultHandler.onResult(result == DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                });

        PropertyModel dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, modalDialogController)
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                mContext.getResources().getString(
                                        R.string.password_manager_outdated_gms_dialog_description))
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mContext.getResources().getString(
                                        R.string.password_manager_outdated_gms_positive_button))
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mContext.getResources().getString(
                                        R.string.password_manager_outdated_gms_negative_button))
                        .with(ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .build();

        mModalDialogManager.showDialog(dialogModel, ModalDialogManager.ModalDialogType.APP);
    }
}

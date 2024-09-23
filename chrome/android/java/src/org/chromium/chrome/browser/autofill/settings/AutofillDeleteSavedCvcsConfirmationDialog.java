// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/** Dialog that confirms whether the user wishes to delete all saved CVCs. */
public class AutofillDeleteSavedCvcsConfirmationDialog {
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final Callback<Boolean> mResultHandler;

    public AutofillDeleteSavedCvcsConfirmationDialog(
            Context context,
            ModalDialogManager modalDialogManager,
            Callback<Boolean> resultHandler) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mResultHandler = resultHandler;
    }

    /** Shows an AutofillDeleteSavedCvcsConfirmationDialog. */
    public void show() {
        SimpleModalDialogController modalDialogController =
                new SimpleModalDialogController(
                        mModalDialogManager,
                        result -> {
                            // TODO(crbug.com/40287181): Add a metric when user deletes saved CVCs.
                            mResultHandler.onResult(
                                    result == DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                        });
        PropertyModel deleteCvcsPropertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, modalDialogController)
                        .with(
                                ModalDialogProperties.TITLE,
                                mContext.getString(
                                        R.string
                                                .autofill_delete_saved_cvcs_confirmation_dialog_title))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                mContext.getString(
                                        R.string
                                                .autofill_delete_saved_cvcs_confirmation_dialog_message))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mContext.getString(
                                        R.string
                                                .autofill_delete_saved_cvcs_confirmation_dialog_delete_button_label))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mContext.getString(android.R.string.cancel))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .build();

        mModalDialogManager.showDialog(
                deleteCvcsPropertyModel, ModalDialogManager.ModalDialogType.APP);
    }
}

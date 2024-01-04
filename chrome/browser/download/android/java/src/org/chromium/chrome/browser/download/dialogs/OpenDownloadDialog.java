// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.download.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Dialog for confirming that the user wants to open a pdf download after download completion. */
public class OpenDownloadDialog {
    /**
     * Called to show a dialog for opening a download.
     *
     * @param context Context for showing the dialog.
     * @param modalDialogManager Manager for managing the modal dialog.
     * @param callback Callback to run when confirming the dialog, dismissalCause is passed as a
     *     param.
     */
    public void show(
            Context context, ModalDialogManager modalDialogManager, Callback<Integer> callback) {
        var controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                            modalDialogManager.dismissDialog(
                                    model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                        } else {
                            modalDialogManager.dismissDialog(
                                    model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                        }
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        if (callback != null) callback.onResult(dismissalCause);
                    }
                };
        var resources = context.getResources();
        PropertyModel propertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(
                                ModalDialogProperties.TITLE,
                                resources.getString(R.string.open_download_dialog_title))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                resources.getString(R.string.open_download_dialog_text))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources.getString(R.string.open_download_dialog_always_open_text))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources.getString(R.string.open_download_dialog_just_once_text))
                        .build();

        modalDialogManager.showDialog(propertyModel, ModalDialogManager.ModalDialogType.TAB);
    }
}

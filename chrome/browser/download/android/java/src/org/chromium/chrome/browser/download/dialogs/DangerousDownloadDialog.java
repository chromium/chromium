// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;

import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.download.R;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Dialog for confirming that user want to download a dangerous file, using the default model dialog
 * from ModalDialogManager.
 */
public class DangerousDownloadDialog {
    public DangerousDownloadDialog() {}

    /**
     * Called to show a warning dialog for dangerous download.
     * @param context Context for showing the dialog.
     * @param modalDialogManager Manager for managing the modal dialog.
     * @param fileName Name of the download file.
     * @param totalBytes Total bytes of the file.
     * @param iconId Icon ID of the warning dialog.
     * @param callback Callback to run when confirming the download, true for accept the download,
     *         false otherwise.
     */
    public void show(Context context, ModalDialogManager modalDialogManager, String fileName,
            long totalBytes, int iconId, Callback<Boolean> callback) {
        String message =
                context.getResources().getString(R.string.dangerous_download_dialog_text, fileName);
        if (totalBytes > 0) {
            message += "\n\n(" + DownloadUtils.getStringForBytes(context, totalBytes) + ")";
        }

        PropertyModel propertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER,
                                new ModalDialogProperties.Controller() {
                                    @Override
                                    public void onClick(PropertyModel model, int buttonType) {
                                        if (callback != null) {
                                            callback.onResult(buttonType
                                                    == ModalDialogProperties.ButtonType.POSITIVE);
                                        }
                                        modalDialogManager.dismissDialog(model,
                                                buttonType
                                                                == DialogDismissalCause
                                                                           .POSITIVE_BUTTON_CLICKED
                                                        ? DialogDismissalCause
                                                                  .POSITIVE_BUTTON_CLICKED
                                                        : DialogDismissalCause
                                                                  .NEGATIVE_BUTTON_CLICKED);
                                    }

                                    @Override
                                    public void onDismiss(PropertyModel model, int dismissalCause) {
                                        if (callback != null
                                                && dismissalCause
                                                        != DialogDismissalCause
                                                                   .POSITIVE_BUTTON_CLICKED
                                                && dismissalCause
                                                        != DialogDismissalCause
                                                                   .NEGATIVE_BUTTON_CLICKED) {
                                            callback.onResult(false);
                                        }
                                    }
                                })
                        .with(ModalDialogProperties.TITLE,
                                context.getResources().getString(
                                        R.string.dangerous_download_dialog_title))
                        .with(ModalDialogProperties.MESSAGE, message)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                context.getResources().getString(
                                        R.string.dangerous_download_dialog_confirm_text))
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                context.getResources().getString(R.string.cancel))
                        .with(ModalDialogProperties.TITLE_ICON,
                                ResourcesCompat.getDrawable(
                                        context.getResources(), iconId, context.getTheme()))
                        .build();

        modalDialogManager.showDialog(propertyModel, ModalDialogManager.ModalDialogType.APP);
    }
}

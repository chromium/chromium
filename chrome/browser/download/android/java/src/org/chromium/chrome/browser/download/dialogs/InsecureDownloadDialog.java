// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.download.R;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Dialog for confirming that the user wants to download an insecurely-delivered file, using the
 * default model dialog from ModalDialogManager.
 */
public class InsecureDownloadDialog {
    /**
     * Called to show a warning dialog for insecure download.
     * @param context Context for showing the dialog.
     * @param modalDialogManager Manager for managing the modal dialog.
     * @param fileName Name of the download file.
     * @param totalBytes Total bytes of the file.
     * @param callback Callback to run when confirming the download, true for accept the download,
     *         false otherwise.
     */
    public void show(
            Context context,
            ModalDialogManager modalDialogManager,
            String fileName,
            long totalBytes,
            Callback<Boolean> callback) {
        String message =
                totalBytes > 0
                        ? String.format(
                                "%s (%s)",
                                fileName, DownloadUtils.getStringForBytes(context, totalBytes))
                        : fileName;

        var controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        boolean acceptDownload =
                                buttonType == ModalDialogProperties.ButtonType.POSITIVE;
                        if (callback != null) {
                            callback.onResult(acceptDownload);
                        }
                        modalDialogManager.dismissDialog(
                                model,
                                acceptDownload
                                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        if (dismissalCause != DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                && dismissalCause != DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
                            if (callback != null) callback.onResult(false);
                        }
                    }
                };
        var resources = context.getResources();
        PropertyModel propertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(
                                ModalDialogProperties.TITLE,
                                resources.getString(R.string.insecure_download_dialog_title))
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, message)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources.getString(R.string.insecure_download_dialog_confirm_text))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources.getString(R.string.insecure_download_dialog_discard_text))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_OUTLINE)
                        .with(
                                ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS,
                                UiUtils.PROMPT_INPUT_PROTECTION_SHORT_DELAY_MS)
                        .build();

        modalDialogManager.showDialog(propertyModel, ModalDialogManager.ModalDialogType.TAB);
    }
}

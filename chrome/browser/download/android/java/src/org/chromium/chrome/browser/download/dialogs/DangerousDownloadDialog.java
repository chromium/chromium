// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.download.R;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Dialog for confirming that user want to download a dangerous file, using the default model dialog
 * from ModalDialogManager.
 */
public class DangerousDownloadDialog {
    /**
     * Events related to the dangerous download dialog, used for UMA reporting.
     * These values are persisted to logs. Entries should not be renumbered and
     * numeric values should never be reused.
     */
    @IntDef({
        DangerousDownloadDialogEvent.DANGEROUS_DOWNLOAD_DIALOG_SHOW,
        DangerousDownloadDialogEvent.DANGEROUS_DOWNLOAD_DIALOG_CONFIRM,
        DangerousDownloadDialogEvent.DANGEROUS_DOWNLOAD_DIALOG_CANCEL,
        DangerousDownloadDialogEvent.DANGEROUS_DOWNLOAD_DIALOG_DISMISS
    })
    private @interface DangerousDownloadDialogEvent {
        int DANGEROUS_DOWNLOAD_DIALOG_SHOW = 0;
        int DANGEROUS_DOWNLOAD_DIALOG_CONFIRM = 1;
        int DANGEROUS_DOWNLOAD_DIALOG_CANCEL = 2;
        int DANGEROUS_DOWNLOAD_DIALOG_DISMISS = 3;

        int COUNT = 4;
    }

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
    public void show(
            Context context,
            ModalDialogManager modalDialogManager,
            String fileName,
            long totalBytes,
            int iconId,
            Callback<Boolean> callback) {
        var resources = context.getResources();
        String message =
                totalBytes > 0
                        ? resources.getString(R.string.dangerous_download_dialog_text, fileName)
                        : resources.getString(
                                R.string.dangerous_download_dialog_text_with_size,
                                fileName,
                                DownloadUtils.getStringForBytes(context, totalBytes));

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
                        recordDangerousDownloadDialogEvent(
                                acceptDownload
                                        ? DangerousDownloadDialogEvent
                                                .DANGEROUS_DOWNLOAD_DIALOG_CONFIRM
                                        : DangerousDownloadDialogEvent
                                                .DANGEROUS_DOWNLOAD_DIALOG_CANCEL);
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        if (dismissalCause != DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                && dismissalCause != DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
                            if (callback != null) callback.onResult(false);
                            recordDangerousDownloadDialogEvent(
                                    DangerousDownloadDialogEvent.DANGEROUS_DOWNLOAD_DIALOG_DISMISS);
                        }
                    }
                };
        PropertyModel propertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(
                                ModalDialogProperties.TITLE,
                                resources.getString(R.string.dangerous_download_dialog_title))
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, message)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources.getString(
                                        R.string.dangerous_download_dialog_confirm_text))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources.getString(R.string.cancel))
                        .with(
                                ModalDialogProperties.TITLE_ICON,
                                ResourcesCompat.getDrawable(resources, iconId, context.getTheme()))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_OUTLINE)
                        .with(
                                ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS,
                                UiUtils.PROMPT_INPUT_PROTECTION_SHORT_DELAY_MS)
                        .build();

        modalDialogManager.showDialog(propertyModel, ModalDialogManager.ModalDialogType.TAB);
        recordDangerousDownloadDialogEvent(
                DangerousDownloadDialogEvent.DANGEROUS_DOWNLOAD_DIALOG_SHOW);
    }

    /**
     * Collects dangerous download dialog UI event metrics.
     *
     * @param event The UI event to collect.
     */
    private static void recordDangerousDownloadDialogEvent(
            @DangerousDownloadDialogEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "Download.DangerousDialog.Events", event, DangerousDownloadDialogEvent.COUNT);
    }
}

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.download.R;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Dialog for confirming that the user wants to download a mixed-content file, using the default
 * model dialog from ModalDialogManager.
 */
public class MixedContentDownloadDialog {
    /**
     * Events related to the mixed-content download dialog, used for UMA reporting.
     * These values are persisted to logs. Entries should not be renumbered and
     * numeric values should never be reused.
     */
    @IntDef({MixedContentDownloadDialogEvent.SHOW, MixedContentDownloadDialogEvent.CONFIRM,
            MixedContentDownloadDialogEvent.CANCEL, MixedContentDownloadDialogEvent.DISMISS})
    private @interface MixedContentDownloadDialogEvent {
        int SHOW = 0;
        int CONFIRM = 1;
        int CANCEL = 2;
        int DISMISS = 3;

        int COUNT = 4;
    }

    /**
     * Called to show a warning dialog for mixed-content download.
     * @param context Context for showing the dialog.
     * @param modalDialogManager Manager for managing the modal dialog.
     * @param fileName Name of the download file.
     * @param totalBytes Total bytes of the file.
     * @param callback Callback to run when confirming the download, true for accept the download,
     *         false otherwise.
     */
    public void show(Context context, ModalDialogManager modalDialogManager, String fileName,
            long totalBytes, Callback<Boolean> callback) {
        String message = totalBytes > 0
                ? fileName + " (" + DownloadUtils.getStringForBytes(context, totalBytes) + ")"
                : fileName;

        PropertyModel propertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER,
                                new ModalDialogProperties.Controller() {
                                    @Override
                                    public void onClick(PropertyModel model, int buttonType) {
                                        boolean acceptDownload = buttonType
                                                == ModalDialogProperties.ButtonType.POSITIVE;
                                        if (callback != null) {
                                            callback.onResult(acceptDownload);
                                        }
                                        modalDialogManager.dismissDialog(model,
                                                acceptDownload ? DialogDismissalCause
                                                                         .POSITIVE_BUTTON_CLICKED
                                                               : DialogDismissalCause
                                                                         .NEGATIVE_BUTTON_CLICKED);
                                        recordMixedContentDownloadDialogEvent(acceptDownload
                                                        ? MixedContentDownloadDialogEvent.CONFIRM
                                                        : MixedContentDownloadDialogEvent.CANCEL);
                                    }

                                    @Override
                                    public void onDismiss(PropertyModel model, int dismissalCause) {
                                        if (dismissalCause
                                                        != DialogDismissalCause
                                                                   .POSITIVE_BUTTON_CLICKED
                                                && dismissalCause
                                                        != DialogDismissalCause
                                                                   .NEGATIVE_BUTTON_CLICKED) {
                                            if (callback != null) callback.onResult(false);
                                            recordMixedContentDownloadDialogEvent(
                                                    MixedContentDownloadDialogEvent.DISMISS);
                                        }
                                    }
                                })
                        .with(ModalDialogProperties.TITLE,
                                context.getResources().getString(
                                        R.string.mixed_content_download_dialog_title))
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, message)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                context.getResources().getString(
                                        R.string.mixed_content_download_dialog_confirm_text))
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                context.getResources().getString(
                                        R.string.mixed_content_download_dialog_discard_text))
                        .with(ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_OUTLINE)
                        .build();

        modalDialogManager.showDialog(propertyModel, ModalDialogManager.ModalDialogType.TAB);
        recordMixedContentDownloadDialogEvent(MixedContentDownloadDialogEvent.SHOW);
    }

    /**
     * Collects mixed content download dialog UI event metrics.
     * @param event The UI event to collect.
     */
    private static void recordMixedContentDownloadDialogEvent(
            @MixedContentDownloadDialogEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "Download.MixedContentDialog.Events", event, MixedContentDownloadDialogEvent.COUNT);
    }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;
import android.view.LayoutInflater;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.download.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Dialog for confirming that the user wants to open a pdf download after download completion. */
public class OpenDownloadDialog {
    /**
     * Events related to the open download dialog, used for UMA reporting. These values are
     * persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({
        OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_SHOW,
        OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_ALWAYS_OPEN,
        OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_JUST_ONCE,
        OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_DISMISS
    })
    public @interface OpenDownloadDialogEvent {
        int OPEN_DOWNLOAD_DIALOG_SHOW = 0;
        int OPEN_DOWNLOAD_DIALOG_ALWAYS_OPEN = 1;
        int OPEN_DOWNLOAD_DIALOG_JUST_ONCE = 2;
        int OPEN_DOWNLOAD_DIALOG_DISMISS = 3;

        int COUNT = 4;
    }

    /**
     * Called to show a dialog for opening a download.
     *
     * @param context Context for showing the dialog.
     * @param modalDialogManager Manager for managing the modal dialog.
     * @param autoOpenEnabled Whether auto-open PDF is enabled.
     * @param appName Name of the app to open the file, null if there are multiple apps.
     * @param callback Callback to run when confirming the dialog, dismissalCause is passed as a
     *     param.
     */
    public void show(
            Context context,
            ModalDialogManager modalDialogManager,
            boolean autoOpenEnabled,
            String appName,
            Callback<Integer> callback) {
        OpenDownloadCustomView customView =
                (OpenDownloadCustomView)
                        LayoutInflater.from(context).inflate(R.layout.open_download_dialog, null);
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
                        if (callback != null) {
                            @OpenDownloadDialogEvent
                            int result = OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_DISMISS;
                            if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                                result =
                                        customView.getAutoOpenEnabled()
                                                ? OpenDownloadDialogEvent
                                                        .OPEN_DOWNLOAD_DIALOG_ALWAYS_OPEN
                                                : OpenDownloadDialogEvent
                                                        .OPEN_DOWNLOAD_DIALOG_JUST_ONCE;
                            }
                            callback.onResult(result);
                        }
                    }
                };
        var resources = context.getResources();
        String title = resources.getString(R.string.open_download_dialog_title);
        String positiveButtonText =
                resources.getString(R.string.open_download_dialog_continue_text);
        if (appName != null) {
            title = resources.getString(R.string.open_download_with_app_dialog_title, appName);
            positiveButtonText = resources.getString(R.string.open_download_dialog_open_text);
        }
        PropertyModel propertyModel =
                new PropertyModel.Builder(OpenDownloadDialogProperties.ALL_KEYS)
                        .with(OpenDownloadDialogProperties.TITLE, title)
                        .with(
                                OpenDownloadDialogProperties.AUTO_OPEN_CHECKBOX_CHECKED,
                                autoOpenEnabled)
                        .build();
        PropertyModelChangeProcessor.create(
                propertyModel,
                customView,
                OpenDownloadDialogViewBinder::bind,
                /* performInitialBind= */ true);

        PropertyModel showPropertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, positiveButtonText)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources.getString(R.string.open_download_dialog_cancel_text))
                        .build();
        modalDialogManager.showDialog(showPropertyModel, ModalDialogManager.ModalDialogType.TAB);
    }
}

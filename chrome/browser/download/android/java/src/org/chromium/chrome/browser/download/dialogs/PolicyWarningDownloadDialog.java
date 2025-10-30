// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.download.R;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Dialog for warning the user about a download that may contain sensitive content, based on
 * enterprise policies.
 */
@NullMarked
public class PolicyWarningDownloadDialog {
    /**
     * Events related to the dangerous download dialog, used for UMA reporting. These values are
     * persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({
        PolicyWarningDownloadDialogEvent.DIALOG_SHOW,
        PolicyWarningDownloadDialogEvent.DIALOG_CONFIRM,
        PolicyWarningDownloadDialogEvent.DIALOG_CANCEL,
        PolicyWarningDownloadDialogEvent.DIALOG_DISMISS
    })
    private @interface PolicyWarningDownloadDialogEvent {
        int DIALOG_SHOW = 0;
        int DIALOG_CONFIRM = 1;
        int DIALOG_CANCEL = 2;
        int DIALOG_DISMISS = 3;

        int COUNT = 4;
    }

    public PolicyWarningDownloadDialog() {}

    /**
     * Shows the policy warning download dialog.
     *
     * @param context Context for showing the dialog.
     * @param modalDialogManager Manager for managing the modal dialog.
     * @param fileName Name of the download file.
     * @param callback Callback to run when the user confirms or cancels the download.
     */
    public void show(
            Context context,
            ModalDialogManager modalDialogManager,
            String fileName,
            Callback<Boolean> callback) {
        var resources = context.getResources();
        var controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        // This is a special scenario as per UX specs, the Cancel button is
                        // shown on the right as the default option on the dialog. Hence, the
                        // positive button is to cancel, and negative button is to download the
                        // item.
                        boolean acceptDownload =
                                buttonType == ModalDialogProperties.ButtonType.NEGATIVE;
                        if (callback != null) {
                            callback.onResult(acceptDownload);
                        }
                        modalDialogManager.dismissDialog(
                                model,
                                acceptDownload
                                        ? DialogDismissalCause.NEGATIVE_BUTTON_CLICKED
                                        : DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                        recordPolicyWarningDownloadDialogEvent(
                                acceptDownload
                                        ? PolicyWarningDownloadDialogEvent.DIALOG_CONFIRM
                                        : PolicyWarningDownloadDialogEvent.DIALOG_CANCEL);
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        if (dismissalCause != DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                && dismissalCause != DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
                            if (callback != null) callback.onResult(false);
                            recordPolicyWarningDownloadDialogEvent(
                                    PolicyWarningDownloadDialogEvent.DIALOG_DISMISS);
                        }
                    }
                };

        PropertyModel propertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(
                                ModalDialogProperties.TITLE,
                                resources.getString(R.string.policy_warning_download_dialog_title))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPHS,
                                new ArrayList<>(
                                        List.of(
                                                TextUtils.expandTemplate(
                                                        resources.getString(
                                                                R.string
                                                                        .policy_warning_download_dialog_text),
                                                        fileName))))
                        .with(
                                // The positive button is to cancel the download.
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources.getString(R.string.cancel))
                        .with(
                                // The negative button is to download anyway.
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources.getString(
                                        R.string.dangerous_download_dialog_confirm_text))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(
                                ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS,
                                UiUtils.PROMPT_INPUT_PROTECTION_SHORT_DELAY_MS)
                        .build();

        modalDialogManager.showDialog(propertyModel, ModalDialogManager.ModalDialogType.TAB);
        recordPolicyWarningDownloadDialogEvent(PolicyWarningDownloadDialogEvent.DIALOG_SHOW);
    }

    /**
     * Collects policy warning dialog UI event metrics.
     *
     * @param event The UI event to collect.
     */
    private static void recordPolicyWarningDownloadDialogEvent(
            @PolicyWarningDownloadDialogEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "Download.PolicyWarningDialog.Events",
                event,
                PolicyWarningDownloadDialogEvent.COUNT);
    }
}

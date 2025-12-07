// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.home.DownloadHelpPageLauncher;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Dialog for confirming that the user wants to download a file with a Safe Browsing download
 * warning (i.e. bypassing the warning), using the default modal dialog from ModalDialogManager. The
 * dialog has two buttons: NEGATIVE accepts the bypass; POSITIVE opens the "Learn more" help page.
 */
@NullMarked
public class DownloadWarningBypassDialog {
    public static final String DOWNLOAD_BLOCKED_LEARN_MORE_URL =
            "https://support.google.com/chrome?p=ib_download_blocked";

    /**
     * Events related to the download warning bypass dialog. Reported to UMA and used to communicate
     * the result of the dialog. These values are persisted to logs. Entries should not be
     * renumbered and numeric values should never be reused.
     */
    @IntDef({
        DownloadWarningBypassDialogEvent.SHOW,
        DownloadWarningBypassDialogEvent.VALIDATE,
        DownloadWarningBypassDialogEvent.LEARN_MORE,
        DownloadWarningBypassDialogEvent.DISMISS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DownloadWarningBypassDialogEvent {
        // Logged when a dialog is shown.
        int SHOW = 0;
        // Following 3 buckets communicate the user's action on the dialog. Exactly 1 of these
        // should be logged after each dialog shown.
        int VALIDATE = 1;
        int LEARN_MORE = 2;
        int DISMISS = 3;

        int COUNT = 4;
    }

    /**
     * Called to show a warning bypass dialog for dangerous download with Safe Browsing warning.
     *
     * @param context Context for showing the dialog.
     * @param modalDialogManager Manager for managing the modal dialog.
     * @param helpPageLauncher Helper for opening the "Learn more" help page.
     * @param fileName Name of the download file.
     * @param callback Callback to run when reporting whether the user selected the bypass option on
     *     the dialog.
     */
    public void show(
            Context context,
            ModalDialogManager modalDialogManager,
            DownloadHelpPageLauncher helpPageLauncher,
            String fileName,
            Callback<Boolean> callback) {
        var controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        handleResult(
                                buttonType == ModalDialogProperties.ButtonType.NEGATIVE
                                        ? DownloadWarningBypassDialogEvent.VALIDATE
                                        : DownloadWarningBypassDialogEvent.LEARN_MORE);
                        modalDialogManager.dismissDialog(
                                model,
                                buttonType == ModalDialogProperties.ButtonType.NEGATIVE
                                        ? DialogDismissalCause.NEGATIVE_BUTTON_CLICKED
                                        : DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        if (dismissalCause != DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                && dismissalCause != DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
                            handleResult(DownloadWarningBypassDialogEvent.DISMISS);
                        }
                    }

                    private void handleResult(@DownloadWarningBypassDialogEvent int result) {
                        logDialogEventHistogram(result);
                        if (callback != null) {
                            callback.onResult(result == DownloadWarningBypassDialogEvent.VALIDATE);
                        }
                        if (result == DownloadWarningBypassDialogEvent.LEARN_MORE) {
                            openLearnMorePage(helpPageLauncher, context);
                        }
                    }
                };
        var resources = context.getResources();
        PropertyModel propertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.TITLE, fileName)
                        .with(ModalDialogProperties.TITLE_MAX_LINES, 1)
                        .with(
                                ModalDialogProperties.TITLE_ICON,
                                UiUtils.getTintedDrawable(
                                        context,
                                        R.drawable.dangerous_filled_24dp,
                                        R.color.error_icon_color_tint_list))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                resources.getString(R.string.download_warning_bypass_dialog_text))
                        // The bypass action is mapped to the negative button, because bypassing the
                        // security warning is the "discouraged" action.
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources.getString(
                                        R.string.download_warning_bypass_dialog_action_download))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources.getString(
                                        R.string.download_warning_bypass_dialog_action_learn_more))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_CONTENT_DESCRIPTION,
                                resources.getString(
                                        R.string
                                                .download_warning_bypass_dialog_action_learn_more_accessibility_description))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_OUTLINE)
                        .with(
                                ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS,
                                UiUtils.PROMPT_INPUT_PROTECTION_SHORT_DELAY_MS)
                        .with(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY, true)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();
        modalDialogManager.showDialog(
                propertyModel, ModalDialogManager.ModalDialogType.APP, /* showAsNext= */ true);
        logDialogEventHistogram(DownloadWarningBypassDialogEvent.SHOW);
    }

    /** Opens the "Learn More" help page URL in a Custom Tab. */
    void openLearnMorePage(DownloadHelpPageLauncher helpPageLauncher, Context context) {
        helpPageLauncher.openUrl(context, DOWNLOAD_BLOCKED_LEARN_MORE_URL);
    }

    void logDialogEventHistogram(@DownloadWarningBypassDialogEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "Download.Android.WarningBypassDialog.Events",
                event,
                DownloadWarningBypassDialogEvent.COUNT);
    }
}

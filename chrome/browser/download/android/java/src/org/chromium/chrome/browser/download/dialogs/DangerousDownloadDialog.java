// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;
import android.graphics.Typeface;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;

import androidx.annotation.IntDef;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.download.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Dialog for confirming that user want to download a dangerous file, using the default model dialog
 * from ModalDialogManager. This dialog applies only to dangerous file types, i.e. dangerType is
 * DownloadDangerType.DANGEROUS_FILE. Downloads with Safe Browsing warnings may trigger a different
 * dialog (see {@link DownloadWarningBypassDialog}).
 */
@NullMarked
public class DangerousDownloadDialog {
    /**
     * Events related to the dangerous download dialog, used for UMA reporting. These values are
     * persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
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
     *
     * @param context Context for showing the dialog.
     * @param modalDialogManager Manager for managing the modal dialog.
     * @param fileName Name of the download file.
     * @param totalBytes Total bytes of the file.
     * @param downloadDomain Domain name to associate with the downloaded file.
     * @param iconId Icon ID of the warning dialog.
     * @param callback Callback to run when confirming the download, true for accept the download,
     *     false otherwise.
     */
    public void show(
            Context context,
            ModalDialogManager modalDialogManager,
            String fileName,
            long totalBytes,
            String downloadDomain,
            int iconId,
            Callback<Boolean> callback) {
        var resources = context.getResources();
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
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPHS,
                                getMessageParagraphs(context, fileName, totalBytes, downloadDomain))
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
     * Selects the appropriate message string template and inserts formatted substitutions.
     *
     * @return an ArrayList suitable for setting as MESSAGE_PARAGRAPHS property for the dialog.
     */
    private static ArrayList<CharSequence> getMessageParagraphs(
            Context context, String fileName, long totalBytes, String downloadDomain) {
        boolean hasSize = totalBytes > 0;
        boolean hasDownloadDomain = !downloadDomain.isEmpty();

        int stringResId;
        int numSubstitutions;
        if (hasSize && hasDownloadDomain) {
            stringResId = R.string.dangerous_download_dialog_text_with_size_and_domain;
            numSubstitutions = 3;
        } else if (hasSize) {
            stringResId = R.string.dangerous_download_dialog_text_with_size;
            numSubstitutions = 2;
        } else if (hasDownloadDomain) {
            stringResId = R.string.dangerous_download_dialog_text_with_domain;
            numSubstitutions = 2;
        } else {
            stringResId = R.string.dangerous_download_dialog_text;
            numSubstitutions = 1;
        }
        CharSequence messageTemplate = context.getString(stringResId);
        ArrayList<CharSequence> substitutions = new ArrayList<CharSequence>();

        SpannableString formattedFileName = new SpannableString(fileName);
        formattedFileName.setSpan(
                new StyleSpan(Typeface.BOLD),
                0,
                fileName.length(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        substitutions.add(formattedFileName);

        if (hasSize) {
            String formattedSize = DownloadUtils.getStringForBytes(context, totalBytes);
            substitutions.add(formattedSize);
        }

        if (hasDownloadDomain) {
            SpannableString formattedDownloadDomain = new SpannableString(downloadDomain);
            formattedDownloadDomain.setSpan(
                    new ForegroundColorSpan(SemanticColorUtils.getDefaultTextColorLink(context)),
                    0,
                    downloadDomain.length(),
                    Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            substitutions.add(formattedDownloadDomain);
        }

        assert substitutions.size() == numSubstitutions;
        CharSequence message =
                TextUtils.expandTemplate(
                        messageTemplate, substitutions.toArray(new CharSequence[0]));
        return new ArrayList<>(List.of(message));
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

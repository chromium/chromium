// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.text.style.ClickableSpan;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.interstitial.NewDownloadTab;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Dialog for confirming that user want to download a file that already exists on disk, using the
 * default model dialog from ModalDialogManager.
 */
public class DuplicateDownloadDialog {
    private ModalDialogManager mModalDialogManager;
    private PropertyModel mPropertyModel;

    /**
     * Called to show a warning dialog for duplicate download.
     * @param context Context for showing the dialog.
     * @param modalDialogManager Manager for managing the modal dialog.
     * @param filePath Path of the download file.
     * @param pageUrl URL of the page, empty for file downloads.
     * @param totalBytes Total bytes of the file.
     * @param duplicateExists Whether a duplicate download is in progress.
     * @param otrProfileID Off the record profile ID.
     * @param callback Callback to run when confirming the download, true for accept the download,
     *         false otherwise.
     */
    public void show(
            Context context,
            ModalDialogManager modalDialogManager,
            String filePath,
            String pageUrl,
            long totalBytes,
            boolean duplicateExists,
            OTRProfileID otrProfileID,
            Callback<Boolean> callback) {
        var resources = context.getResources();
        mModalDialogManager = modalDialogManager;
        mPropertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.CONTROLLER,
                                getController(context, modalDialogManager, pageUrl, callback))
                        .with(
                                ModalDialogProperties.TITLE,
                                resources,
                                pageUrl.isEmpty()
                                        ? R.string.duplicate_download_dialog_title
                                        : R.string.duplicate_page_download_dialog_title)
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                getClickableSpan(
                                        context,
                                        filePath,
                                        pageUrl,
                                        totalBytes,
                                        duplicateExists,
                                        otrProfileID))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                R.string.duplicate_download_dialog_confirm_text)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.cancel)
                        .build();

        if (OTRProfileID.isOffTheRecord(otrProfileID)) {
            mPropertyModel.set(
                    ModalDialogProperties.MESSAGE_PARAGRAPH_2,
                    resources.getString(R.string.download_location_incognito_warning));
        }

        modalDialogManager.showDialog(mPropertyModel, ModalDialogManager.ModalDialogType.TAB);
    }

    @NonNull
    private ModalDialogProperties.Controller getController(
            Context context,
            ModalDialogManager modalDialogManager,
            String pageUrl,
            Callback<Boolean> callback) {
        return new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                boolean isConfirm = buttonType == ModalDialogProperties.ButtonType.POSITIVE;
                if (callback != null) {
                    callback.onResult(isConfirm);
                }
                modalDialogManager.dismissDialog(
                        model,
                        isConfirm
                                ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {
                if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                    return;
                }
                if (callback != null
                        && dismissalCause != DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
                    callback.onResult(false);
                }
                if (context instanceof AsyncInitializationActivity) {
                    NewDownloadTab.closeExistingNewDownloadTab(
                            ((AsyncInitializationActivity) context).getWindowAndroid());
                }
            }
        };
    }

    /**
     * Called to close the dialog.
     * @param isOfflinePage Whether this is an offline page download.
     */
    private void closeDialog(boolean isOfflinePage) {
        mModalDialogManager.dismissDialog(mPropertyModel, DialogDismissalCause.ACTION_ON_CONTENT);
    }

    /**
     * Gets the clickable span to display on the dialog.
     * @param context Context for showing the dialog.
     * @param filePath Path of the download file. Or the actual page URL for offline page download.
     * @param pageUrl URL of the page, formatted for better display and empty for file downloads.
     * @param totalBytes Total bytes of the file.
     * @param duplicateExists Whether a duplicate download is in progress.
     * @param otrProfileID Off the record profile ID.
     */
    private CharSequence getClickableSpan(
            Context context,
            String filePath,
            String pageUrl,
            long totalBytes,
            boolean duplicateExists,
            OTRProfileID otrProfileID) {
        if (pageUrl.isEmpty()) {
            DuplicateDownloadClickableSpan span =
                    new DuplicateDownloadClickableSpan(
                            context,
                            filePath,
                            () -> this.closeDialog(false),
                            otrProfileID,
                            DownloadOpenSource.DUPLICATE_DOWNLOAD_DIALOG);
            String template = context.getString(R.string.duplicate_download_dialog_text);
            return DownloadUtils.getDownloadMessageText(
                    context, template, filePath, true, totalBytes, (ClickableSpan) span);
        }
        return DownloadUtils.getOfflinePageMessageText(
                context,
                filePath,
                duplicateExists,
                new ClickableSpan() {
                    @Override
                    public void onClick(View view) {
                        closeDialog(true);
                        DownloadUtils.openPageUrl(context, filePath);
                    }
                });
    }
}

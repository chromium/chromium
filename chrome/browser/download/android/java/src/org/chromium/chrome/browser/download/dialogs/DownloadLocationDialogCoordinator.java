// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.view.LayoutInflater;

import androidx.annotation.NonNull;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.download.DirectoryOption;
import org.chromium.chrome.browser.download.DownloadDialogBridge;
import org.chromium.chrome.browser.download.DownloadDirectoryProvider;
import org.chromium.chrome.browser.download.DownloadLocationDialogType;
import org.chromium.chrome.browser.download.DownloadPromptStatus;
import org.chromium.chrome.browser.download.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.io.File;
import java.util.ArrayList;

/**
 * The factory class that contains all dependencies for the download location dialog.
 * Also provides the public functionalties to interact with dialog.
 */
// TODO(xingliu): Refactor download location dialog to fully use clank MVC.
public class DownloadLocationDialogCoordinator implements ModalDialogProperties.Controller {
    @NonNull
    private DownloadLocationDialogController mController;
    private PropertyModel mDialogModel;
    private DownloadLocationCustomView mCustomView;
    private ModalDialogManager mModalDialogManager;
    private long mTotalBytes;
    private @DownloadLocationDialogType int mDialogType;
    private String mSuggestedPath;
    private Context mContext;

    /**
     * Initializes the download location dialog.
     * @param controller Receives events from download location dialog.
     */
    public void initialize(DownloadLocationDialogController controller) {
        mController = controller;
    }

    /**
     * Shows the download location dialog.
     * @param context The {@link Context} for the dialog.
     * @param modalDialogManager {@link ModalDialogManager} to control the dialog.
     * @param totalBytes The total download file size. May be 0 if not available.
     * @param dialogType The type of the location dialog.
     * @param suggestedPath The suggested file path used by the location dialog.
     */
    public void showDialog(Context context, ModalDialogManager modalDialogManager, long totalBytes,
            @DownloadLocationDialogType int dialogType, String suggestedPath) {
        if (context == null || modalDialogManager == null) {
            onDismiss(null, DialogDismissalCause.ACTIVITY_DESTROYED);
            return;
        }

        mContext = context;
        mModalDialogManager = modalDialogManager;
        mTotalBytes = totalBytes;
        mDialogType = dialogType;
        mSuggestedPath = suggestedPath;

        DownloadDirectoryProvider.getInstance().getAllDirectoriesOptions(
                (ArrayList<DirectoryOption> dirs) -> { onDirectoryOptionsRetrieved(dirs); });
    }

    /**
     * Destroy the location dialog.
     */
    public void destroy() {
        if (mModalDialogManager != null) {
            mModalDialogManager.dismissDialog(
                    mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
        }
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        switch (buttonType) {
            case ModalDialogProperties.ButtonType.POSITIVE:
                mModalDialogManager.dismissDialog(
                        model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                break;
            case ModalDialogProperties.ButtonType.NEGATIVE:
                mModalDialogManager.dismissDialog(
                        model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                break;
            default:
        }
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                handleResponses(mCustomView.getFileName(), mCustomView.getDirectoryOption(),
                        mCustomView.getDontShowAgain());
                break;
            default:
                cancel();
                break;
        }
        mDialogModel = null;
        mCustomView = null;
    }

    /**
     * Called after retrieved the download directory options.
     * @param dirs An list of available download directories.
     */
    private void onDirectoryOptionsRetrieved(ArrayList<DirectoryOption> dirs) {
        // If there is only one directory available, don't show the default dialog, and set the
        // download directory to default. Dialog will still show for other types of dialogs, like
        // name conflict or disk error.
        if (dirs.size() == 1 && mDialogType == DownloadLocationDialogType.DEFAULT) {
            final DirectoryOption dir = dirs.get(0);
            if (dir.type == DirectoryOption.DownloadLocationDirectoryType.DEFAULT) {
                assert (!TextUtils.isEmpty(dir.location));
                DownloadDialogBridge.setDownloadAndSaveFileDefaultDirectory(dir.location);
                mController.onDownloadLocationDialogComplete(mSuggestedPath);
            }
            return;
        }

        // Already showing the dialog.
        if (mDialogModel != null) return;

        // Actually show the dialog.
        mCustomView = (DownloadLocationCustomView) LayoutInflater.from(mContext).inflate(
                R.layout.download_location_dialog, null);
        mCustomView.initialize(
                mDialogType, new File(mSuggestedPath), mTotalBytes, getTitle(mDialogType));

        Resources resources = mContext.getResources();
        mDialogModel = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                               .with(ModalDialogProperties.CONTROLLER, this)
                               .with(ModalDialogProperties.CUSTOM_VIEW, mCustomView)
                               .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                       R.string.duplicate_download_infobar_download_button)
                               .with(ModalDialogProperties.PRIMARY_BUTTON_FILLED, true)
                               .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                       R.string.cancel)
                               .build();

        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    private String getTitle(@DownloadLocationDialogType int dialogType) {
        switch (dialogType) {
            case DownloadLocationDialogType.LOCATION_FULL:
                return mContext.getString(R.string.download_location_not_enough_space);

            case DownloadLocationDialogType.LOCATION_NOT_FOUND:
                return mContext.getString(R.string.download_location_no_sd_card);

            case DownloadLocationDialogType.NAME_CONFLICT:
                return mContext.getString(R.string.download_location_download_again);

            case DownloadLocationDialogType.NAME_TOO_LONG:
                return mContext.getString(R.string.download_location_rename_file);

            case DownloadLocationDialogType.LOCATION_SUGGESTION: // Intentional fall through.
            case DownloadLocationDialogType.DEFAULT:
                return mContext.getString(R.string.download_location_dialog_title);
        }
        assert false;
        return null;
    }

    /**
     * Pass along information from location dialog to native.
     *
     * @param fileName Name the user gave the file.
     * @param directoryOption Location the user wants the file saved to.
     * @param dontShowAgain Whether the user wants the "Save download to..." dialog shown again.
     */
    private void handleResponses(
            String fileName, DirectoryOption directoryOption, boolean dontShowAgain) {
        // If there's no file location, treat as a cancellation.
        if (directoryOption == null || directoryOption.location == null || fileName == null) {
            cancel();
            return;
        }

        // Update native with new path.
        DownloadDialogBridge.setDownloadAndSaveFileDefaultDirectory(directoryOption.location);

        RecordHistogram.recordEnumeratedHistogram("MobileDownload.Location.Dialog.DirectoryType",
                directoryOption.type, DirectoryOption.DownloadLocationDirectoryType.NUM_ENTRIES);

        File file = new File(directoryOption.location, fileName);

        assert mController != null;
        mController.onDownloadLocationDialogComplete(file.getAbsolutePath());

        // Update preference to show prompt based on whether checkbox is checked only when the user
        // click the positive button.
        if (dontShowAgain) {
            DownloadDialogBridge.setPromptForDownloadAndroid(DownloadPromptStatus.DONT_SHOW);
        } else {
            DownloadDialogBridge.setPromptForDownloadAndroid(DownloadPromptStatus.SHOW_PREFERENCE);
        }
    }

    private void cancel() {
        assert mController != null;
        mController.onDownloadLocationDialogCanceled();
    }
}

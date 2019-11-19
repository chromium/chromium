// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.view.LayoutInflater;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.download.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.io.File;
import java.util.ArrayList;

/**
 * Helper class to handle communication between download location dialog and native.
 */
public class DownloadLocationDialogBridge implements ModalDialogProperties.Controller {
    private long mNativeDownloadLocationDialogBridge;
    private PropertyModel mDialogModel;
    private DownloadLocationCustomView mCustomView;
    private ModalDialogManager mModalDialogManager;
    private long mTotalBytes;
    private @DownloadLocationDialogType int mDialogType;
    private String mSuggestedPath;
    private Context mContext;

    private DownloadLocationDialogBridge(long nativeDownloadLocationDialogBridge) {
        mNativeDownloadLocationDialogBridge = nativeDownloadLocationDialogBridge;
    }

    @CalledByNative
    public static DownloadLocationDialogBridge create(long nativeDownloadLocationDialogBridge) {
        return new DownloadLocationDialogBridge(nativeDownloadLocationDialogBridge);
    }

    @CalledByNative
    private void destroy() {
        mNativeDownloadLocationDialogBridge = 0;
        if (mModalDialogManager != null) {
            mModalDialogManager.dismissDialog(
                    mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
        }
    }

    @CalledByNative
    public void showDialog(WindowAndroid windowAndroid, long totalBytes,
            @DownloadLocationDialogType int dialogType, String suggestedPath) {
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        // If the activity has gone away, just clean up the native pointer.
        if (activity == null) {
            onDismiss(null, DialogDismissalCause.ACTIVITY_DESTROYED);
            return;
        }

        mModalDialogManager = activity.getModalDialogManager();
        mContext = activity;
        mTotalBytes = totalBytes;
        mDialogType = dialogType;
        mSuggestedPath = suggestedPath;

        DownloadDirectoryProvider.getInstance().getAllDirectoriesOptions(
                (ArrayList<DirectoryOption> dirs) -> { onDirectoryOptionsRetrieved(dirs); });
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
                assert(!TextUtils.isEmpty(dir.location));
                setDownloadAndSaveFileDefaultDirectory(dir.location);
                DownloadLocationDialogBridgeJni.get().onComplete(
                        mNativeDownloadLocationDialogBridge, DownloadLocationDialogBridge.this,
                        mSuggestedPath);
            }
            return;
        }

        // Already showing the dialog.
        if (mDialogModel != null) return;

        // Actually show the dialog.
        mCustomView = (DownloadLocationCustomView) LayoutInflater.from(mContext).inflate(
                R.layout.download_location_dialog, null);
        mCustomView.initialize(mDialogType, new File(mSuggestedPath));

        Resources resources = mContext.getResources();
        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.TITLE, getTitle(mTotalBytes, mDialogType))
                        .with(ModalDialogProperties.CUSTOM_VIEW, mCustomView)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                R.string.duplicate_download_infobar_download_button)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.cancel)
                        .build();

        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    private String getTitle(long totalBytes, @DownloadLocationDialogType int dialogType) {
        switch (dialogType) {
            case DownloadLocationDialogType.LOCATION_FULL:
                return mContext.getString(R.string.download_location_not_enough_space);

            case DownloadLocationDialogType.LOCATION_NOT_FOUND:
                return mContext.getString(R.string.download_location_no_sd_card);

            case DownloadLocationDialogType.NAME_CONFLICT:
                return mContext.getString(R.string.download_location_download_again);

            case DownloadLocationDialogType.NAME_TOO_LONG:
                return mContext.getString(R.string.download_location_rename_file);

            case DownloadLocationDialogType.DEFAULT:
                String title = mContext.getString(R.string.download_location_dialog_title);
                if (totalBytes > 0) {
                    StringBuilder stringBuilder = new StringBuilder(title);
                    stringBuilder.append(" ");
                    stringBuilder.append(DownloadUtils.getStringForBytes(mContext, totalBytes));
                    title = stringBuilder.toString();
                }
                return title;
        }
        assert false;
        return null;
    }

    /**
     * Pass along information from location dialog to native.
     *
     * @param fileName      Name the user gave the file.
     * @param directoryOption  Location the user wants the file saved to.
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
        if (mNativeDownloadLocationDialogBridge != 0) {
            setDownloadAndSaveFileDefaultDirectory(directoryOption.location);

            RecordHistogram.recordEnumeratedHistogram(
                    "MobileDownload.Location.Dialog.DirectoryType", directoryOption.type,
                    DirectoryOption.DownloadLocationDirectoryType.NUM_ENTRIES);

            File file = new File(directoryOption.location, fileName);
            DownloadLocationDialogBridgeJni.get().onComplete(mNativeDownloadLocationDialogBridge,
                    DownloadLocationDialogBridge.this, file.getAbsolutePath());
        }

        // Update preference to show prompt based on whether checkbox is checked only when the user
        // click the positive button.
        if (dontShowAgain) {
            DownloadUtils.setPromptForDownloadAndroid(DownloadPromptStatus.DONT_SHOW);
        } else {
            DownloadUtils.setPromptForDownloadAndroid(DownloadPromptStatus.SHOW_PREFERENCE);
        }
    }

    private void cancel() {
        if (mNativeDownloadLocationDialogBridge != 0) {
            DownloadLocationDialogBridgeJni.get().onCanceled(
                    mNativeDownloadLocationDialogBridge, DownloadLocationDialogBridge.this);
        }
    }

    /**
     * @return The stored download default directory.
     */
    public static String getDownloadDefaultDirectory() {
        return DownloadLocationDialogBridgeJni.get().getDownloadDefaultDirectory();
    }

    /**
     * @param directory New directory to set as the download default directory.
     */
    public static void setDownloadAndSaveFileDefaultDirectory(String directory) {
        DownloadLocationDialogBridgeJni.get().setDownloadAndSaveFileDefaultDirectory(directory);
    }

    @NativeMethods
    interface Natives {
        void onComplete(long nativeDownloadLocationDialogBridge,
                DownloadLocationDialogBridge caller, String returnedPath);
        void onCanceled(
                long nativeDownloadLocationDialogBridge, DownloadLocationDialogBridge caller);
        String getDownloadDefaultDirectory();
        void setDownloadAndSaveFileDefaultDirectory(String directory);
    }
}

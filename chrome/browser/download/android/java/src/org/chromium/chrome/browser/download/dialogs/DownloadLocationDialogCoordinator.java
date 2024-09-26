// Copyright 2020 The Chromium Authors
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
import org.chromium.chrome.browser.download.settings.DownloadLocationHelperImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.io.File;
import java.util.ArrayList;

/**
 * The factory class that contains all dependencies for the download location dialog.
 * Also provides the public functionalties to interact with dialog.
 */
public class DownloadLocationDialogCoordinator implements ModalDialogProperties.Controller {
    @NonNull private DownloadLocationDialogController mController;
    private PropertyModel mDialogModel;
    private PropertyModel mDownloadLocationDialogModel;
    private PropertyModelChangeProcessor<PropertyModel, DownloadLocationCustomView, PropertyKey>
            mPropertyModelChangeProcessor;
    private DownloadLocationCustomView mCustomView;
    private ModalDialogManager mModalDialogManager;
    private long mTotalBytes;
    private @DownloadLocationDialogType int mDialogType;
    private String mSuggestedPath;
    private Context mContext;
    private boolean mHasMultipleDownloadLocations;
    private Profile mProfile;
    private boolean mLocationDialogManaged;

    /**
     * Initializes the download location dialog.
     * @param controller Receives events from download location dialog.
     */
    public void initialize(DownloadLocationDialogController controller) {
        mController = controller;
    }

    /**
     * Shows the download location dialog.
     *
     * @param context The {@link Context} for the dialog.
     * @param modalDialogManager {@link ModalDialogManager} to control the dialog.
     * @param totalBytes The total download file size. May be 0 if not available.
     * @param dialogType The type of the location dialog.
     * @param suggestedPath The suggested file path used by the location dialog.
     */
    public void showDialog(
            Context context,
            ModalDialogManager modalDialogManager,
            long totalBytes,
            @DownloadLocationDialogType int dialogType,
            String suggestedPath,
            Profile profile) {
        if (context == null || modalDialogManager == null) {
            onDismiss(null, DialogDismissalCause.ACTIVITY_DESTROYED);
            return;
        }

        mContext = context;
        mModalDialogManager = modalDialogManager;
        mTotalBytes = totalBytes;
        mDialogType = dialogType;
        mSuggestedPath = suggestedPath;
        mLocationDialogManaged = DownloadDialogBridge.isLocationDialogManaged(profile);
        mProfile = profile;

        DownloadDirectoryProvider.getInstance()
                .getAllDirectoriesOptions(
                        (ArrayList<DirectoryOption> dirs) -> {
                            onDirectoryOptionsRetrieved(dirs);
                        });
    }

    /** Destroy the location dialog. */
    public void destroy() {
        if (mModalDialogManager != null) {
            mModalDialogManager.dismissDialog(
                    mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
        }
        if (mPropertyModelChangeProcessor != null) mPropertyModelChangeProcessor.destroy();
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
                handleResponses(
                        mCustomView.getFileName(),
                        mCustomView.getDirectoryOption(),
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
        // name conflict or disk error or if Incognito download warning is needed.
        if (dirs.size() == 1
                && !mLocationDialogManaged
                && mDialogType == DownloadLocationDialogType.DEFAULT
                && !mProfile.isOffTheRecord()) {
            final DirectoryOption dir = dirs.get(0);
            if (dir.type == DirectoryOption.DownloadLocationDirectoryType.DEFAULT) {
                assert !TextUtils.isEmpty(dir.location);
                DownloadDialogBridge.setDownloadAndSaveFileDefaultDirectory(mProfile, dir.location);
                mController.onDownloadLocationDialogComplete(mSuggestedPath);
            }
            return;
        }

        // Already showing the dialog.
        if (mDialogModel != null) return;

        mHasMultipleDownloadLocations = dirs.size() > 1;

        // Actually show the dialog.
        mDownloadLocationDialogModel = getLocationDialogModel();
        mCustomView =
                (DownloadLocationCustomView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.download_location_dialog, null);
        mCustomView.initialize(
                mDialogType,
                mTotalBytes,
                (isChecked) -> {
                    DownloadDialogBridge.setPromptForDownloadAndroid(
                            mProfile,
                            isChecked
                                    ? DownloadPromptStatus.DONT_SHOW
                                    : DownloadPromptStatus.SHOW_PREFERENCE);
                },
                new DownloadLocationHelperImpl(mProfile));
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mDownloadLocationDialogModel,
                        mCustomView,
                        DownloadLocationDialogViewBinder::bind,
                        /* performInitialBind= */ true);

        Resources resources = mContext.getResources();
        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mCustomView)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                R.string.duplicate_download_infobar_download_button)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.cancel)
                        .with(
                                ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS,
                                UiUtils.PROMPT_INPUT_PROTECTION_SHORT_DELAY_MS)
                        .build();

        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    private PropertyModel getLocationDialogModel() {
        boolean isInitial =
                DownloadDialogBridge.getPromptForDownloadAndroid(mProfile)
                        == DownloadPromptStatus.SHOW_INITIAL;

        PropertyModel.Builder builder =
                new PropertyModel.Builder(DownloadLocationDialogProperties.ALL_KEYS);
        builder.with(DownloadLocationDialogProperties.DONT_SHOW_AGAIN_CHECKBOX_CHECKED, isInitial);
        builder.with(
                DownloadLocationDialogProperties.FILE_NAME, new File(mSuggestedPath).getName());
        builder.with(DownloadLocationDialogProperties.SHOW_SUBTITLE, true);
        builder.with(
                DownloadLocationDialogProperties.DONT_SHOW_AGAIN_CHECKBOX_SHOWN,
                !mLocationDialogManaged);
        switch (mDialogType) {
            case DownloadLocationDialogType.LOCATION_FULL:
                builder.with(
                        DownloadLocationDialogProperties.TITLE,
                        mContext.getString(R.string.download_location_not_enough_space));
                builder.with(
                        DownloadLocationDialogProperties.SUBTITLE,
                        mContext.getString(R.string.download_location_download_to_default_folder));
                builder.with(
                        DownloadLocationDialogProperties.DONT_SHOW_AGAIN_CHECKBOX_SHOWN, false);
                break;
            case DownloadLocationDialogType.LOCATION_NOT_FOUND:
                builder.with(
                        DownloadLocationDialogProperties.TITLE,
                        mContext.getString(R.string.download_location_no_sd_card));
                builder.with(
                        DownloadLocationDialogProperties.SUBTITLE,
                        mContext.getString(R.string.download_location_download_to_default_folder));
                builder.with(
                        DownloadLocationDialogProperties.DONT_SHOW_AGAIN_CHECKBOX_SHOWN, false);
                break;
            case DownloadLocationDialogType.NAME_CONFLICT:
                builder.with(
                        DownloadLocationDialogProperties.TITLE,
                        mContext.getString(R.string.download_location_download_again));
                builder.with(
                        DownloadLocationDialogProperties.SUBTITLE,
                        mContext.getString(R.string.download_location_name_exists));
                break;
            case DownloadLocationDialogType.NAME_TOO_LONG:
                builder.with(
                        DownloadLocationDialogProperties.TITLE,
                        mContext.getString(R.string.download_location_rename_file));
                builder.with(
                        DownloadLocationDialogProperties.SUBTITLE,
                        mContext.getString(R.string.download_location_name_too_long));
                builder.with(
                        DownloadLocationDialogProperties.DONT_SHOW_AGAIN_CHECKBOX_SHOWN, false);
                break;
            case DownloadLocationDialogType.LOCATION_SUGGESTION:
                builder.with(DownloadLocationDialogProperties.TITLE, getDefaultTitle());
                builder.with(DownloadLocationDialogProperties.SHOW_LOCATION_AVAILABLE_SPACE, true);
                assert mTotalBytes > 0;
                builder.with(
                        DownloadLocationDialogProperties.FILE_SIZE,
                        DownloadUtils.getStringForBytes(mContext, mTotalBytes));
                builder.with(DownloadLocationDialogProperties.SHOW_SUBTITLE, false);
                break;
            case DownloadLocationDialogType.DEFAULT:
                builder.with(DownloadLocationDialogProperties.TITLE, getDefaultTitle());

                if (mTotalBytes > 0) {
                    builder.with(
                            DownloadLocationDialogProperties.SUBTITLE,
                            DownloadUtils.getStringForBytes(mContext, mTotalBytes));
                } else {
                    builder.with(DownloadLocationDialogProperties.SHOW_SUBTITLE, false);
                }
                break;
        }

        if (mProfile.isOffTheRecord()) {
            builder.with(DownloadLocationDialogProperties.SHOW_INCOGNITO_WARNING, true);
            builder.with(DownloadLocationDialogProperties.DONT_SHOW_AGAIN_CHECKBOX_SHOWN, false);
        }

        return builder.build();
    }

    private String getDefaultTitle() {
        return mContext.getString(
                mLocationDialogManaged
                                || (mProfile.isOffTheRecord() && !mHasMultipleDownloadLocations)
                        ? R.string.download_location_dialog_title_confirm_download
                        : R.string.download_location_dialog_title);
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
        DownloadDialogBridge.setDownloadAndSaveFileDefaultDirectory(
                mProfile, directoryOption.location);

        RecordHistogram.recordEnumeratedHistogram(
                "MobileDownload.Location.Dialog.DirectoryType",
                directoryOption.type,
                DirectoryOption.DownloadLocationDirectoryType.NUM_ENTRIES);

        File file = new File(directoryOption.location, fileName);

        assert mController != null;
        mController.onDownloadLocationDialogComplete(file.getAbsolutePath());

        // Update preference to show prompt based on whether checkbox is checked only when the user
        // click the positive button.
        if (!mLocationDialogManaged) {
            DownloadDialogBridge.setPromptForDownloadAndroid(
                    mProfile,
                    dontShowAgain
                            ? DownloadPromptStatus.DONT_SHOW
                            : DownloadPromptStatus.SHOW_PREFERENCE);
        }
    }

    private void cancel() {
        assert mController != null;
        mController.onDownloadLocationDialogCanceled();
    }
}

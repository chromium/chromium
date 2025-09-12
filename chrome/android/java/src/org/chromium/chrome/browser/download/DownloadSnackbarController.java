// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

/** Class for displaying a snackbar when a download completes. */
@NullMarked
public class DownloadSnackbarController implements SnackbarManager.SnackbarController {
    public static final int INVALID_NOTIFICATION_ID = -1;
    private static final int SNACKBAR_DURATION_MS = 7000;

    @Override
    public void onAction(@Nullable Object actionData) {
        DownloadManagerService.openDownloadsPage(
                /* otrProfileId= */ null, DownloadOpenSource.SNACK_BAR);
    }

    @Override
    public void onDismissNoAction(@Nullable Object actionData) {}

    /**
     * Called to display the download failed snackbar.
     *
     * @param errorMessage The message to show on the snackbar.
     * @param showAllDownloads Whether to show all downloads in case the failure is caused by
     *     duplicated files.
     * @param otrProfileId The {@link OtrProfileId} of the download. Null if in regular mode.
     */
    public void onDownloadFailed(
            String errorMessage, boolean showAllDownloads, @Nullable OtrProfileId otrProfileId) {
        if (isShowingDownloadInfoBar(otrProfileId)) return;
        if (getSnackbarManager() == null) return;
        // TODO(qinmin): Coalesce snackbars if multiple downloads finish at the same time.
        Snackbar snackbar =
                Snackbar.make(
                                errorMessage,
                                this,
                                Snackbar.TYPE_NOTIFICATION,
                                Snackbar.UMA_DOWNLOAD_FAILED)
                        .setDefaultLines(false)
                        .setDuration(SNACKBAR_DURATION_MS);
        if (showAllDownloads) {
            snackbar.setAction(
                    ContextUtils.getApplicationContext().getString(R.string.open_downloaded_label),
                    null);
        }
        getSnackbarManager().showSnackbar(snackbar);
    }

    /**
     * Displays a snackbar that says alerts the user that some downloads may be missing because a
     * missing SD card was detected.
     */
    void onDownloadDirectoryNotFound() {
        if (getSnackbarManager() == null) return;

        Snackbar snackbar =
                Snackbar.make(
                                ContextUtils.getApplicationContext()
                                        .getString(R.string.download_location_no_sd_card_snackbar),
                                this,
                                Snackbar.TYPE_NOTIFICATION,
                                Snackbar.UMA_MISSING_FILES_NO_SD_CARD)
                        .setDefaultLines(false)
                        .setDuration(SNACKBAR_DURATION_MS);
        getSnackbarManager().showSnackbar(snackbar);
    }

    private @Nullable Activity getActivity() {
        if (ApplicationStatus.hasVisibleActivities()) {
            return ApplicationStatus.getLastTrackedFocusedActivity();
        } else {
            return null;
        }
    }

    public @Nullable SnackbarManager getSnackbarManager() {
        Activity activity = getActivity();
        if (activity != null && activity instanceof SnackbarManager.SnackbarManageable) {
            return ((SnackbarManager.SnackbarManageable) activity).getSnackbarManager();
        }
        return null;
    }

    private boolean isShowingDownloadInfoBar(@Nullable OtrProfileId otrProfileId) {
        DownloadMessageUiController messageUiController =
                DownloadManagerService.getDownloadManagerService()
                        .getMessageUiController(otrProfileId);
        return messageUiController == null ? false : messageUiController.isShowing();
    }
}

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;
import android.app.DownloadManager;
import android.content.Intent;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorNotificationBridgeUiFactory;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OpenParams;

/** Class for displaying a snackbar when a download completes. */
public class DownloadSnackbarController implements SnackbarManager.SnackbarController {
    public static final int INVALID_NOTIFICATION_ID = -1;
    private static final int SNACKBAR_DURATION_MS = 7000;

    private static class ActionDataInfo {
        public final DownloadInfo downloadInfo;
        public final int notificationId;
        public final long systemDownloadId;
        public final boolean usesAndroidDownloadManager;

        ActionDataInfo(
                DownloadInfo downloadInfo,
                int notificationId,
                long systemDownloadId,
                boolean usesAndroidDownloadManager) {
            this.downloadInfo = downloadInfo;
            this.notificationId = notificationId;
            this.systemDownloadId = systemDownloadId;
            this.usesAndroidDownloadManager = usesAndroidDownloadManager;
        }
    }

    @Override
    public void onAction(Object actionData) {
        if (!(actionData instanceof ActionDataInfo)) {
            DownloadManagerService.openDownloadsPage(
                    /* otrProfileID= */ null, DownloadOpenSource.SNACK_BAR);
            return;
        }

        DownloadManagerService manager = DownloadManagerService.getDownloadManagerService();
        final ActionDataInfo download = (ActionDataInfo) actionData;
        if (LegacyHelpers.isLegacyDownload(download.downloadInfo.getContentId())) {
            if (download.usesAndroidDownloadManager) {
                ContextUtils.getApplicationContext()
                        .startActivity(
                                new Intent(DownloadManager.ACTION_VIEW_DOWNLOADS)
                                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
            } else {
                manager.openDownloadedContent(
                        download.downloadInfo,
                        download.systemDownloadId,
                        DownloadOpenSource.SNACK_BAR);
            }
        } else {
            OfflineContentAggregatorNotificationBridgeUiFactory.instance()
                    .openItem(
                            new OpenParams(LaunchLocation.PROGRESS_BAR),
                            download.downloadInfo.getContentId());
        }

        if (download.notificationId != INVALID_NOTIFICATION_ID) {
            manager.getDownloadNotifier()
                    .removeDownloadNotification(download.notificationId, download.downloadInfo);
        }
    }

    @Override
    public void onDismissNoAction(Object actionData) {}

    /**
     * Called to display the download failed snackbar.
     *
     * @param errorMessage     The message to show on the snackbar.
     * @param showAllDownloads Whether to show all downloads in case the failure is caused by
     *                         duplicated files.
     * @param otrProfileID     The {@link OTRProfileID} of the download. Null if in regular mode.
     */
    public void onDownloadFailed(
            String errorMessage, boolean showAllDownloads, OTRProfileID otrProfileID) {
        if (isShowingDownloadInfoBar(otrProfileID)) return;
        if (getSnackbarManager() == null) return;
        // TODO(qinmin): Coalesce snackbars if multiple downloads finish at the same time.
        Snackbar snackbar =
                Snackbar.make(
                                errorMessage,
                                this,
                                Snackbar.TYPE_NOTIFICATION,
                                Snackbar.UMA_DOWNLOAD_FAILED)
                        .setSingleLine(false)
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
                        .setSingleLine(false)
                        .setDuration(SNACKBAR_DURATION_MS);
        getSnackbarManager().showSnackbar(snackbar);
    }

    private Activity getActivity() {
        if (ApplicationStatus.hasVisibleActivities()) {
            return ApplicationStatus.getLastTrackedFocusedActivity();
        } else {
            return null;
        }
    }

    public SnackbarManager getSnackbarManager() {
        Activity activity = getActivity();
        if (activity != null && activity instanceof SnackbarManager.SnackbarManageable) {
            return ((SnackbarManager.SnackbarManageable) activity).getSnackbarManager();
        }
        return null;
    }

    private boolean isShowingDownloadInfoBar(OTRProfileID otrProfileID) {
        DownloadMessageUiController messageUiController =
                DownloadManagerService.getDownloadManagerService()
                        .getMessageUiController(otrProfileID);
        return messageUiController == null ? false : messageUiController.isShowing();
    }
}

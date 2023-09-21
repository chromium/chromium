// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.chrome.browser.download.DownloadSnackbarController.INVALID_NOTIFICATION_ID;

import android.app.Notification;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;

/**
 * User initiated jobs implementation of {@link DownloadContinuityManager} that plays a role in
 * keeping chrome alive when there are active downloads. This class is only responsible for
 * attaching the notification to the job life cycle. Starting and stopping of jobs is
 * handled in AutoResumptionHandler in native. Only active for Android versions >= U.
 */
public class DownloadUserInitiatedTaskManager extends DownloadContinuityManager {
    private static final String TAG = "DownloadUitm";

    /** Constructor. */
    public DownloadUserInitiatedTaskManager() {}

    /**
     * Process the notification queue for all cases and initiate any needed actions, i.e. attach
     * the best download notification to the background job.
     * @param isProcessingPending Whether the call was made to process pending notifications that
     *                            have accumulated in the queue during the startup process or if it
     *                            was made based on during a basic update.
     */
    @VisibleForTesting
    @Override
    void processDownloadUpdateQueue(boolean isProcessingPending) {
        DownloadUpdate downloadUpdate = findInterestingDownloadUpdate();
        // If the selected downloadUpdate is not active, there are no active downloads left. Return.
        if (downloadUpdate == null || !isActive(downloadUpdate.mDownloadStatus)) return;

        // If the pinned notification is still active, return.
        if (mDownloadUpdateQueue.get(mPinnedNotificationId) != null
                && isActive(mDownloadUpdateQueue.get(mPinnedNotificationId).mDownloadStatus)) {
            return;
        }

        // Start or update the notification.
        attachNotificationToJob(downloadUpdate);

        // Clear out inactive download updates in queue if there is at least one active download.
        cleanDownloadUpdateQueue();
    }

    /** Helper code to start or update foreground service. */
    @VisibleForTesting
    void attachNotificationToJob(DownloadUpdate update) {
        Log.w(TAG, "attachNotificationToJob id: " + update.mNotificationId);

        int notificationId = update.mNotificationId;
        Notification notification = update.mNotification;
        if (notificationId == INVALID_NOTIFICATION_ID || notification == null) {
            return;
        }

        // TODO(shaktisahu): Attach notification to the job.
        // setNotification(notificationId, notification);

        mPinnedNotificationId = notificationId;
    }
}

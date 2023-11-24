// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.chrome.browser.download.DownloadSnackbarController.INVALID_NOTIFICATION_ID;

import android.app.Notification;
import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.chrome.browser.download.DownloadNotificationService.DownloadStatus;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * Base class responsible for managing foreground service or user-initiated jobs life cycle for
 * ensuring download continuity. A foreground service (for android < U) or user-initiated task
 * (for android >= U) is spawn to ensure that chrome doesn't get killed even if chrome is in
 * background when there are active downloads.
 */
public abstract class DownloadContinuityManager {
    protected static class DownloadUpdate {
        int mNotificationId;
        Notification mNotification;
        @DownloadStatus int mDownloadStatus;
        Context mContext;

        DownloadUpdate(
                int notificationId,
                @Nullable Notification notification,
                @DownloadStatus int downloadStatus,
                Context context) {
            mNotificationId = notificationId;
            mNotification = notification;
            mDownloadStatus = downloadStatus;
            mContext = context;
        }
    }

    private static final String TAG = "DownloadCm";

    protected int mPinnedNotificationId = INVALID_NOTIFICATION_ID;

    @VisibleForTesting
    protected final Map<Integer, DownloadUpdate> mDownloadUpdateQueue = new HashMap<>();

    public DownloadContinuityManager() {}

    /**
     * Updates download notification status. For in-progress downloads, a notification will have a
     * foreground service or a job associated. If all notifications are not in progress, foreground
     * service or job will stop.
     * @param context Android {@link Context}.
     * @param downloadStatus Download status.
     * @param notificationId The notification id.
     * @param notification The notification associated with the id. Can be null if
     *     {@link DownloadNotificationService} tries to cancel a notification.
     */
    public void updateDownloadStatus(
            Context context,
            @DownloadStatus int downloadStatus,
            int notificationId,
            @Nullable Notification notification) {
        if (!isEnabled()) return;
        if (downloadStatus != DownloadStatus.IN_PROGRESS) {
            Log.w(
                    TAG,
                    "updateDownloadStatus status: " + downloadStatus + ", id: " + notificationId);
        }
        mDownloadUpdateQueue.put(
                notificationId,
                new DownloadUpdate(notificationId, notification, downloadStatus, context));
        processDownloadUpdateQueue(false /* not isProcessingPending */);
    }

    /**
     * Process the notification queue for all cases and initiate any needed actions.
     * @param isProcessingPending Whether the call was made to process pending notifications that
     *                            have accumulated in the queue during the startup process or if it
     *                            was made based on during a basic update.
     */
    abstract void processDownloadUpdateQueue(boolean isProcessingPending);

    /** Whether this manager is enabled. */
    abstract boolean isEnabled();

    /** Helper code to process download update queue. */
    protected @Nullable DownloadUpdate findInterestingDownloadUpdate() {
        Iterator<Map.Entry<Integer, DownloadUpdate>> entries =
                mDownloadUpdateQueue.entrySet().iterator();
        while (entries.hasNext()) {
            Map.Entry<Integer, DownloadUpdate> entry = entries.next();
            // Return an active entry if possible.
            if (isActive(entry.getValue().mDownloadStatus)) return entry.getValue();
            // If there are no active entries, just return the last entry.
            if (!entries.hasNext()) return entry.getValue();
        }
        // If there's no entries, return null.
        return null;
    }

    /** @return Whether the download is active. */
    protected boolean isActive(@DownloadStatus int downloadStatus) {
        return downloadStatus == DownloadStatus.IN_PROGRESS;
    }

    protected void cleanDownloadUpdateQueue() {
        Iterator<Map.Entry<Integer, DownloadUpdate>> entries =
                mDownloadUpdateQueue.entrySet().iterator();
        while (entries.hasNext()) {
            Map.Entry<Integer, DownloadUpdate> entry = entries.next();
            // Remove entry that is not active or pinned.
            if (!isActive(entry.getValue().mDownloadStatus)
                    && entry.getValue().mNotificationId != mPinnedNotificationId) {
                entries.remove();
            }
        }
    }
}

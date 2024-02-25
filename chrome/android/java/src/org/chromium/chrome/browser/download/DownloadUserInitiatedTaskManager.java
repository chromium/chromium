// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.chrome.browser.download.DownloadSnackbarController.INVALID_NOTIFICATION_ID;

import android.app.Notification;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;

import java.util.HashMap;
import java.util.Map;

/**
 * User initiated jobs implementation of {@link DownloadContinuityManager} that plays a role in
 * keeping chrome alive when there are active downloads. This class is only responsible for
 * attaching the notification to the job life cycle. Starting and stopping of jobs is
 * handled in AutoResumptionHandler in native. Only active for Android versions >= U.
 */
public class DownloadUserInitiatedTaskManager extends DownloadContinuityManager {
    private static final String TAG = "DownloadUitm";

    private Map<Integer, TaskFinishedCallback> mTaskNotificationCallbacks = new HashMap<>();

    /** Constructor.*/
    public DownloadUserInitiatedTaskManager() {}

    /**
     * Called to add a callback that will be run to attach a notification to the background task
     * life-cycle.
     *
     * @param taskNotificationCallback The callback to be invoked to attach notification.
     */
    public void setTaskNotificationCallback(
            int taskId, TaskFinishedCallback taskNotificationCallback) {
        if (taskNotificationCallback == null) {
            mTaskNotificationCallbacks.remove(taskId);
        } else {
            mTaskNotificationCallbacks.put(taskId, taskNotificationCallback);
        }
    }

    @Override
    boolean isEnabled() {
        return DownloadUtils.shouldUseUserInitiatedJobs();
    }

    /**
     * Process the notification queue for all cases and initiate any needed actions, i.e. attach
     * the best download notification to the background job.
     * @param isProcessingPending Unused.
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

    /** Helper code to attach notification the Job. */
    @VisibleForTesting
    void attachNotificationToJob(DownloadUpdate update) {
        Log.w(TAG, "attachNotificationToJob id: " + update.mNotificationId);

        int notificationId = update.mNotificationId;
        Notification notification = update.mNotification;
        if (notificationId == INVALID_NOTIFICATION_ID || notification == null) {
            return;
        }

        if (mTaskNotificationCallbacks.isEmpty()) return;

        // Attach notification to the job.
        for (TaskFinishedCallback taskFinishedCallback : mTaskNotificationCallbacks.values()) {
            taskFinishedCallback.setNotification(notificationId, notification);
        }

        mPinnedNotificationId = notificationId;
    }
}

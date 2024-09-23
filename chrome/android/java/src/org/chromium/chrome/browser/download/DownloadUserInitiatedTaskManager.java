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

    /**
     * Notification callbacks for background jobs in progress. One callback for each type of Job.
     * Cleared only when a job is stopped or completed. There are few subtleties: 1. Since there can
     * be multiple downloads, we keep the callback around so that it can be reattached with a new
     * notification in case the download completes and there are more downloads still in progress.
     * The job is completed only when all the downloads meeting network conditions are completed. 2.
     * We only clear the callback when the job is stopped or completed invoked from above via {@code
     * setTaskNotificationCallback}. 3. We also don't want to invoke the same callback again and
     * again with the same download. This is possible since a callback can span across multiple
     * downloads or a download can span across multiple callbacks. This is accomplished by
     * maintaining a boolean {@code mHasUnseenCallbacks} which is set when a new callback is
     * received.
     */
    private Map<Integer, TaskFinishedCallback> mTaskNotificationCallbacks = new HashMap<>();

    /**
     * Accounts for callbacks for jobs started that haven't yet been attached with a notification.
     * See documentation above.
     */
    private boolean mHasUnseenCallbacks;

    /** Constructor. */
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
            mHasUnseenCallbacks = true;
            mTaskNotificationCallbacks.put(taskId, taskNotificationCallback);
        }
    }

    @Override
    boolean isEnabled() {
        return DownloadUtils.shouldUseUserInitiatedJobs();
    }

    /**
     * Process the notification queue for all cases and initiate any needed actions, i.e. attach the
     * best download notification to the background job.
     *
     * @param isProcessingPending Unused.
     */
    @VisibleForTesting
    @Override
    void processDownloadUpdateQueue(boolean isProcessingPending) {
        DownloadUpdate downloadUpdate = findInterestingDownloadUpdate();
        // If the selected downloadUpdate is not active, there are no active downloads left. Return.
        if (downloadUpdate == null || !isActive(downloadUpdate.mDownloadStatus)) return;

        // If the pinned notification is still active and we already have processed all the
        // callbacks, return.
        if (mDownloadUpdateQueue.get(mPinnedNotificationId) != null
                && isActive(mDownloadUpdateQueue.get(mPinnedNotificationId).mDownloadStatus)
                && !mHasUnseenCallbacks) {
            return;
        }

        // This is an active download. Notify JobScheduler with a notification as we haven't done it
        // already.
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

        // Attach notification to the job. Note, we don't clear the callbacks here since it's
        // possible that the download ends but another download starts thereby changing the pinned
        // notification ID. The API needs to be reinvoked with the new notification ID to avoid ANR.
        // We only clear the callback from above when the job is not running or completed.
        for (TaskFinishedCallback taskFinishedCallback : mTaskNotificationCallbacks.values()) {
            taskFinishedCallback.setNotification(notificationId, notification);
        }

        mHasUnseenCallbacks = false;

        mPinnedNotificationId = notificationId;
    }
}

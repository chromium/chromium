// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.os.Handler;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.PendingState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Iterator;
import java.util.PriorityQueue;

/**
 * DownloadNotifier implementation that creates and updates download notifications.
 * This class creates the {@link DownloadNotificationService} when needed, and binds
 * to the latter to issue calls to show and update notifications.
 */
public class SystemDownloadNotifier implements DownloadNotifier {
    // To avoid notification updates being throttled by Android, using 220 ms as the interavl
    // so that no more than 5 updates are posted per second.
    private static final long UPDATE_DELAY_MILLIS = 220;
    private final PriorityQueue<NotificationInfo> mPendingNotificationUpdates =
            new PriorityQueue<NotificationInfo>(5,
                    (n1, n2)
                            -> n1.mPriority == n2.mPriority ? (int) (n1.mTimestamp - n2.mTimestamp)
                                                            : n1.mPriority - n2.mPriority);
    private Handler mHandler;
    private DownloadNotificationService mDownloadNotificationService;
    private boolean mIsNotificationUpdateScheduled;

    @IntDef({NotificationPriority.HIGH, NotificationPriority.LOW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface NotificationPriority {
        int HIGH = 0;
        int LOW = 1;
    }

    /**
     * Notification type for constructing the notification later on.
     * TODO(qinmin): this is very ugly and it doesn't scale if we want a more general notification
     * frame work. A better solution is to pass a notification builder or a notification into the
     * queue, so we don't need the switch statement in updateNotification().
     */
    @IntDef({NotificationType.PROGRESS, NotificationType.PAUSED, NotificationType.SUCCEEDED,
            NotificationType.FAILED, NotificationType.INTERRUPTED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface NotificationType {
        int PROGRESS = 0;
        int PAUSED = 1;
        int SUCCEEDED = 2;
        int FAILED = 3;
        int INTERRUPTED = 4;
    }

    /**
     * Information related to a notification.
     */
    private static final class NotificationInfo {
        final @NotificationType int mType;
        final DownloadInfo mInfo;
        final @NotificationPriority int mPriority;
        long mTimestamp;
        long mStartTime;
        long mSystemDownloadId;
        boolean mCanResolve;
        boolean mIsSupportedMimeType;
        boolean mCanDownloadWhileMetered;
        boolean mIsAutoResumable;
        @PendingState
        int mPendingState;

        NotificationInfo(
                @NotificationType int type, DownloadInfo info, @NotificationPriority int priority) {
            mType = type;
            mInfo = info;
            mPriority = priority;
            mTimestamp = SystemClock.uptimeMillis();
        }
    }

    /**
     * Constructor.
     */
    public SystemDownloadNotifier() {}

    DownloadNotificationService getDownloadNotificationService() {
        if (mDownloadNotificationService == null) {
            mDownloadNotificationService = DownloadNotificationService.getInstance();
        }
        return mDownloadNotificationService;
    }

    @VisibleForTesting
    void setDownloadNotificationService(DownloadNotificationService downloadNotificationService) {
        mDownloadNotificationService = downloadNotificationService;
    }

    private Handler getHandler() {
        if (mHandler == null) {
            mHandler = new Handler();
        }
        return mHandler;
    }

    @VisibleForTesting
    void setHandler(Handler handler) {
        mHandler = handler;
    }

    @Override
    public void notifyDownloadCanceled(ContentId id) {
        removePendingNotificationAndGetTimestamp(id);
        getDownloadNotificationService().notifyDownloadCanceled(id, false);
    }

    @Override
    public void notifyDownloadSuccessful(DownloadInfo info, long systemDownloadId,
            boolean canResolve, boolean isSupportedMimeType) {
        NotificationInfo notificationInfo =
                new NotificationInfo(NotificationType.SUCCEEDED, info, NotificationPriority.HIGH);
        notificationInfo.mSystemDownloadId = systemDownloadId;
        notificationInfo.mCanResolve = canResolve;
        notificationInfo.mIsSupportedMimeType = isSupportedMimeType;
        addPendingNotification(notificationInfo);
    }

    @Override
    public void notifyDownloadFailed(DownloadInfo info) {
        NotificationInfo notificationInfo =
                new NotificationInfo(NotificationType.FAILED, info, NotificationPriority.HIGH);
        addPendingNotification(notificationInfo);
    }

    @Override
    public void notifyDownloadProgress(
            DownloadInfo info, long startTime, boolean canDownloadWhileMetered) {
        NotificationInfo notificationInfo =
                new NotificationInfo(NotificationType.PROGRESS, info, NotificationPriority.LOW);
        notificationInfo.mStartTime = startTime;
        notificationInfo.mCanDownloadWhileMetered = canDownloadWhileMetered;
        addPendingNotification(notificationInfo);
    }

    @Override
    public void notifyDownloadPaused(DownloadInfo info) {
        NotificationInfo notificationInfo =
                new NotificationInfo(NotificationType.PAUSED, info, NotificationPriority.HIGH);
        addPendingNotification(notificationInfo);
    }

    @Override
    public void notifyDownloadInterrupted(
            DownloadInfo info, boolean isAutoResumable, @PendingState int pendingState) {
        NotificationInfo notificationInfo =
                new NotificationInfo(NotificationType.INTERRUPTED, info, NotificationPriority.HIGH);
        notificationInfo.mIsAutoResumable = isAutoResumable;
        notificationInfo.mPendingState = pendingState;
        addPendingNotification(notificationInfo);
    }

    @Override
    public void removeDownloadNotification(int notificationId, DownloadInfo info) {
        removePendingNotificationAndGetTimestamp(info.getContentId());
        getDownloadNotificationService().cancelNotification(notificationId, info.getContentId());
    }

    @Override
    public void resumePendingDownloads() {
        if (DownloadNotificationService.isTrackingResumableDownloads(
                    ContextUtils.getApplicationContext())) {
            getDownloadNotificationService().resumeAllPendingDownloads();
        }
    }

    /**
     * Add a new notification to be handled. If there is currently a posted task to handle pending
     * notifications, adding the new notification to the pending queue. Otherwise, process the
     * notification immediately and post a task to handle incoming ones.
     * @param notificationInfo Notification to be displayed.
     */
    void addPendingNotification(NotificationInfo notificationInfo) {
        long timestamp =
                removePendingNotificationAndGetTimestamp(notificationInfo.mInfo.getContentId());
        if (timestamp > 0) {
            // Use the old timestamp, so notifications won't starve.
            notificationInfo.mTimestamp = timestamp;
        }
        if (mIsNotificationUpdateScheduled) {
            mPendingNotificationUpdates.add(notificationInfo);
        } else {
            mIsNotificationUpdateScheduled = true;
            updateNotification(notificationInfo);
            getHandler().postDelayed(() -> { handlePendingNotifications(); }, UPDATE_DELAY_MILLIS);
        }
    }

    /**
     * Removes a enqueued notification given its content Id, and returns its timestamp.
     * If the notification is not found, return -1.
     * @param contentId ContentId of the notification.
     * @return Timestamp of the removed notification, or -1 if not found.
     */
    private long removePendingNotificationAndGetTimestamp(ContentId contentId) {
        Iterator<NotificationInfo> iter = mPendingNotificationUpdates.iterator();
        while (iter.hasNext()) {
            NotificationInfo info = iter.next();
            if (info.mInfo.getContentId().equals(contentId)) {
                long timestamp = info.mTimestamp;
                iter.remove();
                return timestamp;
            }
        }
        return -1;
    }

    /**
     * Sends a notification update to Android NotificationManager.
     * @param notificationInfo Information about the notification to be updated.
     */
    private void updateNotification(NotificationInfo notificationInfo) {
        DownloadInfo info = notificationInfo.mInfo;
        switch (notificationInfo.mType) {
            case NotificationType.PROGRESS:
                getDownloadNotificationService().notifyDownloadProgress(info.getContentId(),
                        info.getFileName(), info.getProgress(), info.getBytesReceived(),
                        info.getTimeRemainingInMillis(), notificationInfo.mStartTime,
                        info.isOffTheRecord(), notificationInfo.mCanDownloadWhileMetered,
                        info.getIsTransient(), info.getIcon(), info.getOriginalUrl(),
                        info.getShouldPromoteOrigin());
                break;
            case NotificationType.PAUSED:
                getDownloadNotificationService().notifyDownloadPaused(info.getContentId(),
                        info.getFileName(), true, false, info.isOffTheRecord(),
                        info.getIsTransient(), info.getIcon(), info.getOriginalUrl(),
                        info.getShouldPromoteOrigin(), false, true, info.getPendingState());
                break;
            case NotificationType.SUCCEEDED:
                final int notificationId =
                        getDownloadNotificationService().notifyDownloadSuccessful(
                                info.getContentId(), info.getFilePath(), info.getFileName(),
                                notificationInfo.mSystemDownloadId, info.isOffTheRecord(),
                                notificationInfo.mIsSupportedMimeType, info.getIsOpenable(),
                                info.getIcon(), info.getOriginalUrl(),
                                info.getShouldPromoteOrigin(), info.getReferrer(),
                                info.getBytesTotalSize());

                if (info.getIsOpenable()) {
                    DownloadManagerService.getDownloadManagerService().onSuccessNotificationShown(
                            info, notificationInfo.mCanResolve, notificationId,
                            notificationInfo.mSystemDownloadId);
                }
                break;
            case NotificationType.FAILED:
                getDownloadNotificationService().notifyDownloadFailed(info.getContentId(),
                        info.getFileName(), info.getIcon(), info.getOriginalUrl(),
                        info.getShouldPromoteOrigin(), info.isOffTheRecord(), info.getFailState());
                break;
            case NotificationType.INTERRUPTED:
                getDownloadNotificationService().notifyDownloadPaused(info.getContentId(),
                        info.getFileName(), info.isResumable(), notificationInfo.mIsAutoResumable,
                        info.isOffTheRecord(), info.getIsTransient(), info.getIcon(),
                        info.getOriginalUrl(), info.getShouldPromoteOrigin(), false, false,
                        notificationInfo.mPendingState);
                break;
        }
    }

    /**
     * Process the pending notifications from the priority queue.
     */
    private void handlePendingNotifications() {
        NotificationInfo info = mPendingNotificationUpdates.poll();
        if (info != null) {
            updateNotification(info);
            getHandler().postDelayed(() -> { handlePendingNotifications(); }, UPDATE_DELAY_MILLIS);
        } else {
            mIsNotificationUpdateScheduled = false;
        }
    }
}

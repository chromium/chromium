// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.notifications.PendingNotificationTask;
import org.chromium.components.browser_ui.notifications.ThrottlingNotificationScheduler;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.PendingState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * DownloadNotifier implementation that creates and updates download notifications.
 * This class creates the {@link DownloadNotificationService} when needed, and binds
 * to the latter to issue calls to show and update notifications.
 */
public class SystemDownloadNotifier implements DownloadNotifier {
    private DownloadNotificationService mDownloadNotificationService;

    /**
     * Notification type for constructing the notification later on.
     * TODO(qinmin): this is very ugly and it doesn't scale if we want a more general notification
     * frame work. A better solution is to pass a notification builder or a notification into the
     * queue, so we don't need the switch statement in updateNotification().
     */
    @IntDef({
        NotificationType.PROGRESS,
        NotificationType.PAUSED,
        NotificationType.SUCCEEDED,
        NotificationType.FAILED,
        NotificationType.INTERRUPTED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NotificationType {
        int PROGRESS = 0;
        int PAUSED = 1;
        int SUCCEEDED = 2;
        int FAILED = 3;
        int INTERRUPTED = 4;
    }

    /** Information related to a notification. */
    private static final class NotificationInfo {
        final @NotificationType int mType;
        final DownloadInfo mInfo;
        final @PendingNotificationTask.Priority int mPriority;
        long mStartTime;
        long mSystemDownloadId;
        boolean mCanResolve;
        boolean mIsSupportedMimeType;
        boolean mCanDownloadWhileMetered;
        boolean mIsAutoResumable;
        @PendingState int mPendingState;

        NotificationInfo(
                @NotificationType int type,
                DownloadInfo info,
                @PendingNotificationTask.Priority int priority) {
            mType = type;
            mInfo = info;
            mPriority = priority;
        }
    }

    /** Constructor. */
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

    @Override
    public void notifyDownloadCanceled(ContentId id) {
        ThrottlingNotificationScheduler.getInstance().cancelPendingNotificationTask(id);
        getDownloadNotificationService().notifyDownloadCanceled(id, false);
    }

    @Override
    public void notifyDownloadSuccessful(
            DownloadInfo info,
            long systemDownloadId,
            boolean canResolve,
            boolean isSupportedMimeType) {
        NotificationInfo notificationInfo =
                new NotificationInfo(
                        NotificationType.SUCCEEDED, info, PendingNotificationTask.Priority.HIGH);
        notificationInfo.mSystemDownloadId = systemDownloadId;
        notificationInfo.mCanResolve = canResolve;
        notificationInfo.mIsSupportedMimeType = isSupportedMimeType;
        addPendingNotification(notificationInfo);
    }

    @Override
    public void notifyDownloadFailed(DownloadInfo info) {
        NotificationInfo notificationInfo =
                new NotificationInfo(
                        NotificationType.FAILED, info, PendingNotificationTask.Priority.HIGH);
        addPendingNotification(notificationInfo);
    }

    @Override
    public void notifyDownloadProgress(
            DownloadInfo info, long startTime, boolean canDownloadWhileMetered) {
        NotificationInfo notificationInfo =
                new NotificationInfo(
                        NotificationType.PROGRESS, info, PendingNotificationTask.Priority.LOW);
        notificationInfo.mStartTime = startTime;
        notificationInfo.mCanDownloadWhileMetered = canDownloadWhileMetered;
        addPendingNotification(notificationInfo);
    }

    @Override
    public void notifyDownloadPaused(DownloadInfo info) {
        NotificationInfo notificationInfo =
                new NotificationInfo(
                        NotificationType.PAUSED, info, PendingNotificationTask.Priority.HIGH);
        addPendingNotification(notificationInfo);
    }

    @Override
    public void notifyDownloadInterrupted(
            DownloadInfo info, boolean isAutoResumable, @PendingState int pendingState) {
        NotificationInfo notificationInfo =
                new NotificationInfo(
                        NotificationType.INTERRUPTED, info, PendingNotificationTask.Priority.HIGH);
        notificationInfo.mIsAutoResumable = isAutoResumable;
        notificationInfo.mPendingState = pendingState;
        addPendingNotification(notificationInfo);
    }

    @Override
    public void removeDownloadNotification(int notificationId, DownloadInfo info) {
        ThrottlingNotificationScheduler.getInstance()
                .cancelPendingNotificationTask(info.getContentId());
        getDownloadNotificationService().cancelNotification(notificationId, info.getContentId());
    }

    /**
     * Add a new notification to be handled. If there is currently a posted task to handle pending
     * notifications, adding the new notification to the pending queue. Otherwise, process the
     * notification immediately and post a task to handle incoming ones.
     * @param notificationInfo Notification to be displayed.
     */
    void addPendingNotification(NotificationInfo notificationInfo) {
        ThrottlingNotificationScheduler.getInstance()
                .addPendingNotificationTask(
                        new PendingNotificationTask(
                                notificationInfo.mInfo.getContentId(),
                                notificationInfo.mPriority,
                                () -> {
                                    updateNotification(notificationInfo);
                                }));
    }

    /**
     * Sends a notification update to Android NotificationManager.
     * @param notificationInfo Information about the notification to be updated.
     */
    private void updateNotification(NotificationInfo notificationInfo) {
        DownloadInfo info = notificationInfo.mInfo;
        switch (notificationInfo.mType) {
            case NotificationType.PROGRESS:
                getDownloadNotificationService()
                        .notifyDownloadProgress(
                                info.getContentId(),
                                info.getFileName(),
                                info.getProgress(),
                                info.getBytesReceived(),
                                info.getTimeRemainingInMillis(),
                                notificationInfo.mStartTime,
                                info.getOTRProfileId(),
                                notificationInfo.mCanDownloadWhileMetered,
                                info.getIsTransient(),
                                info.getIcon(),
                                info.getOriginalUrl(),
                                info.getShouldPromoteOrigin());
                break;
            case NotificationType.PAUSED:
                getDownloadNotificationService()
                        .notifyDownloadPaused(
                                info.getContentId(),
                                info.getFileName(),
                                true,
                                false,
                                info.getOTRProfileId(),
                                info.getIsTransient(),
                                info.getIcon(),
                                info.getOriginalUrl(),
                                info.getShouldPromoteOrigin(),
                                false,
                                true,
                                info.getPendingState());
                break;
            case NotificationType.SUCCEEDED:
                final int notificationId =
                        getDownloadNotificationService()
                                .notifyDownloadSuccessful(
                                        info.getContentId(),
                                        info.getFilePath(),
                                        info.getFileName(),
                                        notificationInfo.mSystemDownloadId,
                                        info.getOTRProfileId(),
                                        notificationInfo.mIsSupportedMimeType,
                                        info.getIsOpenable(),
                                        info.getIcon(),
                                        info.getOriginalUrl(),
                                        info.getShouldPromoteOrigin(),
                                        info.getReferrer(),
                                        info.getBytesTotalSize());

                if (info.getIsOpenable()) {
                    DownloadManagerService.getDownloadManagerService()
                            .onSuccessNotificationShown(
                                    info,
                                    notificationInfo.mCanResolve,
                                    notificationId,
                                    notificationInfo.mSystemDownloadId);
                }
                break;
            case NotificationType.FAILED:
                getDownloadNotificationService()
                        .notifyDownloadFailed(
                                info.getContentId(),
                                info.getFileName(),
                                info.getIcon(),
                                info.getOriginalUrl(),
                                info.getShouldPromoteOrigin(),
                                info.getOTRProfileId(),
                                info.getFailState());
                break;
            case NotificationType.INTERRUPTED:
                getDownloadNotificationService()
                        .notifyDownloadPaused(
                                info.getContentId(),
                                info.getFileName(),
                                info.isResumable(),
                                notificationInfo.mIsAutoResumable,
                                info.getOTRProfileId(),
                                info.getIsTransient(),
                                info.getIcon(),
                                info.getOriginalUrl(),
                                info.getShouldPromoteOrigin(),
                                false,
                                false,
                                notificationInfo.mPendingState);
                break;
        }
    }
}

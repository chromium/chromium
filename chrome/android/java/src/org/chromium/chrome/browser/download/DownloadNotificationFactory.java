// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static android.app.DownloadManager.ACTION_NOTIFICATION_CLICKED;

import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_CANCEL;
import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_OPEN;
import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_PAUSE;
import static org.chromium.chrome.browser.download.DownloadNotificationService.ACTION_DOWNLOAD_RESUME;
import static org.chromium.chrome.browser.download.DownloadNotificationService.EXTRA_DOWNLOAD_CONTENTID_ID;
import static org.chromium.chrome.browser.download.DownloadNotificationService.EXTRA_DOWNLOAD_CONTENTID_NAMESPACE;
import static org.chromium.chrome.browser.download.DownloadNotificationService.EXTRA_IS_OFF_THE_RECORD;
import static org.chromium.chrome.browser.download.DownloadNotificationService.EXTRA_NOTIFICATION_BUNDLE_ICON_ID;
import static org.chromium.chrome.browser.download.DownloadNotificationService.EXTRA_OTR_PROFILE_ID;

import android.app.DownloadManager;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.core.app.NotificationCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineItem;

/** Creates and updates notifications related to downloads. */
public final class DownloadNotificationFactory {
    // Limit file name to 25 characters. TODO(qinmin): use different limit for different devices?
    public static final int MAX_FILE_NAME_LENGTH = 25;

    // Limit the origin length so that the eTLD+1 cannot be hidden. If the origin exceeds this
    // length the eTLD+1 is extracted and shown.
    public static final int MAX_ORIGIN_LENGTH = 40;

    // Time out duration for success and failed download notification.
    private static final long TIME_OUT_DURATION_IN_MILLIS = 60 * 60 * 1000;

    private static <T> void checkNotNull(T reference) {
        if (reference == null) {
            throw new NullPointerException();
        }
    }

    private static void checkArgument(boolean expression) {
        if (!expression) {
            throw new IllegalArgumentException();
        }
    }

    /**
     * Builds a downloads notification based on the status of the download and its information. All
     * changes to this function should consider the difference between normal profile and off the
     * record profile.
     * @param context of the download.
     * @param downloadStatus (in progress, paused, successful, failed, deleted, or summary).
     * @param downloadUpdate information about the download (ie. contentId, fileName, icon,
     * isOffTheRecord, etc).
     * @param notificationId The notification id passed to {@link
     *         android.app.NotificationManager#notify(String, int, Notification)}.
     * @return Notification that is built based on these parameters.
     */
    public static Notification buildNotification(
            Context context,
            @DownloadNotificationService.DownloadStatus int downloadStatus,
            DownloadUpdate downloadUpdate,
            int notificationId) {
        // TODO(xingliu): Write a unit test for this class.
        String channelId = ChromeChannelDefinitions.ChannelId.DOWNLOADS;
        if (LegacyHelpers.isLegacyDownload(downloadUpdate.getContentId())
                && downloadStatus == DownloadNotificationService.DownloadStatus.COMPLETED) {
            channelId = ChromeChannelDefinitions.ChannelId.COMPLETED_DOWNLOADS;
        }
        var metadata =
                new NotificationMetadata(
                        LegacyHelpers.isLegacyDownload(downloadUpdate.getContentId())
                                ? NotificationUmaTracker.SystemNotificationType.DOWNLOAD_FILES
                                : NotificationUmaTracker.SystemNotificationType.DOWNLOAD_PAGES,
                        /* tag= */ null,
                        notificationId);
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                channelId, metadata)
                        .setLocalOnly(true)
                        .setGroup(NotificationConstants.GROUP_DOWNLOADS)
                        .setAutoCancel(true);

        String contentText;
        int iconId;
        @NotificationUmaTracker.ActionType int cancelActionType;
        @NotificationUmaTracker.ActionType int pauseActionType;
        @NotificationUmaTracker.ActionType int resumeActionType;
        if (LegacyHelpers.isLegacyDownload(downloadUpdate.getContentId())) {
            cancelActionType = NotificationUmaTracker.ActionType.DOWNLOAD_CANCEL;
            pauseActionType = NotificationUmaTracker.ActionType.DOWNLOAD_PAUSE;
            resumeActionType = NotificationUmaTracker.ActionType.DOWNLOAD_RESUME;
        } else {
            cancelActionType = NotificationUmaTracker.ActionType.DOWNLOAD_PAGE_CANCEL;
            pauseActionType = NotificationUmaTracker.ActionType.DOWNLOAD_PAGE_PAUSE;
            resumeActionType = NotificationUmaTracker.ActionType.DOWNLOAD_PAGE_RESUME;
        }

        var resources = context.getResources();
        switch (downloadStatus) {
            case DownloadNotificationService.DownloadStatus.IN_PROGRESS:
                checkNotNull(downloadUpdate.getProgress());
                checkNotNull(downloadUpdate.getContentId());
                checkArgument(downloadUpdate.getNotificationId() != -1);

                if (downloadUpdate.getIsDownloadPending()) {
                    contentText =
                            StringUtils.getPendingStatusForUi(downloadUpdate.getPendingState());
                } else {
                    // Incognito mode should hide download progress details like file size.
                    OfflineItem.Progress progress =
                            downloadUpdate.getIsOffTheRecord()
                                    ? OfflineItem.Progress.createIndeterminateProgress()
                                    : downloadUpdate.getProgress();
                    contentText = StringUtils.getProgressTextForUi(progress);
                }

                iconId =
                        downloadUpdate.getIsDownloadPending()
                                ? R.drawable.ic_download_pending
                                : android.R.drawable.stat_sys_download;

                Intent pauseIntent =
                        buildActionIntent(
                                context,
                                ACTION_DOWNLOAD_PAUSE,
                                downloadUpdate.getContentId(),
                                downloadUpdate.getOTRProfileID());
                Intent cancelIntent =
                        buildActionIntent(
                                context,
                                ACTION_DOWNLOAD_CANCEL,
                                downloadUpdate.getContentId(),
                                downloadUpdate.getOTRProfileID());
                cancelIntent.putExtra(
                        NotificationConstants.EXTRA_NOTIFICATION_ID,
                        downloadUpdate.getNotificationId());
                builder.setOngoing(true)
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_HIGH)
                        .setAutoCancel(false)
                        .addAction(
                                R.drawable.ic_pause_white_24dp,
                                resources.getString(R.string.download_notification_pause_button),
                                buildPendingIntentProvider(
                                        context, pauseIntent, downloadUpdate.getNotificationId()),
                                pauseActionType)
                        .addAction(
                                R.drawable.btn_close_white,
                                resources.getString(R.string.download_notification_cancel_button),
                                buildPendingIntentProvider(
                                        context, cancelIntent, downloadUpdate.getNotificationId()),
                                cancelActionType);

                if (!downloadUpdate.getIsOffTheRecord()) {
                    builder.setLargeIcon(downloadUpdate.getIcon());
                }

                if (!downloadUpdate.getIsDownloadPending()) {
                    boolean indeterminate = downloadUpdate.getProgress().isIndeterminate();
                    builder.setProgress(
                            100,
                            indeterminate ? -1 : downloadUpdate.getProgress().getPercentage(),
                            indeterminate);
                }

                if (!downloadUpdate.getProgress().isIndeterminate()
                        && !downloadUpdate.getIsOffTheRecord()
                        && downloadUpdate.getTimeRemainingInMillis() >= 0
                        && !LegacyHelpers.isLegacyOfflinePage(downloadUpdate.getContentId())) {
                    String subText =
                            StringUtils.timeLeftForUi(
                                    context, downloadUpdate.getTimeRemainingInMillis());
                    builder.setSubText(subText);
                }

                if (downloadUpdate.getStartTime() > 0) {
                    builder.setWhen(downloadUpdate.getStartTime());
                }

                break;
            case DownloadNotificationService.DownloadStatus.PAUSED:
                checkNotNull(downloadUpdate.getContentId());
                checkArgument(downloadUpdate.getNotificationId() != -1);

                contentText = resources.getString(R.string.download_notification_paused);
                iconId = R.drawable.ic_download_pause;

                Intent resumeIntent =
                        buildActionIntent(
                                context,
                                ACTION_DOWNLOAD_RESUME,
                                downloadUpdate.getContentId(),
                                downloadUpdate.getOTRProfileID());
                cancelIntent =
                        buildActionIntent(
                                context,
                                ACTION_DOWNLOAD_CANCEL,
                                downloadUpdate.getContentId(),
                                downloadUpdate.getOTRProfileID());

                builder.setAutoCancel(false)
                        .addAction(
                                R.drawable.ic_file_download_white_24dp,
                                resources.getString(R.string.download_notification_resume_button),
                                buildPendingIntentProvider(
                                        context, resumeIntent, downloadUpdate.getNotificationId()),
                                resumeActionType)
                        .addAction(
                                R.drawable.btn_close_white,
                                resources.getString(R.string.download_notification_cancel_button),
                                buildPendingIntentProvider(
                                        context, cancelIntent, downloadUpdate.getNotificationId()),
                                cancelActionType);

                if (!downloadUpdate.getIsOffTheRecord()) {
                    builder.setLargeIcon(downloadUpdate.getIcon());
                }

                if (downloadUpdate.getIsTransient()) {
                    builder.setDeleteIntent(
                            buildPendingIntentProvider(
                                    context, cancelIntent, downloadUpdate.getNotificationId()));
                }

                break;
            case DownloadNotificationService.DownloadStatus.COMPLETED:
                checkArgument(downloadUpdate.getNotificationId() != -1);

                // Don't show file size in incognito mode.
                if (downloadUpdate.getTotalBytes() > 0 && !downloadUpdate.getIsOffTheRecord()) {
                    contentText =
                            resources.getString(
                                    R.string.download_notification_completed_with_size,
                                    org.chromium.components.browser_ui.util.DownloadUtils
                                            .getStringForBytes(
                                                    context, downloadUpdate.getTotalBytes()));
                } else {
                    contentText = resources.getString(R.string.download_notification_completed);
                }

                iconId = R.drawable.offline_pin;
                // Download from Android DownloadManager carries an empty namespace.
                if (TextUtils.isEmpty(downloadUpdate.getContentId().namespace)) {
                    // Create an intent to view all Android downloads.
                    Intent intent = new Intent(DownloadManager.ACTION_VIEW_DOWNLOADS);
                    intent.setFlags(
                            Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
                    builder.setContentIntent(
                            PendingIntentProvider.getActivity(
                                    context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT));
                } else if (downloadUpdate.getIsOpenable()) {
                    Intent intent =
                            buildActionIntent(
                                    context,
                                    ACTION_DOWNLOAD_OPEN,
                                    downloadUpdate.getContentId(),
                                    null);

                    ComponentName component =
                            new ComponentName(
                                    context.getPackageName(),
                                    DownloadBroadcastManager.class.getName());
                    intent.setComponent(component);
                    builder.setContentIntent(
                            PendingIntentProvider.getService(
                                    context,
                                    downloadUpdate.getNotificationId(),
                                    intent,
                                    PendingIntent.FLAG_UPDATE_CURRENT));
                }

                // It's the job of the service to ensure that the default icon is provided when
                // in incognito mode.
                if (downloadUpdate.getIcon() != null) {
                    builder.setLargeIcon(downloadUpdate.getIcon());
                }
                builder.setTimeoutAfter(TIME_OUT_DURATION_IN_MILLIS);

                break;
            case DownloadNotificationService.DownloadStatus.FAILED:
                iconId = android.R.drawable.stat_sys_download_done;
                contentText = StringUtils.getFailStatusForUi(downloadUpdate.getFailState());
                builder.setTimeoutAfter(TIME_OUT_DURATION_IN_MILLIS);
                break;
            default:
                iconId = -1;
                contentText = "";
                break;
        }

        Bundle extras = new Bundle();
        extras.putInt(EXTRA_NOTIFICATION_BUNDLE_ICON_ID, iconId);
        builder.setSmallIcon(iconId).addExtras(extras);

        // Context text is shown as title in incognito mode as the file name is not shown.
        if (downloadUpdate.getIsOffTheRecord()) {
            builder.setContentTitle(contentText);
        } else {
            builder.setContentText(contentText);
        }

        // Don't show file name in incognito mode.
        if (downloadUpdate.getFileName() != null && !downloadUpdate.getIsOffTheRecord()) {
            builder.setContentTitle(
                    StringUtils.getAbbreviatedFileName(
                            downloadUpdate.getFileName(), MAX_FILE_NAME_LENGTH));
        }

        if (!downloadUpdate.getIsTransient()
                && downloadUpdate.getNotificationId() != -1
                && downloadStatus != DownloadNotificationService.DownloadStatus.COMPLETED
                && downloadStatus != DownloadNotificationService.DownloadStatus.FAILED) {
            Intent downloadHomeIntent =
                    buildActionIntent(
                            context,
                            ACTION_NOTIFICATION_CLICKED,
                            null,
                            downloadUpdate.getOTRProfileID());
            builder.setContentIntent(
                    PendingIntentProvider.getService(
                            context,
                            downloadUpdate.getNotificationId(),
                            downloadHomeIntent,
                            PendingIntent.FLAG_UPDATE_CURRENT));
        }

        if (downloadUpdate.getIsOffTheRecord()) {
            // A sub text to inform the users that they are using incognito mode.
            builder.setSubText(
                    resources.getString(R.string.download_notification_incognito_subtext));
        } else if (downloadUpdate.getShouldPromoteOrigin()) {
            // Always show the origin URL if available (for normal profiles).
            String formattedUrl =
                    DownloadUtils.formatUrlForDisplayInNotification(
                            downloadUpdate.getOriginalUrl(),
                            DownloadUtils.MAX_ORIGIN_LENGTH_FOR_NOTIFICATION);
            if (formattedUrl != null) builder.setSubText(formattedUrl);
        }

        return builder.build();
    }

    /**
     * Helper method to build a PendingIntent from the provided intent.
     * @param intent Intent to broadcast.
     * @param notificationId ID of the notification.
     */
    private static PendingIntentProvider buildPendingIntentProvider(
            Context context, Intent intent, int notificationId) {
        return PendingIntentProvider.getService(
                context, notificationId, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    /**
     * Helper method to build an download action Intent from the provided information.
     * @param context {@link Context} to pull resources from.
     * @param action Download action to perform.
     * @param id The {@link ContentId} of the download.
     * @param otrProfileID The {@link OTRProfileID} of the download. Null if in regular mode.
     */
    public static Intent buildActionIntent(
            Context context, String action, ContentId id, OTRProfileID otrProfileID) {
        ComponentName component =
                new ComponentName(
                        context.getPackageName(), DownloadBroadcastManager.class.getName());
        Intent intent = new Intent(action);
        intent.setComponent(component);
        intent.putExtra(EXTRA_DOWNLOAD_CONTENTID_ID, id != null ? id.id : "");
        intent.putExtra(EXTRA_DOWNLOAD_CONTENTID_NAMESPACE, id != null ? id.namespace : "");
        intent.putExtra(EXTRA_IS_OFF_THE_RECORD, OTRProfileID.isOffTheRecord(otrProfileID));
        intent.putExtra(EXTRA_OTR_PROFILE_ID, OTRProfileID.serialize(otrProfileID));
        return intent;
    }
}

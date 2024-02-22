// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing;

import android.app.Notification;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Build;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationType;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

/** Provides common functionality for handling sharing notifications. */
public final class SharingNotificationUtil {
    /**
     * Shows a notification with a configuration common to all sharing notifications.
     *
     * @param type The type of notification.
     * @param group The notification group.
     * @param id The notification id.
     * @param contentIntent The notification content intent.
     * @param deleteIntent The notification delete intent.
     * @param confirmIntent The notification confirm intent.
     * @param cancelIntent The notification cancel intent.
     * @param contentTitle The notification title text.
     * @param contentText The notification content text.
     * @param largeIconId The large notification icon resource id, 0 if not used.
     * @param color The color to be used for the notification.
     * @param startsActivity Whether the {@code contentIntent} starts an Activity.
     */
    public static void showNotification(
            @SystemNotificationType int type,
            String group,
            int id,
            PendingIntentProvider contentIntent,
            PendingIntentProvider deleteIntent,
            PendingIntentProvider confirmIntent,
            PendingIntentProvider cancelIntent,
            String contentTitle,
            String contentText,
            @DrawableRes int smallIconId,
            @DrawableRes int largeIconId,
            int color,
            boolean startsActivity) {
        Context context = ContextUtils.getApplicationContext();
        Resources resources = context.getResources();
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                ChromeChannelDefinitions.ChannelId.SHARING,
                                new NotificationMetadata(type, group, id))
                        .setContentTitle(contentTitle)
                        .setContentText(contentText)
                        .setBigTextStyle(contentText)
                        .setColor(context.getColor(color))
                        .setGroup(group)
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_HIGH)
                        .setSmallIcon(smallIconId)
                        .setAutoCancel(true)
                        .setDefaults(Notification.DEFAULT_ALL);

        if (contentIntent != null) {
            if (startsActivity && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                // We can't use the NotificationIntentInterceptor to start Activities starting in
                // Android S. Use the unmodified PendingIntent directly instead.
                builder.setContentIntent(contentIntent.getPendingIntent());
            } else {
                builder.setContentIntent(contentIntent);
            }
        }
        if (deleteIntent != null) {
            builder.setDeleteIntent(deleteIntent);
        }
        if (confirmIntent != null) {
            builder.addAction(
                    R.drawable.ic_checkmark_24dp,
                    resources.getString(R.string.submit),
                    confirmIntent,
                    NotificationUmaTracker.ActionType.SHARING_CONFIRM);
        }
        if (cancelIntent != null) {
            builder.addAction(
                    R.drawable.ic_cancel_circle,
                    resources.getString(R.string.cancel),
                    cancelIntent,
                    NotificationUmaTracker.ActionType.SHARING_CANCEL);
        }

        if (largeIconId != 0) {
            Bitmap largeIcon = BitmapFactory.decodeResource(resources, largeIconId);
            if (largeIcon != null) builder.setLargeIcon(largeIcon);
        }
        NotificationWrapper notification = builder.buildNotificationWrapper();

        BaseNotificationManagerProxyFactory.create(context).notify(notification);
        NotificationUmaTracker.getInstance()
                .onNotificationShown(type, notification.getNotification());
    }

    public static void dismissNotification(String tag, int notificationId) {
        Context context = ContextUtils.getApplicationContext();
        BaseNotificationManagerProxyFactory.create(context).cancel(tag, notificationId);
    }

    /**
     * Shows a notification for sending outgoing Sharing messages.
     *
     * @param type The type of notification.
     * @param group The notification group.
     * @param id The notification id.
     * @param targetName The name of target device
     */
    public static void showSendingNotification(
            @SystemNotificationType int type, String group, int id, String targetName) {
        Context context = ContextUtils.getApplicationContext();
        Resources resources = context.getResources();
        String contentTitle =
                resources.getString(R.string.sharing_sending_notification_title, targetName);
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                ChromeChannelDefinitions.ChannelId.SHARING,
                                new NotificationMetadata(type, group, id))
                        .setContentTitle(contentTitle)
                        .setGroup(group)
                        .setColor(context.getColor(R.color.default_icon_color_accent1_baseline))
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_HIGH)
                        .setSmallIcon(R.drawable.ic_devices_16dp)
                        .setProgress(/* max= */ 0, /* percentage= */ 0, true)
                        .setOngoing(true)
                        .setDefaults(Notification.DEFAULT_ALL);
        NotificationWrapper notification = builder.buildNotificationWrapper();

        BaseNotificationManagerProxyFactory.create(context).notify(notification);
        NotificationUmaTracker.getInstance()
                .onNotificationShown(type, notification.getNotification());
    }

    /**
     * Shows a notification for displaying error after sending outgoing Sharing message.
     *
     * @param type The type of notification.
     * @param group The notification group.
     * @param id The notification id.
     * @param contentTitle The title of the notification.
     * @param contentText The text shown in the notification.
     * @param tryAgainIntent PendingIntent to try sharing to same device again.
     */
    public static void showSendErrorNotification(
            @SystemNotificationType int type,
            String group,
            int id,
            String contentTitle,
            String contentText,
            @Nullable PendingIntentProvider tryAgainIntent) {
        Context context = ContextUtils.getApplicationContext();
        Resources resources = context.getResources();
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                ChromeChannelDefinitions.ChannelId.SHARING,
                                new NotificationMetadata(type, group, id))
                        .setContentTitle(contentTitle)
                        .setGroup(group)
                        .setColor(context.getColor(R.color.google_red_600))
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_HIGH)
                        .setSmallIcon(R.drawable.ic_error_outline_red_24dp)
                        .setContentText(contentText)
                        .setDefaults(Notification.DEFAULT_ALL)
                        .setAutoCancel(true);

        if (tryAgainIntent != null) {
            builder.setContentIntent(tryAgainIntent)
                    .addAction(
                            R.drawable.ic_cancel_circle,
                            resources.getString(R.string.try_again),
                            tryAgainIntent,
                            NotificationUmaTracker.ActionType.SHARING_TRY_AGAIN);
        }

        NotificationWrapper notification = builder.buildWithBigTextStyle(contentText);

        BaseNotificationManagerProxyFactory.create(context).notify(notification);
        NotificationUmaTracker.getInstance()
                .onNotificationShown(type, notification.getNotification());
    }

    private SharingNotificationUtil() {}
}

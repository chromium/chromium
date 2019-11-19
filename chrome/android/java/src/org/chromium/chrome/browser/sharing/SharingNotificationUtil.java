// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing;

import android.app.Notification;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.v4.app.NotificationCompat;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.ChromeNotification;
import org.chromium.chrome.browser.notifications.ChromeNotificationBuilder;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.browser.notifications.NotificationMetadata;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationType;
import org.chromium.chrome.browser.notifications.PendingIntentProvider;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;

/**
 * Provides common functionality for handling sharing notifications.
 */
public final class SharingNotificationUtil {
    // TODO(himanshujaju) - We have only two small icons, one for error and one for non error. We
    // could avoid passing them around.

    /**
     * Shows a notification with a configuration common to all sharing notifications.
     *
     * @param type The type of notification.
     * @param group The notification group.
     * @param id The notification id.
     * @param contentIntent The notification content intent.
     * @param contentTitle The notification title text.
     * @param contentText The notification content text.
     * @param largeIconId The large notification icon resource id, 0 if not used.
     * @param color The color to be used for the notification.
     */
    public static void showNotification(@SystemNotificationType int type, String group, int id,
            PendingIntentProvider contentIntent, String contentTitle, String contentText,
            @DrawableRes int smallIconId, @DrawableRes int largeIconId, int color) {
        Context context = ContextUtils.getApplicationContext();
        Resources resources = context.getResources();
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(/*preferCompat=*/true,
                                ChannelDefinitions.ChannelId.SHARING,
                                /*remoteAppPackageName=*/null,
                                new NotificationMetadata(type, group, id))
                        .setContentIntent(contentIntent)
                        .setContentTitle(contentTitle)
                        .setContentText(contentText)
                        .setColor(ApiCompatibilityUtils.getColor(context.getResources(), color))
                        .setGroup(group)
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_HIGH)
                        .setSmallIcon(smallIconId)
                        .setAutoCancel(true)
                        .setDefaults(Notification.DEFAULT_ALL);
        if (largeIconId != 0) {
            Bitmap largeIcon = BitmapFactory.decodeResource(resources, largeIconId);
            if (largeIcon != null) builder.setLargeIcon(largeIcon);
        }
        ChromeNotification notification = builder.buildChromeNotification();

        new NotificationManagerProxyImpl(context).notify(notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                type, notification.getNotification());
    }

    public static void dismissNotification(String tag, int notificationId) {
        Context context = ContextUtils.getApplicationContext();
        new NotificationManagerProxyImpl(context).cancel(tag, notificationId);
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
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(/*preferCompat=*/true,
                                ChannelDefinitions.ChannelId.SHARING,
                                /*remoteAppPackageName=*/null,
                                new NotificationMetadata(type, group, id))
                        .setContentTitle(contentTitle)
                        .setGroup(group)
                        .setColor(ApiCompatibilityUtils.getColor(
                                context.getResources(), R.color.default_icon_color_blue))
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_HIGH)
                        .setSmallIcon(R.drawable.ic_devices_16dp)
                        .setProgress(/*max=*/0, /*percentage=*/0, true)
                        .setOngoing(true)
                        .setDefaults(Notification.DEFAULT_ALL);
        ChromeNotification notification = builder.buildChromeNotification();

        new NotificationManagerProxyImpl(context).notify(notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                type, notification.getNotification());
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
    public static void showSendErrorNotification(@SystemNotificationType int type, String group,
            int id, String contentTitle, String contentText,
            @Nullable PendingIntentProvider tryAgainIntent) {
        Context context = ContextUtils.getApplicationContext();
        Resources resources = context.getResources();
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(/*preferCompat=*/true,
                                ChannelDefinitions.ChannelId.SHARING,
                                /*remoteAppPackageName=*/null,
                                new NotificationMetadata(type, group, id))
                        .setContentTitle(contentTitle)
                        .setGroup(group)
                        .setColor(ApiCompatibilityUtils.getColor(resources, R.color.google_red_600))
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_HIGH)
                        .setSmallIcon(R.drawable.ic_error_outline_red_24dp)
                        .setContentText(contentText)
                        .setDefaults(Notification.DEFAULT_ALL)
                        .setAutoCancel(true);

        if (tryAgainIntent != null) {
            builder.setContentIntent(tryAgainIntent)
                    .addAction(R.drawable.ic_cancel_circle, resources.getString(R.string.try_again),
                            tryAgainIntent, NotificationUmaTracker.ActionType.SHARING_TRY_AGAIN);
        }

        ChromeNotification notification = builder.buildWithBigTextStyle(contentText);

        new NotificationManagerProxyImpl(context).notify(notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                type, notification.getNotification());
    }

    private SharingNotificationUtil() {}
}

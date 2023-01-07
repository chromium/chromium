// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;
import androidx.core.app.NotificationCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.webapps.WebappsIconUtils;
import org.chromium.components.webapps.WebappsUtils;
import org.chromium.webapk.lib.client.WebApkNavigationClient;

/** Java counterpart to webapk_install_service.h. */
public class WebApkInstallService {
    /** Prefix used for generating a unique notification tag. */
    @VisibleForTesting
    static final String WEBAPK_INSTALL_NOTIFICATION_TAG_PREFIX =
            "webapk_install_notification_tag_prefix.";

    /** We always use the same platform id for notifications. */
    private static final int PLATFORM_ID = -1;

    /** Displays a notification when a WebAPK is successfully installed. */
    @CalledByNative
    @VisibleForTesting
    static void showInstalledNotification(String webApkPackage, String notificationId,
            String shortName, String url, Bitmap icon, boolean isIconMaskable) {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = WebApkNavigationClient.createLaunchWebApkIntent(webApkPackage, url, false
                /* forceNavigation */);
        PendingIntentProvider clickPendingIntent = PendingIntentProvider.getActivity(
                context, 0 /*requestCode */, intent, PendingIntent.FLAG_UPDATE_CURRENT);

        if (isIconMaskable && WebappsIconUtils.doesAndroidSupportMaskableIcons()) {
            icon = WebappsIconUtils.generateAdaptiveIconBitmap(icon);
        }

        showNotification(notificationId, shortName, url, icon,
                context.getResources().getString(R.string.notification_webapk_installed),
                clickPendingIntent, true /* isCompleted */);
    }

    /** Display a notification when an install starts. */
    @CalledByNative
    @VisibleForTesting
    static void showInstallInProgressNotification(String notificationId, String shortName,
            String url, Bitmap icon, boolean isIconMaskable) {
        String message = ContextUtils.getApplicationContext().getResources().getString(
                R.string.notification_webapk_install_in_progress, shortName);
        if (isIconMaskable && WebappsIconUtils.doesAndroidSupportMaskableIcons()) {
            icon = WebappsIconUtils.generateAdaptiveIconBitmap(icon);
        }
        showNotification(
                notificationId, shortName, url, icon, message, null, false /* isCompleted */);
        WebappsUtils.showToast(message);
    }

    private static void showNotification(String notificationId, String shortName, String url,
            Bitmap icon, String message, PendingIntentProvider clickPendingIntent,
            boolean isCompleted) {
        Context context = ContextUtils.getApplicationContext();
        String channelId;
        int preOPriority;
        if (isCompleted) {
            channelId = ChromeChannelDefinitions.ChannelId.WEBAPPS;
            preOPriority = NotificationCompat.PRIORITY_HIGH;
        } else {
            channelId = ChromeChannelDefinitions.ChannelId.BROWSER;
            preOPriority = NotificationCompat.PRIORITY_DEFAULT;
        }

        int umaType = isCompleted
                ? NotificationUmaTracker.SystemNotificationType.WEBAPK_INSTALL_COMPLETE
                : NotificationUmaTracker.SystemNotificationType.WEBAPK_INSTALL_IN_PROGRESS;

        NotificationMetadata metadata = new NotificationMetadata(
                umaType, WEBAPK_INSTALL_NOTIFICATION_TAG_PREFIX + notificationId, PLATFORM_ID);

        NotificationWrapperBuilder notificationBuilder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                        channelId, metadata);
        notificationBuilder.setContentTitle(shortName)
                .setContentText(message)
                .setLargeIcon(icon)
                .setSmallIcon(R.drawable.ic_chrome)
                .setContentIntent(clickPendingIntent)
                .setPriorityBeforeO(preOPriority)
                .setWhen(System.currentTimeMillis())
                .setSubText(UrlFormatter.formatUrlForSecurityDisplay(
                        url, SchemeDisplay.OMIT_HTTP_AND_HTTPS))
                .setAutoCancel(true);

        NotificationWrapper notification = notificationBuilder.buildNotificationWrapper();
        NotificationManagerProxy notificationManager = new NotificationManagerProxyImpl(context);
        notificationManager.notify(notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                umaType, notification.getNotification());
    }

    /** Cancels any ongoing notification for the WebAPK. */
    @CalledByNative
    private static void cancelNotification(String notificationId) {
        NotificationManagerProxy notificationManager =
                new NotificationManagerProxyImpl(ContextUtils.getApplicationContext());
        notificationManager.cancel(
                WEBAPK_INSTALL_NOTIFICATION_TAG_PREFIX + notificationId, PLATFORM_ID);
    }
}

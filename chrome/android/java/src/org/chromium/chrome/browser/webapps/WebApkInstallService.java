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

import org.jni_zero.CalledByNative;

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
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.webapps.WebApkInstallResult;
import org.chromium.components.webapps.WebappsIconUtils;
import org.chromium.components.webapps.WebappsUtils;
import org.chromium.webapk.lib.client.WebApkNavigationClient;

/** Java counterpart to webapk_install_service.h. */
public class WebApkInstallService {
    /** Prefix used for generating a unique notification tag. */
    static final String WEBAPK_INSTALL_NOTIFICATION_TAG_PREFIX =
            "webapk_install_notification_tag_prefix.";

    /** We always use the same platform id for notifications. */
    private static final int PLATFORM_ID = -1;

    /** Displays a notification when a WebAPK is successfully installed. */
    @CalledByNative
    @VisibleForTesting
    static void showInstalledNotification(
            String webApkPackage,
            String notificationId,
            String shortName,
            String url,
            Bitmap icon,
            boolean isIconMaskable) {
        Context context = ContextUtils.getApplicationContext();
        Intent intent =
                WebApkNavigationClient.createLaunchWebApkIntent(
                        webApkPackage, url, false
                        /* forceNavigation= */ );
        PendingIntentProvider clickPendingIntent =
                PendingIntentProvider.getActivity(
                        context, /* requestCode= */ 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);

        if (isIconMaskable && WebappsIconUtils.doesAndroidSupportMaskableIcons()) {
            icon = WebappsIconUtils.generateAdaptiveIconBitmap(icon);
        }

        showNotification(
                notificationId,
                SystemNotificationType.WEBAPK_INSTALL_COMPLETE,
                shortName,
                url,
                icon,
                context.getResources().getString(R.string.notification_webapk_installed),
                clickPendingIntent);
    }

    /** Display a notification when an install starts. */
    @CalledByNative
    @VisibleForTesting
    static void showInstallInProgressNotification(
            String notificationId,
            String shortName,
            String url,
            Bitmap icon,
            boolean isIconMaskable) {
        String message =
                ContextUtils.getApplicationContext()
                        .getResources()
                        .getString(R.string.notification_webapk_install_in_progress, shortName);
        if (isIconMaskable && WebappsIconUtils.doesAndroidSupportMaskableIcons()) {
            icon = WebappsIconUtils.generateAdaptiveIconBitmap(icon);
        }
        showNotification(
                notificationId,
                SystemNotificationType.WEBAPK_INSTALL_IN_PROGRESS,
                shortName,
                url,
                icon,
                message,
                null);
        WebappsUtils.showToast(message);
    }

    /** Display a notification when an install failed. */
    @CalledByNative
    @VisibleForTesting
    static void showInstallFailedNotification(
            String notificationId,
            String shortName,
            String url,
            Bitmap icon,
            boolean isIconMaskable,
            @WebApkInstallResult int resultCode) {
        Context context = ContextUtils.getApplicationContext();
        String titleMessage =
                context.getResources()
                        .getString(R.string.notification_webapk_install_failed, shortName);
        String contentMessage = getInstallErrorMessage(resultCode);

        PendingIntentProvider openUrlIntent =
                WebApkInstallBroadcastReceiver.createPendingIntent(
                        context,
                        notificationId,
                        url,
                        WebApkInstallBroadcastReceiver.ACTION_OPEN_IN_BROWSER);
        if (isIconMaskable && WebappsIconUtils.doesAndroidSupportMaskableIcons()) {
            icon = WebappsIconUtils.generateAdaptiveIconBitmap(icon);
        }
        showNotification(
                notificationId,
                SystemNotificationType.WEBAPK_INSTALL_FAILED,
                titleMessage,
                url,
                icon,
                contentMessage,
                openUrlIntent);
    }

    private static void showNotification(
            String notificationId,
            @SystemNotificationType int type,
            String shortName,
            String url,
            Bitmap icon,
            String message,
            PendingIntentProvider clickPendingIntent) {
        Context context = ContextUtils.getApplicationContext();

        String channelId;
        int preOPriority;
        if (type == SystemNotificationType.WEBAPK_INSTALL_IN_PROGRESS) {
            channelId = ChromeChannelDefinitions.ChannelId.BROWSER;
            preOPriority = NotificationCompat.PRIORITY_DEFAULT;
        } else {
            channelId = ChromeChannelDefinitions.ChannelId.WEBAPPS;
            preOPriority = NotificationCompat.PRIORITY_HIGH;
        }

        NotificationMetadata metadata =
                new NotificationMetadata(
                        type, getInstallNotificationTag(notificationId), PLATFORM_ID);

        NotificationWrapperBuilder notificationBuilder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                        channelId, metadata);
        notificationBuilder
                .setContentTitle(shortName)
                .setContentText(message)
                .setLargeIcon(icon)
                .setSmallIcon(R.drawable.ic_chrome)
                .setContentIntent(clickPendingIntent)
                .setPriorityBeforeO(preOPriority)
                .setWhen(System.currentTimeMillis())
                .setSubText(
                        UrlFormatter.formatUrlForSecurityDisplay(
                                url, SchemeDisplay.OMIT_HTTP_AND_HTTPS))
                .setAutoCancel(true);

        if (type == SystemNotificationType.WEBAPK_INSTALL_FAILED) {
            notificationBuilder.addAction(
                    0 /* no icon */,
                    context.getResources().getString(R.string.webapk_install_failed_action_open),
                    clickPendingIntent,
                    NotificationUmaTracker.ActionType.WEB_APK_ACTION_BACK_TO_SITE);
        }

        NotificationWrapper notification = notificationBuilder.buildNotificationWrapper();
        BaseNotificationManagerProxyFactory.create(context).notify(notification);
        NotificationUmaTracker.getInstance()
                .onNotificationShown(type, notification.getNotification());
    }

    /** Cancels any ongoing notification for the WebAPK. */
    @CalledByNative
    static void cancelNotification(String notificationId) {
        BaseNotificationManagerProxyFactory.create(ContextUtils.getApplicationContext())
                .cancel(getInstallNotificationTag(notificationId), PLATFORM_ID);
    }

    private static String getInstallErrorMessage(@WebApkInstallResult int resultCode) {
        String message;
        if (resultCode == WebApkInstallResult.NOT_ENOUGH_SPACE) {
            message =
                    ContextUtils.getApplicationContext()
                            .getResources()
                            .getString(R.string.notification_webapk_install_failed_space);
        } else {
            message =
                    ContextUtils.getApplicationContext()
                            .getResources()
                            .getString(
                                    R.string.notification_webapk_install_failed_contents_general);
        }
        return message;
    }

    static String getInstallNotificationTag(String notificationId) {
        return WebApkInstallService.WEBAPK_INSTALL_NOTIFICATION_TAG_PREFIX + notificationId;
    }
}

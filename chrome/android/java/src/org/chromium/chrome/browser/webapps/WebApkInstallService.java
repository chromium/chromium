// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static android.content.Context.NOTIFICATION_SERVICE;

import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.notifications.ChromeNotificationBuilder;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.webapk.lib.client.WebApkNavigationClient;

/** Java counterpart to webapk_install_service.h. */
public class WebApkInstallService {
    /** Prefix used for generating a unique notification tag. */
    private static final String WEBAPK_INSTALL_NOTIFICATION_TAG_PREFIX =
            "webapk_install_notification_tag_prefix.";

    /** We always use the same platform id for notifications. */
    private static final int PLATFORM_ID = -1;

    /** Displays a notification when a WebAPK is successfully installed. */
    @CalledByNative
    private static void showInstalledNotification(String webApkPackage, String manifestUrl,
            String shortName, String url, Bitmap icon, boolean isIconMaskable) {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = WebApkNavigationClient.createLaunchWebApkIntent(webApkPackage, url, false
                /* forceNavigation */);
        PendingIntent clickPendingIntent =
                PendingIntent.getActivity(context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);

        if (isIconMaskable && ShortcutHelper.doesAndroidSupportMaskableIcons()) {
            icon = ShortcutHelper.generateAdaptiveIconBitmap(icon);
        }

        showNotification(manifestUrl, shortName, url, icon,
                context.getResources().getString(R.string.notification_webapk_installed),
                clickPendingIntent);
    }

    /** Display a notification when an install starts. */
    @CalledByNative
    private static void showInstallInProgressNotification(
            String manifestUrl, String shortName, String url, Bitmap icon, boolean isIconMaskable) {
        String message = ContextUtils.getApplicationContext().getResources().getString(
                R.string.notification_webapk_install_in_progress, shortName);
        if (isIconMaskable && ShortcutHelper.doesAndroidSupportMaskableIcons()) {
            icon = ShortcutHelper.generateAdaptiveIconBitmap(icon);
        }
        showNotification(manifestUrl, shortName, url, icon, message, null);
        ShortcutHelper.showToast(message);
    }

    private static void showNotification(String notificationId, String shortName, String url,
            Bitmap icon, String message, PendingIntent clickPendingIntent) {
        Context context = ContextUtils.getApplicationContext();
        ChromeNotificationBuilder notificationBuilder =
                NotificationBuilderFactory.createChromeNotificationBuilder(
                        false /* preferCompat */, ChannelDefinitions.ChannelId.BROWSER);
        notificationBuilder.setContentTitle(shortName)
                .setContentText(message)
                .setLargeIcon(icon)
                .setSmallIcon(R.drawable.ic_chrome)
                .setContentIntent(clickPendingIntent)
                .setWhen(System.currentTimeMillis())
                .setSubText(UrlFormatter.formatUrlForSecurityDisplayOmitScheme(url))
                .setAutoCancel(true);

        NotificationManager notificationManager =
                (NotificationManager) context.getSystemService(NOTIFICATION_SERVICE);
        notificationManager.notify(WEBAPK_INSTALL_NOTIFICATION_TAG_PREFIX + notificationId,
                PLATFORM_ID, notificationBuilder.build());
    }

    /** Cancels any ongoing notification for the WebAPK. */
    @CalledByNative
    private static void cancelNotification(String notificationId) {
        NotificationManager notificationManager =
                (NotificationManager) ContextUtils.getApplicationContext().getSystemService(
                        NOTIFICATION_SERVICE);
        notificationManager.cancel(
                WEBAPK_INSTALL_NOTIFICATION_TAG_PREFIX + notificationId, PLATFORM_ID);
    }
}

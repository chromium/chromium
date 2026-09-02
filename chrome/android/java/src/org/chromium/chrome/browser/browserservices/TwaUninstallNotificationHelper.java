// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

import java.util.Collection;

/**
 * Helper class for creating and displaying a notification when a Trusted Web Activity is
 * uninstalled.
 */
@NullMarked
public class TwaUninstallNotificationHelper {
    private TwaUninstallNotificationHelper() {}

    /**
     * Creates and sends a notification notifying the user that a TWA was uninstalled, providing an
     * opportunity to clear associated browsing data in Chrome.
     */
    public static void showNotification(
            Context context,
            String packageName,
            @Nullable String appName,
            Collection<String> domains,
            Collection<String> origins) {
        if ((domains == null || domains.isEmpty()) && (origins == null || origins.isEmpty())) {
            return;
        }
        NotificationWrapper notification =
                createNotification(context, packageName, appName, domains, origins);
        BaseNotificationManagerProxyFactory.create().notify(notification);
    }

    /** Builds the notification to display when a TWA is uninstalled. */
    public static NotificationWrapper createNotification(
            Context context,
            String packageName,
            @Nullable String appName,
            Collection<String> domains,
            Collection<String> origins) {
        String domainText =
                (domains != null && !domains.isEmpty()) ? domains.iterator().next() : "";
        String effectiveAppName =
                !TextUtils.isEmpty(appName)
                        ? appName
                        : (!TextUtils.isEmpty(domainText) ? domainText : null);

        String title;
        if (!TextUtils.isEmpty(effectiveAppName)) {
            title =
                    context.getString(
                            R.string.twa_post_uninstall_notification_title, effectiveAppName);
        } else {
            title = context.getString(R.string.twa_post_uninstall_notification_title_generic);
        }
        String text = context.getString(R.string.twa_post_uninstall_notification_text);

        // For now, use ClearDataDialogActivity as a placeholder for the new activity.
        // TODO(crbug.com/552664938): Replace with the new dialog activity when available.
        Intent intent =
                ClearDataDialogActivity.createIntent(
                        context, effectiveAppName, domains, origins, /* appUninstalled= */ true);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        PendingIntentProvider pendingIntent =
                PendingIntentProvider.getActivity(
                        context,
                        /* requestCode= */ packageName.hashCode(),
                        intent,
                        PendingIntent.FLAG_UPDATE_CURRENT);

        NotificationMetadata metadata =
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.TRUSTED_WEB_ACTIVITY_SITES,
                        /* notificationTag= */ packageName,
                        /* notificationId= */ packageName.hashCode());

        return NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                        ChromeChannelDefinitions.ChannelId.WEBAPPS, metadata)
                .setSmallIcon(R.drawable.ic_chrome)
                .setContentTitle(title)
                .setContentText(text)
                .setContentIntent(pendingIntent)
                .setAutoCancel(true)
                .buildNotificationWrapper();
    }
}

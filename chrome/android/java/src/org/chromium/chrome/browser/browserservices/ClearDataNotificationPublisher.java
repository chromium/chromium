// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.content.res.Resources;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;

/**
 * The class responsible for showing the "Would you like to clear your data", the "You are clearing
 * your data" and the "Your data has been cleared" notifications that occur when a user uninstalls
 * (or clears data for) a Trusted Web Activity client app.
 *
 * Lifecycle: This class holds no state, create an instance or share as you like.
 * Thread safety: Methods on this class can be called from any thread.
 */
public class ClearDataNotificationPublisher {
    private static final String NOTIFICATION_TAG_CLEAR_DATA = "ClearDataNotification.ClearData";

    /* package */ void showClearDataNotification(Context context, String appName, String domain,
            boolean uninstall) {
        // We base the notification id on the URL so we don't have duplicate Notifications
        // offering to clear the same URL.
        int notificationId = domain.hashCode();

        Resources res = context.getResources();
        String title = res.getString(uninstall ? R.string.you_have_uninstalled_app
                : R.string.you_have_cleared_app, appName);

        Notification notification = NotificationBuilderFactory
                .createChromeNotificationBuilder(true /* preferCompat */,
                        ChannelDefinitions.ChannelId.BROWSER)
                .setContentTitle(title)
                .setContentText(res.getString(R.string.clear_related_data, domain))
                .addAction(R.drawable.btn_star, res.getString(R.string.clear_data_delete),
                        ClearDataService.getClearDataIntent(context, domain, notificationId))
                .addAction(R.drawable.btn_close, res.getString(R.string.close),
                        ClearDataService.getDismissIntent(context, notificationId))
                .setSmallIcon(R.drawable.ic_chrome)
                .build();

        NotificationManager manager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        manager.notify(NOTIFICATION_TAG_CLEAR_DATA, notificationId, notification);
    }

    /* package */ void dismissClearDataNotification(Context context, int notificationId) {
        NotificationManager manager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        manager.cancel(NOTIFICATION_TAG_CLEAR_DATA, notificationId);
    }
}

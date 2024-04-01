// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;

/** Sends notification for information update of Data Sharing service to user. */
public class DataSharingNotificationManager {
    private final Context mContext;
    private final BaseNotificationManagerProxy mNotificationManagerProxy;
    private static final String TAG = "data_sharing";
    // TODO(b/329155961): Use the collaboration_id given by data sharing service.
    private static final int NOTIFICATION_ID = 5000;

    @VisibleForTesting
    DataSharingNotificationManager(Context context, BaseNotificationManagerProxy manager) {
        mContext = context;
        mNotificationManagerProxy = manager;
    }

    public DataSharingNotificationManager(Context context) {
        this(context, BaseNotificationManagerProxyFactory.create(context));
    }

    @VisibleForTesting
    protected NotificationWrapperBuilder getNotificationBuilder() {
        return NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                ChromeChannelDefinitions.ChannelId.BROWSER,
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.DATA_SHARING,
                        TAG,
                        NOTIFICATION_ID));
    }

    /** Show a data sharing notification. */
    public void showNotification(String sharingOrigin) {
        String notificationText =
                mContext.getResources()
                        .getString(
                                R.string.data_sharing_invitation_notification_title, sharingOrigin);

        NotificationWrapper notification =
                getNotificationBuilder()
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setShowWhen(false)
                        .setAutoCancel(true)
                        .setLocalOnly(true)
                        // TODO(b/329155961): Remove temporary strings.
                        .setContentTitle(notificationText)
                        .setContentText(notificationText)
                        .buildNotificationWrapper();

        mNotificationManagerProxy.notify(notification);

        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.DATA_SHARING,
                        notification.getNotification());
    }
}

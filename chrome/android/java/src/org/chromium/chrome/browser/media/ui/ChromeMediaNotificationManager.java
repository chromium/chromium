// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.app.Notification;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.components.browser_ui.media.MediaNotificationManager;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy.StatusBarNotificationProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;

/** Thin wrapper for {@link MediaNotificationManager}. */
@NullMarked
public class ChromeMediaNotificationManager {
    private static boolean sHasCleanedUpStaleNotifications;

    /**
     * Shows a media notification. Passes through to {@link MediaNotificationManager}, utilizing a
     * Chrome specific delegate.
     *
     * @param notificationInfo information to show in the notification
     */
    public static void show(MediaNotificationInfo notificationInfo) {
        MediaNotificationManager.setMultipleMediaNotificationsEnabled(
                ChromeFeatureList.isEnabled(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS));
        MediaNotificationManager.show(
                notificationInfo, ChromeMediaNotificationControllerDelegate::new);
        if (MediaNotificationManager.isMultipleMediaNotificationsEnabled()
                && !sHasCleanedUpStaleNotifications) {
            sHasCleanedUpStaleNotifications = true;
            ResettersForTesting.register(() -> sHasCleanedUpStaleNotifications = false);
            clearStaleNotifications();
        }
    }

    /**
     * Cancels any active media playback notifications in Android's NotificationManager that are not
     * tracked by a live {@link
     * org.chromium.components.browser_ui.media.MediaNotificationController}.
     */
    private static void clearStaleNotifications() {
        BaseNotificationManagerProxy manager = BaseNotificationManagerProxyFactory.create();
        manager.getActiveNotifications(
                notifications -> {
                    for (StatusBarNotificationProxy sbn : notifications) {
                        Notification notification = sbn.getNotification();
                        if (notification != null
                                && ChromeChannelDefinitions.ChannelId.MEDIA_PLAYBACK.equals(
                                        notification.getChannelId())
                                && MediaNotificationManager.getControllerByNotificationId(
                                                sbn.getId())
                                        == null) {
                            manager.cancel(sbn.getId());
                        }
                    }
                });
    }
}

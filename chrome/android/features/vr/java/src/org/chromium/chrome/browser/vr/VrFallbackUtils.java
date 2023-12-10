// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Notification;
import android.content.Context;

import androidx.core.app.NotificationCompat;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;

/** Class providing utils for when the VR module is not installed. */
/* package */ class VrFallbackUtils {
    private static final int PREPARING_VR_NOTIFICATION_TIMEOUT_MS = 5000;
    private static final int PREPARING_VR_NOTIFICATION_DELAY_MS = 2000;

    /** Shows immersive notification informing the user that the VR browser is not ready yet. */
    public static void showFailureNotification(Context context) {
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    NotificationManagerProxy notificationManager =
                            new NotificationManagerProxyImpl(context);
                    var resources = context.getResources();
                    String title = resources.getString(R.string.vr_preparing_vr_notification_title);
                    String message =
                            resources.getString(R.string.vr_preparing_vr_notification_body);
                    Notification notification =
                            NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                            ChromeChannelDefinitions.ChannelId.VR)
                                    .setContentTitle(title)
                                    .setContentText(message)
                                    .setSmallIcon(R.drawable.ic_chrome)
                                    // Need to set category and max priority. Otherwise,
                                    // notification won't show up.
                                    .setCategory(Notification.CATEGORY_MESSAGE)
                                    .setPriorityBeforeO(NotificationCompat.PRIORITY_MAX)
                                    .build();
                    notificationManager.notify(
                            NotificationConstants.NOTIFICATION_ID_PREPARING_VR, notification);

                    // Close notification after a few seconds as it is only really relevant right
                    // after accessing the VR browser failed.
                    PostTask.postDelayedTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                notificationManager.cancel(
                                        NotificationConstants.NOTIFICATION_ID_PREPARING_VR);
                            },
                            PREPARING_VR_NOTIFICATION_TIMEOUT_MS);
                },
                PREPARING_VR_NOTIFICATION_DELAY_MS);
    }
}

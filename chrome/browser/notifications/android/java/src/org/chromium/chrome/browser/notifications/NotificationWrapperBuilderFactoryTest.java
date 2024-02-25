// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.NotificationChannel;
import android.content.Context;
import android.os.Build;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;

/**
 * Tests that NotificationWrapperBuilders created using {@link
 * NotificationWrapperBuilderFactory#createNotificationWrapperBuilder(String)} can be built and the
 * notifications they build don't cause a crash when passed to NotificationManager#notify.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NotificationWrapperBuilderFactoryTest {
    private static final int TEST_NOTIFICATION_ID = 101;

    private NotificationManagerProxy mNotificationManager;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();

        mNotificationManager = new NotificationManagerProxyImpl(context);

        // Don't rely on channels already being registered.
        clearNotificationChannels(mNotificationManager);
    }

    @After
    public void tearDown() {
        // Let's leave things in a clean state.
        mNotificationManager.cancelAll();
    }

    private static void clearNotificationChannels(NotificationManagerProxy notificationManager) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            for (NotificationChannel channel : notificationManager.getNotificationChannels()) {
                if (!channel.getId().equals(NotificationChannel.DEFAULT_CHANNEL_ID)) {
                    notificationManager.deleteNotificationChannel(channel.getId());
                }
            }
        }
    }

    @MediumTest
    @Test
    public void buildCompatNotificationAndNotifyDoesNotCrash() {
        NotificationWrapperBuilder notificationBuilder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                        ChromeChannelDefinitions.ChannelId.BROWSER);

        Notification notification =
                notificationBuilder
                        .setContentTitle("Title")
                        .setSmallIcon(R.drawable.ic_chrome)
                        .build();

        mNotificationManager.notify(TEST_NOTIFICATION_ID, notification);
    }

    @MediumTest
    @Test
    public void buildNotificationWrapper() {
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                        ChromeChannelDefinitions.ChannelId.BROWSER,
                        new NotificationMetadata(
                                NotificationUmaTracker.SystemNotificationType.BROWSER_ACTIONS,
                                null,
                                TEST_NOTIFICATION_ID));

        NotificationWrapper notification =
                builder.setContentTitle("Title")
                        .setSmallIcon(R.drawable.ic_chrome)
                        .buildNotificationWrapper();

        mNotificationManager.notify(notification);
    }
}

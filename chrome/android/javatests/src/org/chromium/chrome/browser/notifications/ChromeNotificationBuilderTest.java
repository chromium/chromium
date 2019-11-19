// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.NotificationChannel;
import android.content.Context;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests that ChromeNotificationBuilders created using
 * {@link NotificationBuilderFactory#createChromeNotificationBuilder(boolean, String)} can be built
 * and the notifications they build don't cause a crash when passed to NotificationManager#notify.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ChromeNotificationBuilderTest {
    private static final int TEST_NOTIFICATION_ID = 101;

    private NotificationManagerProxy mNotificationManager;

    @Before
    public void setUp() {
        Context context = InstrumentationRegistry.getTargetContext();

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
    public void buildNotificationAndNotifyDoesNotCrash() {
        ChromeNotificationBuilder notificationBuilder =
                NotificationBuilderFactory.createChromeNotificationBuilder(
                        false, ChannelDefinitions.ChannelId.BROWSER);

        Notification notification = notificationBuilder.setContentTitle("Title")
                                            .setSmallIcon(R.drawable.ic_chrome)
                                            .build();
        mNotificationManager.notify(TEST_NOTIFICATION_ID, notification);
    }

    @MediumTest
    @Test
    public void buildCompatNotificationAndNotifyDoesNotCrash() {
        ChromeNotificationBuilder notificationBuilder =
                NotificationBuilderFactory.createChromeNotificationBuilder(
                        true, ChannelDefinitions.ChannelId.BROWSER);

        Notification notification = notificationBuilder.setContentTitle("Title")
                                            .setSmallIcon(R.drawable.ic_chrome)
                                            .build();

        mNotificationManager.notify(TEST_NOTIFICATION_ID, notification);
    }

    @MediumTest
    @Test
    public void buildChromeNotification() {
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory.createChromeNotificationBuilder(true,
                        ChannelDefinitions.ChannelId.BROWSER, null,
                        new NotificationMetadata(
                                NotificationUmaTracker.SystemNotificationType.BROWSER_ACTIONS, null,
                                TEST_NOTIFICATION_ID));

        ChromeNotification notification = builder.setContentTitle("Title")
                                                  .setSmallIcon(R.drawable.ic_chrome)
                                                  .buildChromeNotification();

        mNotificationManager.notify(notification);
    }
}

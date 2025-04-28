// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Notification;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationFeatureMap;

import java.util.List;

/** JUnit tests for {@link UnsubscribedNotificationsNotificationManager} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({NotificationFeatureMap.CACHE_NOTIIFICATIONS_ENABLED})
public class UnsubscribedNotificationsNotificationManagerTest {
    private MockNotificationManagerProxy mMockNotificationManager;

    @Before
    public void setUp() {
        mMockNotificationManager = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mMockNotificationManager);
    }

    @Test
    public void testShowNotificationOneSite() {
        UnsubscribedNotificationsNotificationManager.updateNotification(1);
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notifications.size());
        assertEquals(
                "Chrome unsubscribed you from notifications",
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                "Removed notification permissions from one site you haven’t visited recently",
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TEXT));

        Notification notification = notifications.get(0).getNotification();
        assertEquals("Review", notification.actions[0].title);
        assertNotNull(notification.actions[0].actionIntent);
        assertEquals("Got it", notification.actions[1].title);
        assertNotNull(notification.actions[1].actionIntent);
    }

    @Test
    public void testShowNotificationMultipleSites() {
        UnsubscribedNotificationsNotificationManager.updateNotification(2);
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notifications.size());
        assertEquals(
                "Chrome unsubscribed you from notifications",
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                "Removed notification permissions from 2 sites you haven’t visited recently",
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TEXT));
    }

    @Test
    public void testUpdateNotification() {
        assertEquals(0, mMockNotificationManager.getNotifications().size());

        UnsubscribedNotificationsNotificationManager.updateNotification(0);
        assertEquals(0, mMockNotificationManager.getNotifications().size());

        UnsubscribedNotificationsNotificationManager.updateNotification(1);
        {
            List<MockNotificationManagerProxy.NotificationEntry> notifications =
                    mMockNotificationManager.getNotifications();
            assertEquals(1, notifications.size());
            assertEquals(
                    "Removed notification permissions from one site you haven’t visited recently",
                    notifications.get(0).notification.extras.getString(Notification.EXTRA_TEXT));
        }

        UnsubscribedNotificationsNotificationManager.updateNotification(3);
        {
            List<MockNotificationManagerProxy.NotificationEntry> notifications =
                    mMockNotificationManager.getNotifications();
            assertEquals(1, notifications.size());
            assertEquals(
                    "Removed notification permissions from 3 sites you haven’t visited recently",
                    notifications.get(0).notification.extras.getString(Notification.EXTRA_TEXT));
        }

        UnsubscribedNotificationsNotificationManager.updateNotification(0);
        assertEquals(0, mMockNotificationManager.getNotifications().size());
    }
}

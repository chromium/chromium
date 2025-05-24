// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.hamcrest.Matchers.greaterThan;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;

import android.app.Notification;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationFeatureMap;

import java.util.List;

/** JUnit tests for {@link UnsubscribedNotificationsNotificationManager} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({
    NotificationFeatureMap.CACHE_NOTIIFICATIONS_ENABLED,
    ChromeFeatureList.SAFETY_HUB_DISRUPTIVE_NOTIFICATION_REVOCATION + ":shadow_run/false"
})
public class UnsubscribedNotificationsNotificationManagerTest {
    private MockNotificationManagerProxy mMockNotificationManager;

    @Before
    public void setUp() {
        mMockNotificationManager = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mMockNotificationManager);
    }

    @Test
    public void testDisplayNotificationOneSite() {
        UnsubscribedNotificationsNotificationManager.displayNotification(1);
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notifications.size());
        Notification notification = notifications.get(0).notification;
        assertEquals(
                "Chrome unsubscribed you from notifications",
                notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                "Removed notification permissions from one site you haven’t visited recently",
                notification.extras.getString(Notification.EXTRA_TEXT));
        assertEquals("Review", notification.actions[0].title);
        assertNotNull(notification.actions[0].actionIntent);
        assertEquals("Got it", notification.actions[1].title);
        assertNotNull(notification.actions[1].actionIntent);
    }

    @Test
    public void testDisplayNotificationMultipleSites() {
        UnsubscribedNotificationsNotificationManager.displayNotification(2);
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notifications.size());
        Notification notification = notifications.get(0).notification;
        assertEquals(
                "Chrome unsubscribed you from notifications",
                notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                "Removed notification permissions from 2 sites you haven’t visited recently",
                notification.extras.getString(Notification.EXTRA_TEXT));
    }

    @Test
    public void testDisplayNotificationUpdates() {
        assertEquals(0, mMockNotificationManager.getNotifications().size());

        UnsubscribedNotificationsNotificationManager.displayNotification(0);
        assertEquals(0, mMockNotificationManager.getNotifications().size());

        UnsubscribedNotificationsNotificationManager.displayNotification(1);
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notifications.size());
        assertEquals(
                "Removed notification permissions from one site you haven’t visited recently",
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TEXT));

        UnsubscribedNotificationsNotificationManager.displayNotification(3);
        List<MockNotificationManagerProxy.NotificationEntry> notificationsAfter =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notificationsAfter.size());
        assertEquals(
                "Removed notification permissions from 3 sites you haven’t visited recently",
                notificationsAfter.get(0).notification.extras.getString(Notification.EXTRA_TEXT));

        assertThat(
                notificationsAfter.get(0).notification.when,
                greaterThan(notifications.get(0).notification.when));

        UnsubscribedNotificationsNotificationManager.displayNotification(0);
        assertEquals(0, mMockNotificationManager.getNotifications().size());
    }

    @Test
    public void testUpdateNotificationWithNoPreexistentNotification() {
        UnsubscribedNotificationsNotificationManager.updateNotification(1);
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mMockNotificationManager.getNotifications();
        assertEquals(0, notifications.size());
    }

    @Test
    public void testUpdateNotificationNewNumber() {
        UnsubscribedNotificationsNotificationManager.displayNotification(1);
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notifications.size());
        assertEquals(
                "Chrome unsubscribed you from notifications",
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                "Removed notification permissions from one site you haven’t visited recently",
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TEXT));

        UnsubscribedNotificationsNotificationManager.updateNotification(2);
        List<MockNotificationManagerProxy.NotificationEntry> notificationsAfter =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notificationsAfter.size());
        assertEquals(
                "Chrome unsubscribed you from notifications",
                notificationsAfter.get(0).notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                "Removed notification permissions from 2 sites you haven’t visited recently",
                notificationsAfter.get(0).notification.extras.getString(Notification.EXTRA_TEXT));

        assertEquals(
                notificationsAfter.get(0).notification.when,
                notifications.get(0).notification.when);
    }

    @Test
    public void testUpdateNotificationDismisses() {
        UnsubscribedNotificationsNotificationManager.displayNotification(1);
        assertEquals(1, mMockNotificationManager.getNotifications().size());
        UnsubscribedNotificationsNotificationManager.updateNotification(0);
        assertEquals(0, mMockNotificationManager.getNotifications().size());
    }
}

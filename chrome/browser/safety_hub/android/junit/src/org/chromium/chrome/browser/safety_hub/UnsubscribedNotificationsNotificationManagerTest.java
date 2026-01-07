// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Notification;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
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
    @DisableFeatures(ChromeFeatureList.AUTO_REVOKE_SUSPICIOUS_NOTIFICATION)
    public void testDisplayNotificationOneSite() {
        UnsubscribedNotificationsNotificationManager.displayNotification(
                /* numRevokedPermissions= */ 1,
                "example.com",
                /* anySuspiciousRevocations= */ false,
                /* anyDisruptiveRevocations= */ true);
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notifications.size());
        Notification notification = notifications.get(0).notification;
        assertEquals(
                "Unsubscribed from example.com",
                notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                "Chrome stopped a site you haven’t visited recently from sending you notifications",
                notification.extras.getString(Notification.EXTRA_TEXT));
        assertEquals(1, notification.actions.length);
        assertEquals("Review", notification.actions[0].title);
        assertNotNull(notification.actions[0].actionIntent);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTO_REVOKE_SUSPICIOUS_NOTIFICATION)
    public void testDisplayNotificationMultipleSites() {
        UnsubscribedNotificationsNotificationManager.displayNotification(
                /* numRevokedPermissions= */ 2,
                "example.com",
                /* anySuspiciousRevocations= */ false,
                /* anyDisruptiveRevocations= */ true);
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notifications.size());
        Notification notification = notifications.get(0).notification;
        assertEquals(
                "Unsubscribed from 2 sites",
                notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                "Chrome stopped sites you haven’t visited recently from sending you notifications",
                notification.extras.getString(Notification.EXTRA_TEXT));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTO_REVOKE_SUSPICIOUS_NOTIFICATION)
    public void testDisplayNotificationUpdates() {
        assertEquals(0, mMockNotificationManager.getNotifications().size());

        UnsubscribedNotificationsNotificationManager.displayNotification(
                /* numRevokedPermissions= */ 0,
                "example.com",
                /* anySuspiciousRevocations= */ false,
                /* anyDisruptiveRevocations= */ true);
        assertEquals(0, mMockNotificationManager.getNotifications().size());

        UnsubscribedNotificationsNotificationManager.displayNotification(
                /* numRevokedPermissions= */ 1,
                "example.com",
                /* anySuspiciousRevocations= */ false,
                /* anyDisruptiveRevocations= */ true);
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notifications.size());
        assertEquals(
                "Unsubscribed from example.com",
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                "Chrome stopped a site you haven’t visited recently from sending you notifications",
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TEXT));

        UnsubscribedNotificationsNotificationManager.displayNotification(
                /* numRevokedPermissions= */ 3,
                "example.com",
                /* anySuspiciousRevocations= */ false,
                /* anyDisruptiveRevocations= */ true);
        List<MockNotificationManagerProxy.NotificationEntry> notificationsAfter =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notificationsAfter.size());
        assertEquals(
                "Unsubscribed from 3 sites",
                notificationsAfter.get(0).notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                "Chrome stopped sites you haven’t visited recently from sending you notifications",
                notificationsAfter.get(0).notification.extras.getString(Notification.EXTRA_TEXT));

        assertThat(notificationsAfter.get(0).notification.when)
                .isGreaterThan(notifications.get(0).notification.when);

        UnsubscribedNotificationsNotificationManager.displayNotification(
                /* numRevokedPermissions= */ 0,
                "example.com",
                /* anySuspiciousRevocations= */ false,
                /* anyDisruptiveRevocations= */ false);
        assertEquals(0, mMockNotificationManager.getNotifications().size());
    }

    @Test
    public void testUpdateNotificationWithNoPreexistentNotification() {
        UnsubscribedNotificationsNotificationManager.updateNotification(
                /* numRevokedPermissions= */ 1,
                "example.com",
                /* anySuspiciousRevocations= */ true,
                /* anyDisruptiveRevocations= */ false);
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mMockNotificationManager.getNotifications();
        assertEquals(0, notifications.size());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTO_REVOKE_SUSPICIOUS_NOTIFICATION)
    public void testUpdateNotificationNewNumber() {
        UnsubscribedNotificationsNotificationManager.displayNotification(
                /* numRevokedPermissions= */ 1,
                "example.com",
                /* anySuspiciousRevocations= */ false,
                /* anyDisruptiveRevocations= */ true);
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notifications.size());
        assertEquals(
                "Unsubscribed from example.com",
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                "Chrome stopped a site you haven’t visited recently from sending you notifications",
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TEXT));

        UnsubscribedNotificationsNotificationManager.updateNotification(
                /* numRevokedPermissions= */ 2,
                "example.com",
                /* anySuspiciousRevocations= */ false,
                /* anyDisruptiveRevocations= */ true);
        List<MockNotificationManagerProxy.NotificationEntry> notificationsAfter =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notificationsAfter.size());
        assertEquals(
                "Unsubscribed from 2 sites",
                notificationsAfter.get(0).notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                "Chrome stopped sites you haven’t visited recently from sending you notifications",
                notificationsAfter.get(0).notification.extras.getString(Notification.EXTRA_TEXT));

        assertEquals(
                notificationsAfter.get(0).notification.when,
                notifications.get(0).notification.when);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTO_REVOKE_SUSPICIOUS_NOTIFICATION)
    public void testUpdateNotificationDismisses() {
        UnsubscribedNotificationsNotificationManager.displayNotification(
                /* numRevokedPermissions= */ 1,
                "example.com",
                /* anySuspiciousRevocations= */ false,
                /* anyDisruptiveRevocations= */ true);
        assertEquals(1, mMockNotificationManager.getNotifications().size());
        UnsubscribedNotificationsNotificationManager.updateNotification(
                /* numRevokedPermissions= */ 0,
                "example.com",
                /* anySuspiciousRevocations= */ false,
                /* anyDisruptiveRevocations= */ false);
        assertEquals(0, mMockNotificationManager.getNotifications().size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.AUTO_REVOKE_SUSPICIOUS_NOTIFICATION)
    public void testDisplayUpdateNotification_AutoRevokeSuspiciousNotificationEnabled() {
        assertEquals(0, mMockNotificationManager.getNotifications().size());

        UnsubscribedNotificationsNotificationManager.displayNotification(
                /* numRevokedPermissions= */ 1,
                "example.com",
                /* anySuspiciousRevocations= */ true,
                /* anyDisruptiveRevocations= */ false);
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notifications.size());
        assertEquals(
                "Unsubscribed from example.com",
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                "Chrome stopped this site from sending notifications because of spam or deceptive"
                        + " content",
                notifications.get(0).notification.extras.getString(Notification.EXTRA_TEXT));

        UnsubscribedNotificationsNotificationManager.displayNotification(
                /* numRevokedPermissions= */ 3,
                "example.com",
                /* anySuspiciousRevocations= */ true,
                /* anyDisruptiveRevocations= */ false);
        List<MockNotificationManagerProxy.NotificationEntry> notificationsSecondDisplay =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notificationsSecondDisplay.size());
        assertEquals(
                "Unsubscribed from 3 sites",
                notificationsSecondDisplay
                        .get(0)
                        .notification
                        .extras
                        .getString(Notification.EXTRA_TITLE));
        assertEquals(
                "Chrome stopped these sites from sending notifications because of spam or deceptive"
                        + " content",
                notificationsSecondDisplay
                        .get(0)
                        .notification
                        .extras
                        .getString(Notification.EXTRA_TEXT));

        UnsubscribedNotificationsNotificationManager.updateNotification(
                /* numRevokedPermissions= */ 2,
                "example.com",
                /* anySuspiciousRevocations= */ true,
                /* anyDisruptiveRevocations= */ true);
        List<MockNotificationManagerProxy.NotificationEntry> notificationsUpdate =
                mMockNotificationManager.getNotifications();
        assertEquals(1, notificationsUpdate.size());
        assertEquals(
                "Unsubscribed from 2 sites",
                notificationsUpdate.get(0).notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                "Chrome stopped abusive and unused sites from sending notifications",
                notificationsUpdate.get(0).notification.extras.getString(Notification.EXTRA_TEXT));
    }
}

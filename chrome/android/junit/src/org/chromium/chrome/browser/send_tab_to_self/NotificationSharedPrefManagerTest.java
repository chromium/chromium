// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import android.content.SharedPreferences;
import android.support.test.filters.SmallTest;

import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.send_tab_to_self.NotificationSharedPrefManager.ActiveNotification;

/** Tests for NotificationSharedPrefManagerTest */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NotificationSharedPrefManagerTest {
    private @Nullable ActiveNotification deserialize(String serialized) {
        return NotificationSharedPrefManager.deserializeNotification(serialized);
    }

    private void assertNotificationEquals(
            int version, int notificationId, String guid, ActiveNotification actual) {
        Assert.assertNotNull(actual);
        Assert.assertEquals(version, actual.version);
        Assert.assertEquals(notificationId, actual.notificationId);
        Assert.assertEquals(guid, actual.guid);
    }

    @Test
    @SmallTest
    public void testSerialization() {
        ActiveNotification versioned = new ActiveNotification(100, 50, "guid25");
        Assert.assertEquals(
                "100_50_guid25", NotificationSharedPrefManager.serializeNotification(versioned));

        ActiveNotification unVersioned = new ActiveNotification(50, "guid25");
        Assert.assertEquals(
                "1_50_guid25", NotificationSharedPrefManager.serializeNotification(unVersioned));
    }

    @Test
    @SmallTest
    public void testActiveNotificationCreation() {
        ActiveNotification versioned = new ActiveNotification(100, 50, "guid25");
        Assert.assertEquals(100, versioned.version);
        Assert.assertEquals(50, versioned.notificationId);
        Assert.assertEquals("guid25", versioned.guid);

        ActiveNotification unVersioned = new ActiveNotification(5, "guid2");
        Assert.assertEquals(1, unVersioned.version);
        Assert.assertEquals(5, unVersioned.notificationId);
        Assert.assertEquals("guid2", unVersioned.guid);
    }

    @Test
    @SmallTest
    public void testDeserialization() {
        assertNotificationEquals(100, 50, "guid25", deserialize("100_50_guid25"));

        // Too many tokens
        Assert.assertNull(deserialize("100_12_12_12_guid"));

        // Not enough tokens
        Assert.assertNull(deserialize("100_guid"));

        // Malformed version
        Assert.assertNull(deserialize("version_12_guid"));

        // Malformed notificationId
        Assert.assertNull(deserialize("100_notificationId_guid"));

        // Trailing tokens
        assertNotificationEquals(1, 2, "guid", deserialize("1_2_guid_"));
        Assert.assertNull(deserialize("_2_guid"));
        Assert.assertNull(deserialize("1__guid"));
        Assert.assertNull(deserialize("1_2_"));
        Assert.assertNull(deserialize("1__"));
        Assert.assertNull(deserialize("_2_"));
        Assert.assertNull(deserialize("__guid"));
    }

    @Test
    @SmallTest
    public void testNextNotificationId() {
        int id = NotificationSharedPrefManager.getNextNotificationId();
        Assert.assertEquals(0, id);

        id = NotificationSharedPrefManager.getNextNotificationId();
        Assert.assertEquals(1, id);

        id = NotificationSharedPrefManager.getNextNotificationId();
        Assert.assertEquals(2, id);
    }

    @Test
    @SmallTest
    public void testMaxNotificationId() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        prefs.edit()
                .putInt(NotificationSharedPrefManager.PREF_NEXT_NOTIFICATION_ID,
                        Integer.MAX_VALUE - 1)
                .apply();

        // Check that the notificationId is reset.
        int id = NotificationSharedPrefManager.getNextNotificationId();
        Assert.assertEquals(0, id);
        id = NotificationSharedPrefManager.getNextNotificationId();
        Assert.assertEquals(1, id);

        // Check that the notificationId is reset.
        prefs.edit()
                .putInt(NotificationSharedPrefManager.PREF_NEXT_NOTIFICATION_ID, Integer.MAX_VALUE)
                .apply();
        id = NotificationSharedPrefManager.getNextNotificationId();
        Assert.assertEquals(0, id);
    }

    @Test
    @SmallTest
    public void testAddAndFindActiveNotification() {
        ActiveNotification notification = new ActiveNotification(100, 50, "guid25");
        NotificationSharedPrefManager.addActiveNotification(notification);
        Assert.assertEquals(
                notification, NotificationSharedPrefManager.findActiveNotification("guid25"));
    }

    @Test
    @SmallTest
    public void testRemoveActiveNotification() {
        Assert.assertFalse(NotificationSharedPrefManager.removeActiveNotification("guid25"));

        ActiveNotification notification = new ActiveNotification(100, 50, "guid25");
        NotificationSharedPrefManager.addActiveNotification(notification);
        Assert.assertTrue(NotificationSharedPrefManager.removeActiveNotification("guid25"));
    }
}

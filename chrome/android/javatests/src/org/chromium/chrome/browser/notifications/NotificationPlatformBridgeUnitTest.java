// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Notification;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

import java.util.Arrays;

/** Unit tests for NotificationPlatformBridge. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class NotificationPlatformBridgeUnitTest {
    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    /**
     * Verifies that the getOriginFromTag method returns the origin for valid input, and null for
     * invalid input.
     *
     * <p>This is defined in functions in notification_id_generator.cc.
     */
    @Feature({"Browser", "Notifications"})
    @SmallTest
    @Test
    public void testGetOriginFromNotificationTag() {
        // The common case.
        assertEquals(
                "https://example.com",
                NotificationPlatformBridge.getOriginFromNotificationTag(
                        "p#https://example.com#42"));

        // Some invalid input.
        assertNull(
                NotificationPlatformBridge.getOriginFromNotificationTag(
                        "InvalidPrefix#https://example.com#this#tag#contains#the#separator"));
        assertNull(
                NotificationPlatformBridge.getOriginFromNotificationTag("SystemDownloadNotifier"));
        assertNull(NotificationPlatformBridge.getOriginFromNotificationTag(null));
        assertNull(NotificationPlatformBridge.getOriginFromNotificationTag(""));
        assertNull(NotificationPlatformBridge.getOriginFromNotificationTag("#"));
        assertNull(NotificationPlatformBridge.getOriginFromNotificationTag("#######"));
        assertNull(
                NotificationPlatformBridge.getOriginFromNotificationTag(
                        "SystemDownloadNotifier#NotificationPlatformBridge#42"));
        assertNull(
                NotificationPlatformBridge.getOriginFromNotificationTag(
                        "SystemDownloadNotifier#https://example.com#42"));
        assertNull(
                NotificationPlatformBridge.getOriginFromNotificationTag(
                        "NotificationPlatformBridge#SystemDownloadNotifier#42"));
    }

    /**
     * Verifies that the getOriginFromChannelId method returns the origin for a site channel, and
     * null for any other channel or a null channel id.
     */
    @Feature({"Browser", "Notifications"})
    @SmallTest
    @Test
    public void testGetOriginFromChannelId() {
        // Returns the expected origin for a channel id associated with a particular origin.
        assertEquals(
                "https://example.com",
                NotificationPlatformBridge.getOriginFromChannelId(
                        SiteChannelsManager.createChannelId("https://example.com", 62104680000L)));

        // Returns null for a channel id that is not associated with a particular origin.
        assertNull(
                NotificationPlatformBridge.getOriginFromChannelId(
                        ChromeChannelDefinitions.ChannelId.BROWSER));
        assertNull(
                NotificationPlatformBridge.getOriginFromChannelId(
                        ChromeChannelDefinitions.ChannelId.SITES));

        // Returns null if channel id is null.
        assertNull(NotificationPlatformBridge.getOriginFromChannelId(null));
    }

    /** Verifies that the makeDefaults method returns the generated notification defaults. */
    @Feature({"Browser", "Notifications"})
    @SmallTest
    @Test
    public void testMakeDefaults() {
        // 0 should be returned if pattern length is 0, silent is true, and vibration is enabled.
        assertEquals(0, NotificationPlatformBridge.makeDefaults(0, true, true));

        // Notification.DEFAULT_ALL should be returned if pattern length is 0, silent is false and
        // vibration is enabled.
        assertEquals(
                Notification.DEFAULT_ALL, NotificationPlatformBridge.makeDefaults(0, false, true));

        // Vibration should be removed from the defaults if pattern length is greater than 0, silent
        // is false, and vibration is enabled.
        assertEquals(
                Notification.DEFAULT_ALL & ~Notification.DEFAULT_VIBRATE,
                NotificationPlatformBridge.makeDefaults(10, false, true));

        // Vibration should be removed from the defaults if pattern length is greater than 0, silent
        // is false, and vibration is disabled.
        assertEquals(
                Notification.DEFAULT_ALL & ~Notification.DEFAULT_VIBRATE,
                NotificationPlatformBridge.makeDefaults(7, false, false));
    }

    /**
     * Verifies that the makeVibrationPattern method returns vibration pattern used in Android
     * notification.
     */
    @Feature({"Browser", "Notifications"})
    @SmallTest
    @Test
    public void testMakeVibrationPattern() {
        assertTrue(
                Arrays.equals(
                        new long[] {0, 100, 200, 300},
                        NotificationPlatformBridge.makeVibrationPattern(
                                new int[] {100, 200, 300})));
    }
}

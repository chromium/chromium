// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Notification;
import android.os.Build;

import androidx.test.filters.SmallTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.NotificationTestRule;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationFeatureMap;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.EnumSet;
import java.util.List;

/** Tests for the Safety Hub notification about unsubscribed notifications. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({
    NotificationFeatureMap.CACHE_NOTIIFICATIONS_ENABLED,
    ChromeFeatureList.SAFETY_HUB_DISRUPTIVE_NOTIFICATION_REVOCATION + ":shadow_run/false"
})
@Batch(Batch.PER_CLASS)
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
// For some reason these tests are flaky on earlier versions of Android.
@MinAndroidSdkLevel(Build.VERSION_CODES.S)
public final class UnsubscribedNotificationsNotificationTest {
    @Rule public NotificationTestRule mNotificationTestRule = new NotificationTestRule();

    @Test
    @SmallTest
    @Feature({"SafetyHubNotification"})
    public void testReviewNotification() throws Exception {
        UnsubscribedNotificationsNotificationManager.displayNotification(1);
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mNotificationTestRule.getNotificationEntries();
        assertEquals(1, notifications.size());

        Notification notification = notifications.get(0).getNotification();
        assertEquals("Review", notification.actions[0].title);
        Assert.assertNotNull(notification.actions[0].actionIntent);

        SettingsActivity settingsActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        SettingsActivity.class,
                        // In SettingsSingleActivity mode, if there already
                        // exists the settings activity, it will be reused.
                        // In such cases, the activity state is set to
                        // PAUSED, rather than CREATED.
                        // Because there's no guarantee of the existing
                        // settings activity, we wait for either condition.
                        ChromeFeatureList.sSettingsSingleActivity.isEnabled()
                                ? EnumSet.of(Stage.PAUSED, Stage.CREATED)
                                : EnumSet.of(Stage.CREATED),
                        () -> {
                            try {
                                notification.actions[0].actionIntent.send();
                            } catch (Exception e) {
                                fail(e.getMessage());
                            }
                        });
        assertTrue(settingsActivity.getMainFragment() instanceof SafetyHubPermissionsFragment);
    }

    @Test
    @SmallTest
    @Feature({"SafetyHubNotification"})
    public void testReviewNotificationClickingOnContent() throws Exception {
        UnsubscribedNotificationsNotificationManager.displayNotification(1);
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mNotificationTestRule.getNotificationEntries();
        assertEquals(1, notifications.size());

        Notification notification = notifications.get(0).getNotification();
        Assert.assertNotNull(notification.contentIntent);

        SettingsActivity settingsActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        SettingsActivity.class,
                        // In SettingsSingleActivity mode, if there already
                        // exists the settings activity, it will be reused.
                        // In such cases, the activity state is set to
                        // PAUSED, rather than CREATED.
                        // Because there's no guarantee of the existing
                        // settings activity, we wait for either condition.
                        ChromeFeatureList.sSettingsSingleActivity.isEnabled()
                                ? EnumSet.of(Stage.PAUSED, Stage.CREATED)
                                : EnumSet.of(Stage.CREATED),
                        () -> {
                            try {
                                notification.contentIntent.send();
                            } catch (Exception e) {
                                fail(e.getMessage());
                            }
                        });
        assertTrue(settingsActivity.getMainFragment() instanceof SafetyHubPermissionsFragment);
    }
}

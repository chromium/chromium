// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.NotificationManager;
import android.content.Intent;
import android.os.Build;
import android.provider.Settings;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.tasks.tab_management.PriceTrackingUtilities;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;

/**
 * Tests for  {@link PriceDropNotificationManager}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:enable_price_notification/true"})
@Features.DisableFeatures({ChromeFeatureList.START_SURFACE_ANDROID})
public class PriceDropNotificationManagerTest {
    // clang-format on
    private static final String ACTION_APP_NOTIFICATION_SETTINGS =
            "android.settings.APP_NOTIFICATION_SETTINGS";
    private static final String EXTRA_APP_PACKAGE = "app_package";
    private static final String EXTRA_APP_UID = "app_uid";

    private MockNotificationManagerProxy mMockNotificationManager;
    private PriceDropNotificationManager mPriceDropNotificationManager;

    @Before
    public void setUp() {
        mMockNotificationManager = new MockNotificationManagerProxy();
        PriceDropNotificationManager.setNotificationManagerForTesting(mMockNotificationManager);
        mPriceDropNotificationManager = new PriceDropNotificationManager();
    }

    @After
    public void tearDown() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mPriceDropNotificationManager.deleteChannelForTesting();
        }
        PriceDropNotificationManager.setNotificationManagerForTesting(null);
    }

    @Test
    @MediumTest
    public void testCanPostNotification_FeatureDisabled() {
        mMockNotificationManager.setNotificationsEnabled(true);
        PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(false);
        assertFalse(PriceTrackingUtilities.isPriceTrackingEligible());
        assertFalse(mPriceDropNotificationManager.canPostNotification());
    }

    @Test
    @MediumTest
    public void testCanPostNotification_NotificationDisabled() {
        PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(true);
        mMockNotificationManager.setNotificationsEnabled(false);
        assertFalse(mPriceDropNotificationManager.areAppNotificationsEnabled());
        assertFalse(mPriceDropNotificationManager.canPostNotification());
    }

    @Test
    @MediumTest
    public void testCanPostNotificaton() {
        PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(true);
        assertTrue(PriceTrackingUtilities.isPriceTrackingEligible());
        mMockNotificationManager.setNotificationsEnabled(true);
        assertTrue(mPriceDropNotificationManager.areAppNotificationsEnabled());

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            assertTrue(mPriceDropNotificationManager.canPostNotification());
        } else {
            assertNull(mPriceDropNotificationManager.getNotificationChannel());
            assertFalse(mPriceDropNotificationManager.canPostNotification());

            mPriceDropNotificationManager.createNotificationChannel();
            assertNotNull(mPriceDropNotificationManager.getNotificationChannel());
            assertEquals(NotificationManager.IMPORTANCE_LOW,
                    mPriceDropNotificationManager.getNotificationChannel().getImportance());

            assertTrue(mPriceDropNotificationManager.canPostNotification());
        }
    }

    @Test
    @MediumTest
    public void testGetNotificationSettingsIntent_NotificationDisabled() {
        mMockNotificationManager.setNotificationsEnabled(false);
        Intent intent = mPriceDropNotificationManager.getNotificationSettingsIntent();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            assertEquals(ACTION_APP_NOTIFICATION_SETTINGS, intent.getAction());
            assertEquals(ContextUtils.getApplicationContext().getPackageName(),
                    intent.getStringExtra(EXTRA_APP_PACKAGE));
            assertEquals(ContextUtils.getApplicationContext().getApplicationInfo().uid,
                    intent.getIntExtra(EXTRA_APP_UID, 0));
        } else {
            assertEquals(Settings.ACTION_APP_NOTIFICATION_SETTINGS, intent.getAction());
            assertEquals(ContextUtils.getApplicationContext().getPackageName(),
                    intent.getStringExtra(Settings.EXTRA_APP_PACKAGE));
        }
        assertEquals(Intent.FLAG_ACTIVITY_NEW_TASK, intent.getFlags());
    }

    @Test
    @MediumTest
    public void testGetNotificationSettingsIntent_NotificationEnabled() {
        mMockNotificationManager.setNotificationsEnabled(true);
        Intent intent = mPriceDropNotificationManager.getNotificationSettingsIntent();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            assertEquals(ACTION_APP_NOTIFICATION_SETTINGS, intent.getAction());
            assertEquals(ContextUtils.getApplicationContext().getPackageName(),
                    intent.getStringExtra(EXTRA_APP_PACKAGE));
            assertEquals(ContextUtils.getApplicationContext().getApplicationInfo().uid,
                    intent.getIntExtra(EXTRA_APP_UID, 0));
        } else {
            assertEquals(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS, intent.getAction());
            assertEquals(ContextUtils.getApplicationContext().getPackageName(),
                    intent.getStringExtra(Settings.EXTRA_APP_PACKAGE));
            assertEquals(ChromeChannelDefinitions.ChannelId.PRICE_DROP,
                    intent.getStringExtra(Settings.EXTRA_CHANNEL_ID));
        }
        assertEquals(Intent.FLAG_ACTIVITY_NEW_TASK, intent.getFlags());
    }
}

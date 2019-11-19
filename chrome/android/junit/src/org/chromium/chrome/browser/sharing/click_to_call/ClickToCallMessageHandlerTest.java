// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.click_to_call;

import static org.junit.Assert.assertEquals;
import static org.robolectric.Shadows.shadowOf;

import android.app.NotificationManager;
import android.content.Context;
import android.os.Build;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotificationManager;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.DeviceConditions;
import org.chromium.chrome.browser.ShadowDeviceConditions;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.net.ConnectionType;

/**
 * Tests for ClickToCallMessageHandler that check how we handle Click to Call messages. We either
 * display a notification or directly open the dialer.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowDeviceConditions.class})
public class ClickToCallMessageHandlerTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @After
    public void tearDown() {
        FeatureUtilities.setIsClickToCallOpenDialerDirectlyEnabledForTesting(null);
    }

    /**
     * Disabling the flag to directly open the dialer should force us to display a notification.
     */
    @Test
    @Feature({"Browser", "Sharing", "ClickToCall"})
    public void testHandleMessage_disabledFlagShouldDisplayNotification() {
        FeatureUtilities.setIsClickToCallOpenDialerDirectlyEnabledForTesting(false);
        setAtLeastAndroidQ(false);
        setIsScreenOnAndUnlocked(true);

        ClickToCallMessageHandler.handleMessage("18004444444");

        assertEquals(1, getShadowNotificationManager().size());
    }

    /**
     * Android Q+ should always display a notification to open the dialer.
     */
    @Test
    @Feature({"Browser", "Sharing", "ClickToCall"})
    public void testHandleMessage_androidQShouldDisplayNotification() {
        FeatureUtilities.setIsClickToCallOpenDialerDirectlyEnabledForTesting(true);
        setAtLeastAndroidQ(true);
        setIsScreenOnAndUnlocked(true);

        ClickToCallMessageHandler.handleMessage("18004444444");

        assertEquals(1, getShadowNotificationManager().size());
    }

    /**
     * Locked or turned off screens should force us to display a notification.
     */
    @Test
    @Feature({"Browser", "Sharing", "ClickToCall"})
    public void testHandleMessage_lockedScreenShouldDisplayNotification() {
        FeatureUtilities.setIsClickToCallOpenDialerDirectlyEnabledForTesting(true);
        setAtLeastAndroidQ(false);
        setIsScreenOnAndUnlocked(false);

        ClickToCallMessageHandler.handleMessage("18004444444");

        assertEquals(1, getShadowNotificationManager().size());
    }

    /**
     * If all requirements are met, we want to open the dialer directly instead of displaying a
     * notification.
     */
    @Test
    @Feature({"Browser", "Sharing", "ClickToCall"})
    public void testHandleMessage_opensDialerDirectly() {
        FeatureUtilities.setIsClickToCallOpenDialerDirectlyEnabledForTesting(true);
        setAtLeastAndroidQ(false);
        setIsScreenOnAndUnlocked(true);

        ClickToCallMessageHandler.handleMessage("18004444444");

        assertEquals(0, getShadowNotificationManager().size());
    }

    private void setAtLeastAndroidQ(boolean atLeastAndroidQ) {
        // TODO(knollr): update to Build.VERSION_CODES.Q once available.
        int versionCode = atLeastAndroidQ ? 29 : Build.VERSION_CODES.P;
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", versionCode);
    }

    private void setIsScreenOnAndUnlocked(boolean isScreenOnAndUnlocked) {
        DeviceConditions deviceConditions = new DeviceConditions(false /* POWER_CONNECTED */,
                75 /* BATTERY_LEVEL */, ConnectionType.CONNECTION_WIFI, false /* POWER_SAVE */,
                false /* metered */, isScreenOnAndUnlocked);
        ShadowDeviceConditions.setCurrentConditions(deviceConditions);
    }

    private ShadowNotificationManager getShadowNotificationManager() {
        return shadowOf((NotificationManager) RuntimeEnvironment.application.getSystemService(
                Context.NOTIFICATION_SERVICE));
    }
}

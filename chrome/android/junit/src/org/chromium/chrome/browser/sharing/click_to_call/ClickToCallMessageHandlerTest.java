// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.click_to_call;

import static org.junit.Assert.assertEquals;
import static org.robolectric.Shadows.shadowOf;

import android.app.NotificationManager;
import android.content.Context;
import android.os.Build;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotificationManager;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.device.ShadowDeviceConditions;
import org.chromium.net.ConnectionType;

/**
 * Tests for ClickToCallMessageHandler that check how we handle Click to Call messages. We either
 * display a notification or directly open the dialer.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowDeviceConditions.class})
public class ClickToCallMessageHandlerTest {
    /**
     * Android Q+ should always display a notification to open the dialer.
     */
    @Test
    @Feature({"Browser", "Sharing", "ClickToCall"})
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testHandleMessage_androidQShouldDisplayNotification() {
        setIsScreenOnAndUnlocked(true);

        ClickToCallMessageHandler.handleMessage("18004444444");

        assertEquals(1, getShadowNotificationManager().size());
    }

    /**
     * Locked or turned off screens should force us to display a notification.
     */
    @Test
    @Feature({"Browser", "Sharing", "ClickToCall"})
    @Config(sdk = Build.VERSION_CODES.P)
    public void testHandleMessage_lockedScreenShouldDisplayNotification() {
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
    @Config(sdk = Build.VERSION_CODES.P)
    public void testHandleMessage_opensDialerDirectly() {
        setIsScreenOnAndUnlocked(true);

        ClickToCallMessageHandler.handleMessage("18004444444");

        assertEquals(0, getShadowNotificationManager().size());
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

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.click_to_call;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotification;
import org.robolectric.shadows.ShadowNotificationManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.device.ShadowDeviceConditions;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.net.ConnectionType;

/**
 * Tests for ClickToCallMessageHandler that check how we handle Click to Call messages. We either
 * display a notification or directly open the dialer.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowDeviceConditions.class})
public class ClickToCallMessageHandlerTest {
    @Spy private Context mContext = RuntimeEnvironment.application.getApplicationContext();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ContextUtils.initApplicationContextForTests(mContext);
    }

    /** Android Q+ should always display a notification to open the dialer. */
    @Test
    @Feature({"Browser", "Sharing", "ClickToCall"})
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testHandleMessage_androidQShouldDisplayNotification() {
        setIsScreenOnAndUnlocked(true);

        ClickToCallMessageHandler.handleMessage("18004444444");

        assertEquals(1, getShadowNotificationManager().size());
    }

    /** Locked or turned off screens should force us to display a notification. */
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
        verify(mContext, times(1)).startActivity(any(Intent.class));
    }

    @Test
    @Feature({"Browser", "Sharing", "ClickToCall"})
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testHandleMessage_decodesUrlForNotification() {
        setIsScreenOnAndUnlocked(true);

        ClickToCallMessageHandler.handleMessage("%2B44%201234");

        ShadowNotificationManager manager = getShadowNotificationManager();
        assertEquals(1, manager.size());

        Notification notification =
                manager.getNotification(
                        NotificationConstants.GROUP_CLICK_TO_CALL,
                        NotificationConstants.NOTIFICATION_ID_CLICK_TO_CALL);
        ShadowNotification shadowNotification = shadowOf(notification);
        assertEquals("+44 1234", shadowNotification.getContentTitle());
    }

    private void setIsScreenOnAndUnlocked(boolean isScreenOnAndUnlocked) {
        DeviceConditions deviceConditions =
                new DeviceConditions(
                        /* powerConnected= */ false,
                        /* batteryPercentage= */ 75,
                        ConnectionType.CONNECTION_WIFI,
                        /* powerSaveOn= */ false,
                        /* activeNetworkMetered= */ false,
                        isScreenOnAndUnlocked);
        ShadowDeviceConditions.setCurrentConditions(deviceConditions);
    }

    private ShadowNotificationManager getShadowNotificationManager() {
        return shadowOf(
                (NotificationManager)
                        RuntimeEnvironment.application.getSystemService(
                                Context.NOTIFICATION_SERVICE));
    }
}

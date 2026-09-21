// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.scheduler;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;

import android.app.Notification;
import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for DisplayAgent. */
@RunWith(BaseRobolectricTestRunner.class)
public class DisplayAgentUnitTest {
    private Context mContext;
    private BaseNotificationManagerProxy mMockNotificationManager;
    private final List<NotificationWrapper> mNotificationsShown = new ArrayList<>();

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        ContextUtils.initApplicationContextForTests(mContext);
        mMockNotificationManager = mock(BaseNotificationManagerProxy.class);
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mMockNotificationManager);

        mNotificationsShown.clear();
        doAnswer(
                        invocation -> {
                            mNotificationsShown.add(invocation.getArgument(0));
                            return null;
                        })
                .when(mMockNotificationManager)
                .notify(any(NotificationWrapper.class));
    }

    @Test
    public void testShowNotification_SingleButton() {
        DisplayAgent.NotificationData notificationData =
                DisplayAgent.buildNotificationData("Title", "Message");
        DisplayAgent.addButton(
                notificationData, "Helpful", ActionButtonType.HELPFUL, "helpful_button_id");

        DisplayAgent.SystemData systemData =
                DisplayAgent.buildSystemData(SchedulerClientType.CHROME_FINDS, "test_guid_single");

        DisplayAgent.showNotification(notificationData, systemData);

        assertEquals(1, mNotificationsShown.size());
        Notification notification = mNotificationsShown.get(0).getNotification();
        assertEquals(1, notification.actions.length);
        assertEquals("Helpful", notification.actions[0].title);
        assertNotNull(notification.actions[0].actionIntent);
    }

    @Test
    public void testShowNotification_TwoButtonsDistinct() {
        DisplayAgent.NotificationData notificationData =
                DisplayAgent.buildNotificationData("Title", "Message");
        DisplayAgent.addButton(
                notificationData, "Helpful", ActionButtonType.HELPFUL, "helpful_button_id");
        DisplayAgent.addButton(
                notificationData, "Unhelpful", ActionButtonType.UNHELPFUL, "unhelpful_button_id");

        DisplayAgent.SystemData systemData =
                DisplayAgent.buildSystemData(SchedulerClientType.CHROME_FINDS, "test_guid_multi");

        DisplayAgent.showNotification(notificationData, systemData);

        assertEquals(1, mNotificationsShown.size());
        Notification notification = mNotificationsShown.get(0).getNotification();
        assertEquals(2, notification.actions.length);

        // Verify that the PendingIntents are distinct.
        assertNotEquals(
                "Buttons should have distinct PendingIntents",
                notification.actions[0].actionIntent,
                notification.actions[1].actionIntent);
    }
}

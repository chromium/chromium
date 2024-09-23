// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.verify;

import android.app.Notification;
import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotification;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.AsyncNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

/** Unit test for {@link DataSharingNotificationManager} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DataSharingNotificationManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String TAG = "data_sharing";

    @Mock private AsyncNotificationManagerProxy mNotificationManagerProxy;
    private DataSharingNotificationManager mDataSharingNotificationManager;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mDataSharingNotificationManager =
                new DataSharingNotificationManager(mContext, mNotificationManagerProxy);
    }

    @Test
    public void testShowNotification() {
        String originFallback = "Someone";
        ArgumentCaptor<NotificationWrapper> captor =
                ArgumentCaptor.forClass(NotificationWrapper.class);
        mDataSharingNotificationManager.showNotification(originFallback);
        verify(mNotificationManagerProxy).notify(captor.capture());

        NotificationWrapper notificationWrapper = captor.getValue();
        Notification notification = notificationWrapper.getNotification();
        assertEquals(R.drawable.ic_chrome, notification.getSmallIcon().getResId());
        assertEquals(ChromeChannelDefinitions.ChannelId.BROWSER, notification.getChannelId());

        NotificationMetadata notificationMetadata = notificationWrapper.getMetadata();
        assertEquals(
                NotificationUmaTracker.SystemNotificationType.DATA_SHARING,
                notificationMetadata.type);
        assertEquals(TAG, notificationMetadata.tag);

        ShadowNotification shadowNotification =
                Shadows.shadowOf(captor.getValue().getNotification());
        String placeHolderText =
                mContext.getResources()
                        .getString(
                                R.string.data_sharing_invitation_notification_title,
                                originFallback);
        assertFalse(shadowNotification.isWhenShown());
        assertEquals(placeHolderText, shadowNotification.getContentTitle());
        assertEquals(placeHolderText, shadowNotification.getContentText());
    }
}

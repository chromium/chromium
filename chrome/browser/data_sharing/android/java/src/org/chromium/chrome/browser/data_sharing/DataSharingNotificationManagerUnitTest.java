// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.app.Notification;
import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotification;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.AsyncNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

/** Unit tests for {@link DataSharingNotificationManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DataSharingNotificationManagerUnitTest {
    private static final String TAG = "data_sharing";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AsyncNotificationManagerProxy mNotificationManagerProxy;

    @Captor ArgumentCaptor<NotificationWrapper> mNotifyCaptor;

    private DataSharingNotificationManager mDataSharingNotificationManager;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mNotificationManagerProxy);
        mDataSharingNotificationManager = new DataSharingNotificationManager(mContext);
    }

    @Test
    public void testShowInvitationFlowNotification() {
        String originFallback = "Someone";
        mDataSharingNotificationManager.showInvitationFlowNotification(originFallback);
        verify(mNotificationManagerProxy).notify(mNotifyCaptor.capture());

        NotificationWrapper notificationWrapper = mNotifyCaptor.getValue();
        Notification notification = notificationWrapper.getNotification();
        assertEquals(R.drawable.ic_chrome, notification.getSmallIcon().getResId());
        assertEquals(ChromeChannelDefinitions.ChannelId.BROWSER, notification.getChannelId());

        NotificationMetadata notificationMetadata = notificationWrapper.getMetadata();
        assertEquals(
                NotificationUmaTracker.SystemNotificationType.DATA_SHARING,
                notificationMetadata.type);
        assertEquals(TAG, notificationMetadata.tag);

        ShadowNotification shadowNotification = Shadows.shadowOf(notification);
        String placeHolderText =
                mContext.getString(
                        R.string.data_sharing_invitation_notification_title, originFallback);
        assertFalse(shadowNotification.isWhenShown());
        assertEquals(placeHolderText, shadowNotification.getContentTitle());
    }

    @Test
    public void testShowOtherJoinedNotification() {
        String contentTitle = "blah blah expected text";
        mDataSharingNotificationManager.showOtherJoinedNotification(
                contentTitle, /* tabGroupId= */ null);
        verify(mNotificationManagerProxy).notify(mNotifyCaptor.capture());

        NotificationWrapper notificationWrapper = mNotifyCaptor.getValue();
        Notification notification = notificationWrapper.getNotification();
        assertEquals(R.drawable.ic_chrome, notification.getSmallIcon().getResId());
        assertEquals(ChromeChannelDefinitions.ChannelId.BROWSER, notification.getChannelId());

        NotificationMetadata notificationMetadata = notificationWrapper.getMetadata();
        assertEquals(
                NotificationUmaTracker.SystemNotificationType.DATA_SHARING,
                notificationMetadata.type);
        assertEquals(TAG, notificationMetadata.tag);

        ShadowNotification shadowNotification = Shadows.shadowOf(notification);
        assertTrue(shadowNotification.isWhenShown());
        assertEquals(contentTitle, shadowNotification.getContentTitle());
    }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.components.tab_group_sync.SyncedGroupTestHelper.SYNC_GROUP_ID1;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotification;
import org.robolectric.shadows.ShadowPendingIntent;

import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.data_sharing.DataSharingIntentUtils.Action;
import org.chromium.chrome.browser.notifications.NotificationIntentInterceptor;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationFeatureMap;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.url.GURL;

/** Unit tests for {@link DataSharingNotificationManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({NotificationFeatureMap.CACHE_NOTIIFICATIONS_ENABLED})
@Config(manifest = Config.NONE)
public class DataSharingNotificationManagerUnitTest {
    private static final String TAG = "data_sharing";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BaseNotificationManagerProxy mNotificationManagerProxy;

    @Captor ArgumentCaptor<NotificationWrapper> mNotifyCaptor;
    @Captor ArgumentCaptor<Intent> mIntentCaptor;

    private DataSharingNotificationManager mDataSharingNotificationManager;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = Mockito.spy(ApplicationProvider.getApplicationContext());
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
        assertEquals(ChromeChannelDefinitions.ChannelId.COLLABORATION, notification.getChannelId());

        NotificationMetadata notificationMetadata = notificationWrapper.getMetadata();
        assertEquals(
                NotificationUmaTracker.SystemNotificationType.DATA_SHARING,
                notificationMetadata.type);
        assertEquals(TAG, notificationMetadata.tag);

        ShadowNotification shadowNotification = shadowOf(notification);
        String placeHolderText =
                mContext.getString(
                        R.string.data_sharing_invitation_notification_title, originFallback);
        assertFalse(shadowNotification.isWhenShown());
        assertEquals(placeHolderText, shadowNotification.getContentTitle());

        Intent intent = triggerPendingIntent(notification);
        @Action
        int action = intent.getIntExtra(DataSharingIntentUtils.ACTION_EXTRA, Action.UNKNOWN);
        assertEquals(Action.INVITATION_FLOW, action);
        String actualUrl = intent.getStringExtra(DataSharingIntentUtils.INVITATION_URL_EXTRA);
        assertEquals(GURL.emptyGURL().getSpec(), actualUrl);
    }

    @Test
    public void testShowOtherJoinedNotification() {
        String contentTitle = "blah blah expected text";
        mDataSharingNotificationManager.showOtherJoinedNotification(
                contentTitle, SYNC_GROUP_ID1, /* notificationId= */ 1234);
        verify(mNotificationManagerProxy).notify(mNotifyCaptor.capture());

        NotificationWrapper notificationWrapper = mNotifyCaptor.getValue();
        assertEquals(1234, notificationWrapper.getMetadata().id);
        Notification notification = notificationWrapper.getNotification();
        assertEquals(R.drawable.ic_chrome, notification.getSmallIcon().getResId());
        assertEquals(ChannelId.COLLABORATION, notification.getChannelId());

        NotificationMetadata notificationMetadata = notificationWrapper.getMetadata();
        assertEquals(
                NotificationUmaTracker.SystemNotificationType.DATA_SHARING,
                notificationMetadata.type);
        assertEquals(TAG, notificationMetadata.tag);

        ShadowNotification shadowNotification = shadowOf(notification);
        assertTrue(shadowNotification.isWhenShown());
        assertEquals(contentTitle, shadowNotification.getContentTitle());

        Intent intent = triggerPendingIntent(notification);
        @Action
        int action = intent.getIntExtra(DataSharingIntentUtils.ACTION_EXTRA, Action.UNKNOWN);
        assertEquals(Action.MANAGE_TAB_GROUP, action);
        String syncId =
                IntentUtils.safeGetStringExtra(
                        intent, DataSharingIntentUtils.TAB_GROUP_SYNC_ID_EXTRA);
        assertEquals(SYNC_GROUP_ID1, syncId);
    }

    private Intent triggerPendingIntent(Notification notification) {
        Intent originalIntent = readOriginalIntentWithShadows(notification);
        DataSharingNotificationManager.Receiver receiver =
                new DataSharingNotificationManager.Receiver();
        receiver.onReceive(mContext, originalIntent);
        verify(mContext).startActivity(mIntentCaptor.capture(), any());
        return mIntentCaptor.getValue();
    }

    private Intent readOriginalIntentWithShadows(Notification notification) {
        ShadowPendingIntent trampolineShadow = shadowOf(notification.contentIntent);
        Intent trampolineIntent = trampolineShadow.getSavedIntent();
        PendingIntent pendingIntent =
                NotificationIntentInterceptor.getPendingIntentForTesting(trampolineIntent);
        ShadowPendingIntent pendingIntentShadow = shadowOf(pendingIntent);
        return pendingIntentShadow.getSavedIntent();
    }
}

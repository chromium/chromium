// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

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
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotification;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationType;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.AsyncNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

/** Tests for {@link PwdAccessLossNotificationCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PwdAccessLossNotificationCoordinatorTest {
    private PwdAccessLossNotificationCoordinator mCoordinator;
    private Context mContext;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Mock private AsyncNotificationManagerProxy mNotificationManagerProxy;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);

        mContext = RuntimeEnvironment.getApplication();
        mCoordinator =
                new PwdAccessLossNotificationCoordinator(mContext, mNotificationManagerProxy);
    }

    @Test
    public void testShowNotification() {
        ArgumentCaptor<NotificationWrapper> captor =
                ArgumentCaptor.forClass(NotificationWrapper.class);
        mCoordinator.showNotification(PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM);
        verify(mNotificationManagerProxy).notify(captor.capture());

        NotificationWrapper notificationWrapper = captor.getValue();
        Notification notification = notificationWrapper.getNotification();
        assertEquals(R.drawable.ic_chrome, notification.getSmallIcon().getResId());
        assertEquals(ChromeChannelDefinitions.ChannelId.BROWSER, notification.getChannelId());

        NotificationMetadata notificationMetadata = notificationWrapper.getMetadata();
        assertEquals(SystemNotificationType.UPM_ACCESS_LOSS_WARNING, notificationMetadata.type);
        assertEquals(PwdAccessLossNotificationCoordinator.TAG, notificationMetadata.tag);
    }

    @Test
    public void testShowNotificationForNoGmsCore() {
        ArgumentCaptor<NotificationWrapper> captor =
                ArgumentCaptor.forClass(NotificationWrapper.class);
        mCoordinator.showNotification(PasswordAccessLossWarningType.NO_GMS_CORE);
        verify(mNotificationManagerProxy).notify(captor.capture());

        ShadowNotification shadowNotification =
                Shadows.shadowOf(captor.getValue().getNotification());
        assertFalse(shadowNotification.isWhenShown());
        assertEquals(
                mContext.getString(R.string.pwd_access_loss_warning_no_gms_core_title),
                shadowNotification.getContentTitle());
        assertEquals(
                mContext.getString(R.string.pwd_access_loss_notification_no_gms_core_text),
                shadowNotification.getContentText());
    }

    @Test
    public void testShowNotificationForNoUpm() {
        ArgumentCaptor<NotificationWrapper> captor =
                ArgumentCaptor.forClass(NotificationWrapper.class);
        mCoordinator.showNotification(PasswordAccessLossWarningType.NO_UPM);
        verify(mNotificationManagerProxy).notify(captor.capture());

        ShadowNotification shadowNotification =
                Shadows.shadowOf(captor.getValue().getNotification());
        assertFalse(shadowNotification.isWhenShown());
        assertEquals(
                mContext.getString(R.string.pwd_access_loss_warning_update_gms_core_title),
                shadowNotification.getContentTitle());
        assertEquals(
                mContext.getString(R.string.pwd_access_loss_notification_update_gms_core_text),
                shadowNotification.getContentText());
    }

    @Test
    public void testShowNotificationForOnlyAccountUpm() {
        ArgumentCaptor<NotificationWrapper> captor =
                ArgumentCaptor.forClass(NotificationWrapper.class);
        mCoordinator.showNotification(PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM);
        verify(mNotificationManagerProxy).notify(captor.capture());

        ShadowNotification shadowNotification =
                Shadows.shadowOf(captor.getValue().getNotification());
        assertFalse(shadowNotification.isWhenShown());
        assertEquals(
                mContext.getString(R.string.pwd_access_loss_warning_update_gms_core_title),
                shadowNotification.getContentTitle());
        assertEquals(
                mContext.getString(R.string.pwd_access_loss_notification_update_gms_core_text),
                shadowNotification.getContentText());
    }

    @Test
    public void testShowNotificationWhenMigrationFailed() {
        ArgumentCaptor<NotificationWrapper> captor =
                ArgumentCaptor.forClass(NotificationWrapper.class);
        mCoordinator.showNotification(PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED);
        verify(mNotificationManagerProxy).notify(captor.capture());

        ShadowNotification shadowNotification =
                Shadows.shadowOf(captor.getValue().getNotification());
        assertFalse(shadowNotification.isWhenShown());
        assertEquals(
                mContext.getString(R.string.pwd_access_loss_warning_manual_migration_title),
                shadowNotification.getContentTitle());
        assertEquals(
                mContext.getString(R.string.pwd_access_loss_warning_manual_migration_text),
                shadowNotification.getContentText());
    }
}

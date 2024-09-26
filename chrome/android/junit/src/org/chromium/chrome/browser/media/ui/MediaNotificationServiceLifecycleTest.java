// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doCallRealMethod;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.robolectric.Shadows.shadowOf;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotificationManager;

import org.chromium.base.task.AsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.components.browser_ui.media.MediaNotificationController;
import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.components.browser_ui.media.MediaNotificationManager;
import org.chromium.services.media_session.MediaMetadata;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * JUnit tests for checking {@link MediaNotificationController} handles the listener service life
 * cycle correctly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {MediaNotificationTestShadowResources.class})
public class MediaNotificationServiceLifecycleTest extends MediaNotificationTestBase {
    @Test
    public void testServiceLifeCycle() {
        ensureMediaNotificationInfo();

        Intent intent = getController().mDelegate.createServiceIntent();

        assertNull(mService);
        mMockContext.startService(intent);
        verify(getController()).onServiceStarted(mService);
        assertNotNull(mService);
        verify(mService).onStartCommand(intent, 0, 0);

        mService.getImpl().stopListenerService();
        assertNull(mService);
    }

    @Test
    public void testProcessIntentFailureStopsService() {
        MediaNotificationController controller = getController();
        setUpService();

        MockListenerService service = mService;
        MockListenerServiceImpl impl = service.getImpl();
        doReturn(false).when(impl).processIntent(any(Intent.class));
        mMockContext.startService(new Intent());
        verify(service.getImpl()).stopListenerService();
        assertNull(getController());
        verify(controller).onServiceDestroyed();
    }

    @Test
    public void testProcessNullIntent() {
        setUpService();
        assertFalse(mService.getImpl().processIntent(null));
    }

    @Test
    public void testProcessIntentWhenManagerIsNull() {
        setUpService();
        MediaNotificationManager.setControllerForTesting(getNotificationId(), null);
        assertFalse(mService.getImpl().processIntent(new Intent()));
    }

    @Test
    public void testProcessIntentWhenNotificationInfoIsNull() {
        setUpService();
        getController().mMediaNotificationInfo = null;
        assertFalse(mService.getImpl().processIntent(new Intent()));
    }

    @Test
    public void testShowNotificationIsNoOpWhenInfoMatches() {
        doCallRealMethod().when(getController()).onServiceStarted(any(MockListenerService.class));
        setUpServiceAndClearInvocations();

        MediaNotificationInfo newInfo = mMediaNotificationInfoBuilder.build();
        getController().showNotification(newInfo);

        verify(getController()).showNotification(newInfo);
        verifyNoMoreInteractions(getController());
        verify(mMockForegroundServiceUtils, never()).startForegroundService(any(Intent.class));
        verify(mMockContext, never()).startService(any(Intent.class));
        verify(mMockUmaTracker, never()).onNotificationShown(anyInt(), any(Notification.class));
    }

    @Test
    public void testShowNotificationIsNoOpWhenInfoIsPausedAndFromAnotherTab() {
        doCallRealMethod().when(getController()).onServiceStarted(any(MockListenerService.class));
        mMediaNotificationInfoBuilder.setInstanceId(0);
        setUpServiceAndClearInvocations();

        mMediaNotificationInfoBuilder.setInstanceId(1).setPaused(true);
        MediaNotificationInfo newInfo = mMediaNotificationInfoBuilder.build();
        getController().showNotification(newInfo);

        verify(getController()).showNotification(newInfo);
        verifyNoMoreInteractions(getController());
        verify(mMockForegroundServiceUtils, never()).startForegroundService(any(Intent.class));
        verify(mMockContext, never()).startService(any(Intent.class));
        verify(mMockUmaTracker, never()).onNotificationShown(anyInt(), any(Notification.class));
    }

    @Test
    public void testShowNotificationWhenServiceNotCreated() {
        MediaNotificationInfo newInfo = mMediaNotificationInfoBuilder.build();
        getController().showNotification(newInfo);

        verify(getController(), times(1)).updateMediaSession();
        verify(getController(), times(1)).updateNotificationBuilder();
        verify(mMockContext, never()).startService(any(Intent.class));
        verify(mMockForegroundServiceUtils, times(1)).startForegroundService(any(Intent.class));
        verify(getController(), never()).updateNotification(anyBoolean(), eq(false));
    }

    @Test
    public void testShowNotificationWhenServiceAlreadyCreated() {
        doCallRealMethod().when(getController()).onServiceStarted(any(MockListenerService.class));
        setUpServiceAndClearInvocations();

        mMediaNotificationInfoBuilder.setPaused(true);
        MediaNotificationInfo newInfo = mMediaNotificationInfoBuilder.build();
        getController().showNotification(newInfo);

        verify(getController()).showNotification(newInfo);
        verify(mMockForegroundServiceUtils, never()).startForegroundService(any(Intent.class));
        verify(mMockContext, never()).startService(any(Intent.class));
        verify(getController()).updateNotification(anyBoolean(), eq(false));
        verify(mMockUmaTracker, never()).onNotificationShown(anyInt(), any(Notification.class));
    }

    @Test
    public void testShowNotificationBeforeServiceCreatedUpdatesNotificationInfoAndLogsUma() {
        doCallRealMethod().when(getController()).onServiceStarted(any(MockListenerService.class));

        // The initial call to |showNotification()| should update the notification info and request
        // to start the service.
        MediaNotificationInfo oldInfo = mMediaNotificationInfoBuilder.build();
        getController().showNotification(oldInfo);

        InOrder order = inOrder(getController(), mMockForegroundServiceUtils);

        assertEquals(oldInfo, getController().mMediaNotificationInfo);
        order.verify(getController(), times(1)).updateMediaSession();
        order.verify(getController(), times(1)).updateNotificationBuilder();
        order.verify(mMockForegroundServiceUtils, times(1))
                .startForegroundService(any(Intent.class));
        order.verify(getController(), never()).updateNotification(anyBoolean(), eq(false));

        // The second call to |showNotification()| should only update the notification info.
        mMediaNotificationInfoBuilder.setMetadata(new MediaMetadata("new title", "", ""));
        MediaNotificationInfo newInfo = mMediaNotificationInfoBuilder.build();
        getController().showNotification(newInfo);

        assertEquals(newInfo, getController().mMediaNotificationInfo);
        order.verify(getController(), times(1)).updateMediaSession();
        order.verify(getController(), times(1)).updateNotificationBuilder();
        order.verify(mMockForegroundServiceUtils, times(1))
                .startForegroundService(any(Intent.class));
        order.verify(getController(), never()).updateNotification(anyBoolean(), eq(false));

        verify(getController(), never()).onServiceStarted(any(MockListenerService.class));

        // Simulate the service has started.
        mMockContext.startService(getController().mDelegate.createServiceIntent());
        order.verify(getController(), times(1)).onServiceStarted(mService);
        order.verify(getController(), times(1)).updateNotification(anyBoolean(), eq(true));
        verify(mMockUmaTracker)
                .onNotificationShown(
                        eq(NotificationUmaTracker.SystemNotificationType.MEDIA),
                        any(Notification.class));
    }

    @Test
    public void updateNotificationIsNoOpBeforeServiceCreated() {
        getController().mMediaNotificationInfo = mMediaNotificationInfoBuilder.build();
        getController().updateNotification(false, false);

        waitForAsync();
        verify(getController()).updateNotification(anyBoolean(), eq(false));
        verify(getController(), never()).updateMediaSession();
        verify(getController(), never()).updateNotificationBuilder();
    }

    @Test
    public void updateNotificationIsNoOpWhenNotificiationInfoIsNull() {
        setUpService();
        getController().mService = mService;
        getController().mMediaNotificationInfo = null;
        getController().updateNotification(false, false);

        waitForAsync();
        verify(getController()).updateNotification(anyBoolean(), eq(false));
        verify(getController(), never()).updateMediaSession();
        verify(getController(), never()).updateNotificationBuilder();

        verify(mMockForegroundServiceUtils, never()).stopForeground(eq(mService), anyInt());
        verify(mMockForegroundServiceUtils, never())
                .startForeground(eq(mService), anyInt(), any(Notification.class), anyInt());
    }

    @Test
    public void updateNotificationSetsServiceBackgroundWhenPausedAndSupportsSwipeAway() {
        mMediaNotificationInfoBuilder.setPaused(true);
        setUpService();
        getController().mService = mService;
        getController().mMediaNotificationInfo = mMediaNotificationInfoBuilder.build();
        getController().updateNotification(false, false);

        waitForAsync();
        verify(mMockForegroundServiceUtils)
                .stopForeground(eq(mService), eq(Service.STOP_FOREGROUND_DETACH));
        assertEquals(1, getShadowNotificationManager().getAllNotifications().size());
    }

    @Test
    public void updateNotificationSetsServiceBackgroundWhenPausedButDoesntSupportSwipeAway() {
        mMediaNotificationInfoBuilder.setPaused(true).setActions(0);
        setUpService();
        getController().mService = mService;
        getController().mMediaNotificationInfo = mMediaNotificationInfoBuilder.build();
        getController().updateNotification(false, false);

        waitForAsync();
        verify(mMockForegroundServiceUtils)
                .startForeground(
                        eq(mService),
                        eq(getNotificationId()),
                        any(Notification.class),
                        eq(ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK));
    }

    @Test
    public void updateNotificationSetsServiceForegroundWhenPlaying() {
        mMediaNotificationInfoBuilder.setPaused(false);
        setUpService();
        getController().mService = mService;
        getController().mMediaNotificationInfo = mMediaNotificationInfoBuilder.build();
        getController().updateNotification(false, false);

        waitForAsync();
        verify(mMockForegroundServiceUtils)
                .startForeground(
                        eq(mService),
                        eq(getNotificationId()),
                        any(Notification.class),
                        eq(ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK));
    }

    private ShadowNotificationManager getShadowNotificationManager() {
        NotificationManager notificationManager =
                (NotificationManager) mMockContext.getSystemService(Context.NOTIFICATION_SERVICE);
        return shadowOf(notificationManager);
    }

    private static class AsyncTaskRunnableHelper extends CallbackHelper implements Runnable {
        @Override
        public void run() {
            notifyCalled();
        }
    }

    private void waitForAsync() {
        try {
            AsyncTaskRunnableHelper runnableHelper = new AsyncTaskRunnableHelper();
            AsyncTask.SERIAL_EXECUTOR.execute(runnableHelper);
            runnableHelper.waitForCallback(0, 1, 5L, TimeUnit.SECONDS);
        } catch (TimeoutException ex) {
        }
    }
}

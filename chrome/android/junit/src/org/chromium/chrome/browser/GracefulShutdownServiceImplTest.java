// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.Service;
import android.content.Context;
import android.content.Intent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.SplitCompatService;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.components.browser_ui.notifications.ForegroundServiceUtils;

/** Unit tests for {@link GracefulShutdownServiceImpl} and {@link GracefulShutdownService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GracefulShutdownServiceImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private Context mContext;
    @Mock private ForegroundServiceUtils mMockForegroundServiceUtils;
    @Mock private SplitCompatService mMockService;

    private GracefulShutdownServiceImpl mService;

    @Before
    public void setUp() {
        ForegroundServiceUtils.setInstanceForTesting(mMockForegroundServiceUtils);
        mService = new GracefulShutdownServiceImpl();
        lenient().when(mMockService.getString(anyInt())).thenReturn("test_title");
        lenient()
                .when(mMockService.getResources())
                .thenReturn(ContextUtils.getApplicationContext().getResources());
        mService.setServiceForTesting(mMockService);
    }

    @Test
    public void testOnCreate() {
        mService.onCreate();

        verify(mMockForegroundServiceUtils, never())
                .startForeground(any(), anyInt(), any(), anyInt());
    }

    @Test
    public void testOnStartCommand_success() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.GracefulShutdownStatus",
                        GracefulShutdownService.Status.STARTED);

        Intent intent = new Intent();
        int result = mService.onStartCommand(intent, /* flags= */ 0, /* startId= */ 1);
        assertEquals(Service.START_NOT_STICKY, result);

        verify(mMockForegroundServiceUtils)
                .startForeground(
                        eq(mMockService),
                        eq(NotificationConstants.NOTIFICATION_ID_GRACEFUL_SHUTDOWN),
                        any(),
                        anyInt());
        watcher.assertExpected();
    }

    @Test
    public void testOnStartCommand_startForegroundThrows() {
        doThrow(new RuntimeException("Foreground service start failed"))
                .when(mMockForegroundServiceUtils)
                .startForeground(any(), anyInt(), any(), anyInt());
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.GracefulShutdownStatus",
                        GracefulShutdownService.Status.START_FOREGROUND_FAILED);

        Intent intent = new Intent();
        int result = mService.onStartCommand(intent, /* flags= */ 0, /* startId= */ 1);
        assertEquals(Service.START_NOT_STICKY, result);

        verify(mMockService).stopSelf();
        watcher.assertExpected();
    }

    @Test
    public void testOnStartCommand_schedulesStop() {
        Intent intent = new Intent();
        int result = mService.onStartCommand(intent, /* flags= */ 0, /* startId= */ 1);
        assertEquals(Service.START_NOT_STICKY, result);

        // Before delayed timeout is reached, stopForeground and stopSelf should not be called.
        verify(mMockForegroundServiceUtils, never()).stopForeground(any(), anyInt());
        verify(mMockService, never()).stopSelf();

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.GracefulShutdownStatus",
                        GracefulShutdownService.Status.TIMED_OUT);

        // Run delayed tasks (2000ms shutdown timeout).
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mMockForegroundServiceUtils)
                .stopForeground(eq(mMockService), eq(Service.STOP_FOREGROUND_REMOVE));
        verify(mMockService).stopSelf();
        watcher.assertExpected();
    }

    @Test
    public void testOnTimeout() {
        Intent intent = new Intent();
        mService.onStartCommand(intent, /* flags= */ 0, /* startId= */ 1);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.GracefulShutdownStatus",
                        GracefulShutdownService.Status.OS_TIMED_OUT);

        mService.onTimeout(/* startId= */ 1);

        verify(mMockForegroundServiceUtils)
                .stopForeground(eq(mMockService), eq(Service.STOP_FOREGROUND_REMOVE));
        verify(mMockService).stopSelf();
        watcher.assertExpected();

        // Ensure the delayed runnable was cancelled so it won't trigger another stopForeground.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockForegroundServiceUtils)
                .stopForeground(eq(mMockService), eq(Service.STOP_FOREGROUND_REMOVE));
        verify(mMockService).stopSelf();
    }

    @Test
    public void testOnTimeout_withFgsType() {
        Intent intent = new Intent();
        mService.onStartCommand(intent, /* flags= */ 0, /* startId= */ 1);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.GracefulShutdownStatus",
                        GracefulShutdownService.Status.OS_TIMED_OUT);

        mService.onTimeout(/* startId= */ 1, /* fgsType= */ 0);

        verify(mMockForegroundServiceUtils)
                .stopForeground(eq(mMockService), eq(Service.STOP_FOREGROUND_REMOVE));
        verify(mMockService).stopSelf();
        watcher.assertExpected();
    }

    @Test
    public void testOnDestroy_cancelsStopRunnable() {
        Intent intent = new Intent();
        mService.onStartCommand(intent, /* flags= */ 0, /* startId= */ 1);

        mService.onDestroy();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockForegroundServiceUtils, never()).stopForeground(any(), anyInt());
        verify(mMockService, never()).stopSelf();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_ANDROID_GRACEFUL_SHUTDOWN)
    public void testMaybeStartGracefulShutdown_launchFailed() {
        when(mActivity.isFinishing()).thenReturn(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);

        when(mContext.startForegroundService(any()))
                .thenThrow(new IllegalStateException("ForegroundServiceStartNotAllowedException"));

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.GracefulShutdownStatus",
                        GracefulShutdownService.Status.LAUNCH_FAILED);

        GracefulShutdownService.maybeStartGracefulShutdown(mContext);

        watcher.assertExpected();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_ANDROID_GRACEFUL_SHUTDOWN)
    public void testMaybeStartGracefulShutdown_securityException() {
        when(mActivity.isFinishing()).thenReturn(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);

        when(mContext.startForegroundService(any()))
                .thenThrow(new SecurityException("Foreground service not allowed"));

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.GracefulShutdownStatus",
                        GracefulShutdownService.Status.LAUNCH_FAILED);

        GracefulShutdownService.maybeStartGracefulShutdown(mContext);

        watcher.assertExpected();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_ANDROID_GRACEFUL_SHUTDOWN)
    public void testMaybeStartGracefulShutdown_success() {
        when(mActivity.isFinishing()).thenReturn(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.GracefulShutdownStatus",
                        GracefulShutdownService.Status.LAUNCH_ATTEMPTED);

        GracefulShutdownService.maybeStartGracefulShutdown(mContext);

        verify(mContext).startForegroundService(any(Intent.class));
        watcher.assertExpected();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
    }
}

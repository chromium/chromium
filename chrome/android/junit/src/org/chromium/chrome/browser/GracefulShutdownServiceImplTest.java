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
import android.os.Process;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.SplitCompatService;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.components.browser_ui.notifications.ForegroundServiceUtils;

/** Unit tests for {@link GracefulShutdownServiceImpl} and {@link GracefulShutdownService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GracefulShutdownServiceImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private ForegroundServiceUtils mMockForegroundServiceUtils;
    @Mock private SplitCompatService mMockService;
    @Captor private ArgumentCaptor<Intent> mIntentCaptor;

    private Activity mActivity;
    private GracefulShutdownServiceImpl mService;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        ForegroundServiceUtils.setInstanceForTesting(mMockForegroundServiceUtils);
        mService = new GracefulShutdownServiceImpl();
        lenient().when(mMockService.getString(anyInt())).thenReturn("test_title");
        lenient()
                .when(mMockService.getResources())
                .thenReturn(ContextUtils.getApplicationContext().getResources());
        lenient().when(mMockService.stopSelfResult(anyInt())).thenReturn(true);
        mService.setServiceForTesting(mMockService);
    }

    private static Intent createStartIntent() {
        Intent intent = new Intent();
        intent.putExtra(GracefulShutdownServiceImpl.EXTRA_CALLING_PID, Process.myPid());
        return intent;
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

        Intent intent = createStartIntent();
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

        Intent intent = createStartIntent();
        int result = mService.onStartCommand(intent, /* flags= */ 0, /* startId= */ 1);
        assertEquals(Service.START_NOT_STICKY, result);

        verify(mMockService).stopSelfResult(/* startId= */ 1);
        watcher.assertExpected();
    }

    @Test
    public void testOnStartCommand_schedulesStop() {
        Intent intent = createStartIntent();
        int result = mService.onStartCommand(intent, /* flags= */ 0, /* startId= */ 1);
        assertEquals(Service.START_NOT_STICKY, result);

        // Before delayed timeout is reached, stopForeground and stopSelfResult should not be
        // called.
        verify(mMockForegroundServiceUtils, never()).stopForeground(any(), anyInt());
        verify(mMockService, never()).stopSelfResult(anyInt());

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.GracefulShutdownStatus",
                        GracefulShutdownService.Status.TIMED_OUT);

        // Run delayed tasks (2000ms shutdown timeout).
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mMockForegroundServiceUtils)
                .stopForeground(eq(mMockService), eq(Service.STOP_FOREGROUND_REMOVE));
        verify(mMockService).stopSelfResult(/* startId= */ 1);
        watcher.assertExpected();
    }

    @Test
    public void testOnStartCommand_staleCallingPidIgnored() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Tab.Android.GracefulShutdownStatus")
                        .build();

        // 1. Test null intent
        int resultNull = mService.onStartCommand(null, /* flags= */ 0, /* startId= */ 1);
        assertEquals(Service.START_NOT_STICKY, resultNull);
        verify(mMockService).stopSelfResult(/* startId= */ 1);

        // 2. Test mismatched calling PID
        Intent staleIntent = new Intent();
        staleIntent.putExtra(GracefulShutdownServiceImpl.EXTRA_CALLING_PID, Process.myPid() + 100);
        int resultMismatched =
                mService.onStartCommand(staleIntent, /* flags= */ 0, /* startId= */ 2);
        assertEquals(Service.START_NOT_STICKY, resultMismatched);
        verify(mMockService).stopSelfResult(/* startId= */ 2);

        verify(mMockForegroundServiceUtils, never())
                .startForeground(any(), anyInt(), any(), anyInt());
        watcher.assertExpected();
    }

    @Test
    public void testOnStartCommand_stopRunnableUsesLatestDeliveredStartId() {
        // 1. Undelivered newer start queued in AMS: stopSelfResult(1) returns false.
        lenient().when(mMockService.stopSelfResult(1)).thenReturn(false);
        lenient().when(mMockService.stopSelfResult(2)).thenReturn(true);

        assertEquals(
                Service.START_NOT_STICKY,
                mService.onStartCommand(createStartIntent(), /* flags= */ 0, /* startId= */ 1));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockService).stopSelfResult(/* startId= */ 1);
        verify(mMockForegroundServiceUtils, never()).stopForeground(any(), anyInt());

        // 2. Delivered overlapping startId=2 that fails startForeground while startId=1's timer is
        // active: timer must preserve startId=1's window, then stop with mLatestStartId=2.
        assertEquals(
                Service.START_NOT_STICKY,
                mService.onStartCommand(createStartIntent(), /* flags= */ 0, /* startId= */ 1));
        doThrow(new RuntimeException("BFSL denied"))
                .when(mMockForegroundServiceUtils)
                .startForeground(any(), anyInt(), any(), anyInt());
        assertEquals(
                Service.START_NOT_STICKY,
                mService.onStartCommand(createStartIntent(), /* flags= */ 0, /* startId= */ 2));
        verify(mMockService, never()).stopSelfResult(/* startId= */ 2);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockService).stopSelfResult(/* startId= */ 2);
        verify(mMockForegroundServiceUtils)
                .stopForeground(eq(mMockService), eq(Service.STOP_FOREGROUND_REMOVE));
    }

    @Test
    public void testOnTimeout() {
        Intent intent = createStartIntent();
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
        Intent intent = createStartIntent();
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
        Intent intent = createStartIntent();
        mService.onStartCommand(intent, /* flags= */ 0, /* startId= */ 1);

        mService.onDestroy();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockForegroundServiceUtils, never()).stopForeground(any(), anyInt());
        verify(mMockService, never()).stopSelf();
        verify(mMockService, never()).stopSelfResult(anyInt());
    }

    @Test
    public void testMaybeStartGracefulShutdown_launchFailed() {
        mActivity.finish();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);

        when(mContext.startService(any()))
                .thenThrow(new IllegalStateException("BackgroundServiceStartNotAllowedException"));

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.GracefulShutdownStatus",
                        GracefulShutdownService.Status.LAUNCH_FAILED);

        GracefulShutdownServiceImpl.maybeStartGracefulShutdown(mContext);

        watcher.assertExpected();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
    }

    @Test
    public void testMaybeStartGracefulShutdown_success() {
        mActivity.finish();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.Android.GracefulShutdownStatus",
                        GracefulShutdownService.Status.LAUNCH_ATTEMPTED);

        GracefulShutdownServiceImpl.maybeStartGracefulShutdown(mContext);

        verify(mContext).startService(mIntentCaptor.capture());
        assertEquals(
                Process.myPid(),
                mIntentCaptor
                        .getValue()
                        .getIntExtra(GracefulShutdownServiceImpl.EXTRA_CALLING_PID, -1));
        watcher.assertExpected();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
    }
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.app.Notification;
import android.content.Intent;

import androidx.core.app.ServiceCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.actor.foreground_service.ActorForegroundServiceController;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;

/** Unit tests for {@link ActorForegroundServiceImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.GLIC_BACKGROUND_TRIGGERING)
public class ActorForegroundServiceImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ChromeBrowserInitializer mChromeBrowserInitializer;
    @Mock private ActorForegroundServiceControllerImpl mMockController;
    @Mock private Profile mMockProfile;

    private ActorForegroundServiceImpl mServiceImpl;
    private Notification mNotification;

    @Before
    public void setUp() {
        ChromeBrowserInitializer.setForTesting(mChromeBrowserInitializer);
        ActorForegroundServiceController.setInstanceForTesting(mMockController);
        ProfileManager.setLastUsedProfileForTesting(mMockProfile);

        mServiceImpl = new ActorForegroundServiceImpl();
        mServiceImpl.setServiceForTesting(new ActorForegroundService());
        mNotification = new Notification();
    }

    @Test
    public void testLifecycleHistograms() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Actor.ForegroundService.Lifecycle",
                                ActorForegroundServiceUmaHelper.ForegroundLifecycle.STARTED)
                        .expectIntRecord(
                                "Actor.ForegroundService.Lifecycle",
                                ActorForegroundServiceUmaHelper.ForegroundLifecycle.UPDATED)
                        .expectIntRecord(
                                "Actor.ForegroundService.Lifecycle",
                                ActorForegroundServiceUmaHelper.ForegroundLifecycle.STOPPED)
                        .build();

        mServiceImpl.startOrUpdateForegroundService(1, mNotification, -1, false);
        mServiceImpl.startOrUpdateForegroundService(1, mNotification, 1, false);
        mServiceImpl.stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_REMOVE);

        watcher.assertExpected();
    }

    @Test
    public void testStartOrUpdateForegroundService_SwitchingNotificationId_DoesNotStopForeground() {
        ActorForegroundServiceImpl spyService = spy(mServiceImpl);
        Notification notification2 = new Notification();

        spyService.startOrUpdateForegroundService(1, mNotification, -1, false);
        spyService.startOrUpdateForegroundService(2, notification2, 1, true);

        verify(spyService).startForegroundInternal(1, mNotification);
        verify(spyService).startForegroundInternal(2, notification2);
        verify(spyService, never()).stopForegroundInternal(anyInt());
    }

    @Test
    public void testStopReasonStopped() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Actor.ForegroundService.StopReason",
                                ActorForegroundServiceUmaHelper.StopReason.STOPPED)
                        .build();

        mServiceImpl.onStartCommand(new Intent(), 0, 1);
        mServiceImpl.stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_REMOVE);
        mServiceImpl.onDestroy();

        watcher.assertExpected();
    }

    @Test
    public void testStopReasonDestroyed() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Actor.ForegroundService.StopReason",
                                ActorForegroundServiceUmaHelper.StopReason.DESTROYED)
                        .build();

        mServiceImpl.onStartCommand(new Intent(), 0, 1);
        mServiceImpl.onDestroy();

        watcher.assertExpected();
    }

    @Test
    public void testStopReasonTaskRemoved() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Actor.ForegroundService.StopReason",
                                ActorForegroundServiceUmaHelper.StopReason.TASK_REMOVED)
                        .build();

        mServiceImpl.onStartCommand(new Intent(), 0, 1);
        mServiceImpl.onTaskRemoved(new Intent());
        mServiceImpl.onDestroy();

        watcher.assertExpected();
    }

    @Test
    public void testStopReasonLowMemory() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Actor.ForegroundService.StopReason",
                                ActorForegroundServiceUmaHelper.StopReason.LOW_MEMORY)
                        .build();

        mServiceImpl.onStartCommand(new Intent(), 0, 1);
        mServiceImpl.onLowMemory();
        mServiceImpl.onDestroy();

        watcher.assertExpected();
    }

    @Test
    public void testDurationHistogram() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Actor.ForegroundService.Duration")
                        .build();

        mServiceImpl.onStartCommand(new Intent(), 0, 1);
        mServiceImpl.onDestroy();

        watcher.assertExpected();
    }

    @Test
    public void testNoMetricsIfNeverStarted() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Actor.ForegroundService.Duration")
                        .expectNoRecords("Actor.ForegroundService.StopReason")
                        .build();

        mServiceImpl.onCreate();
        mServiceImpl.onTaskRemoved(new Intent());
        mServiceImpl.onLowMemory();
        mServiceImpl.onDestroy();

        watcher.assertExpected();
    }
}

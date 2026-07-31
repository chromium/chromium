// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.robolectric.annotation.Config;

import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;

/** Unit tests for {@link ActorForegroundServiceImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures(ChromeFeatureList.GLIC_BACKGROUND_TRIGGERING)
public class ActorForegroundServiceImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ChromeBrowserInitializer mChromeBrowserInitializer;
    @Mock private ActorForegroundServiceControllerImpl mMockController;
    @Mock private ActorBackgroundActuationManager mMockBackgroundManager;
    @Mock private Profile mMockProfile;

    private ActorForegroundServiceImpl mServiceImpl;
    private Notification mNotification;

    @Before
    public void setUp() {
        ChromeBrowserInitializer.setForTesting(mChromeBrowserInitializer);
        when(mMockController.getBackgroundActuationManager()).thenReturn(mMockBackgroundManager);
        ActorForegroundServiceController.setInstanceForTesting(mMockController);
        ProfileManager.setLastUsedProfileForTesting(mMockProfile);
        IntentUtils.setForceIsTrustedIntentForTesting(false);

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

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_TRIGGERING)
    public void testOnStartCommandPromotesToForegroundWhenGlicTriggeringEnabled() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Actor.ForegroundService.Lifecycle",
                                ActorForegroundServiceUmaHelper.ForegroundLifecycle.STARTED)
                        .build();

        mServiceImpl.onStartCommand(new Intent(), /*flags=*/0, /*startId=*/1);

        watcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_TRIGGERING)
    public void testOnStartCommand_ActivityNotVisible_StartsBackgroundActuation() {
        IntentUtils.setForceIsTrustedIntentForTesting(true);
        when(mMockController.isTabbedActivityVisible()).thenReturn(false);

        Intent intent = new Intent();
        intent.setAction("org.chromium.chrome.browser.actor.START_ACTOR_FOREGROUND_SERVICE");
        intent.putExtra(
                "org.chromium.chrome.browser.actor.EXTRA_GLIC_TRIGGER_MESSAGE_ID",
                "test-message-id");

        mServiceImpl.onStartCommand(intent, /* flags= */ 0, /* startId= */ 1);

        verify(mMockBackgroundManager).startBackgroundActuation(mMockProfile, "test-message-id");
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_TRIGGERING)
    public void testOnStartCommand_ActivityVisible_DoesNotStartBackgroundActuation() {
        IntentUtils.setForceIsTrustedIntentForTesting(true);
        when(mMockController.isTabbedActivityVisible()).thenReturn(true);

        Intent intent = new Intent();
        intent.setAction("org.chromium.chrome.browser.actor.START_ACTOR_FOREGROUND_SERVICE");
        intent.putExtra(
                "org.chromium.chrome.browser.actor.EXTRA_GLIC_TRIGGER_MESSAGE_ID",
                "test-message-id");

        mServiceImpl.onStartCommand(intent, /* flags= */ 0, /* startId= */ 1);

        verify(mMockBackgroundManager, never())
                .startBackgroundActuation(mMockProfile, "test-message-id");
    }
}

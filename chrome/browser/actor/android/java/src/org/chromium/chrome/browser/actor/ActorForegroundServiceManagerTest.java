// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Notification;
import android.os.Looper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;

import java.util.Collections;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link ActorForegroundServiceManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActorForegroundServiceManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActorKeyedService mKeyedService;
    @Mock private ActorForegroundServiceController mServiceController;
    @Mock private ActorNotificationService mNotificationService;
    @Mock private ActorTask mTask;
    @Mock private Profile mProfile;
    @Mock private Notification mNotification;

    private ActorForegroundServiceManager mManager;

    private static class TestActorForegroundServiceManager extends ActorForegroundServiceManager {
        @Override
        protected boolean canStartForeground() {
            return true;
        }
    }

    @Before
    public void setUp() {
        ActorForegroundServiceController.setInstanceForTesting(mServiceController);
        ActorKeyedServiceFactory.setForTesting(mKeyedService);
        ProfileManager.resetForTesting();
        ActorForegroundServiceManager.setWaitTimeForTesting(0);

        mManager = new TestActorForegroundServiceManager();
        mManager.setNotificationServiceForTesting(mNotificationService);
        mManager.setKeyedServiceForTesting(mKeyedService);
        ActorForegroundServiceManager.setInstanceForTesting(mManager);

        when(mTask.getId()).thenReturn(1);
        when(mTask.isCompleted()).thenReturn(false);
        when(mTask.isUnderActorControl()).thenReturn(true);
        when(mKeyedService.getTask(1)).thenReturn(mTask);
        when(mKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(mTask));
        when(mKeyedService.getCurrentActiveTask()).thenReturn(mTask);
        when(mNotificationService.getForegroundNotification(any())).thenReturn(mNotification);

        when(mProfile.isOffTheRecord()).thenReturn(false);

        // Setup default startAndBind behavior to simulate connection
        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            if (runnable != null) {
                                // Simulate service connecting and triggering callback
                                when(mServiceController.isConnected()).thenReturn(true);
                                runnable.run();
                            }
                            return null;
                        })
                .when(mServiceController)
                .startAndBindService(any());

        // Clear setup state and tasks
        shadowOf(Looper.getMainLooper()).idleFor(1, TimeUnit.SECONDS);
        mManager.resetForTesting();
    }

    @Test
    public void testOnTaskStateChanged_StartsService() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);

        assertTrue("Service should be bound.", mManager.isServiceBoundForTesting());
        verify(mServiceController).startAndBindService(any());
    }

    @Test
    public void testOnTaskStateChanged_UpdatesForeground() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        assertTrue("Service should be bound.", mManager.isServiceBoundForTesting());

        // Process the post in startAndBindService
        ShadowLooper.idleMainLooper();

        verify(mServiceController)
                .startOrUpdateForegroundService(
                        anyInt(), eq(mNotification), anyInt(), anyBoolean());
    }

    @Test
    public void testTaskCompleted_StopsServiceWithDelay() throws Exception {
        mManager.setKeyedServiceForTesting(mKeyedService);
        CallbackHelper stopCallback = new CallbackHelper();
        mManager.setStopCallbackForTesting(stopCallback::notifyCalled);

        // Start service
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        assertTrue("Service should be bound.", mManager.isServiceBoundForTesting());
        ShadowLooper.idleMainLooper();

        // Complete task
        when(mTask.isCompleted()).thenReturn(true);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);
        mManager.onTaskStateChanged(1, ActorTaskState.FINISHED);

        // Service shouldn't stop immediately
        assertTrue(
                "Service should still be bound before delay.", mManager.isServiceBoundForTesting());

        // Run delayed task
        ShadowLooper.idleMainLooper();
        stopCallback.waitForOnly();

        assertFalse("Service should be unbound after delay.", mManager.isServiceBoundForTesting());
        verify(mServiceController).stopActorForegroundService(anyInt());
        verify(mServiceController).unbindService();
    }

    @Test
    public void testTaskCompleted_StopsServiceAfterOneHourDelay() throws Exception {
        mManager.setKeyedServiceForTesting(mKeyedService);
        CallbackHelper stopCallback = new CallbackHelper();
        mManager.setStopCallbackForTesting(stopCallback::notifyCalled);

        // Override wait time to 1 hour
        ActorForegroundServiceManager.setWaitTimeForTesting(60 * 60 * 1000);

        // Start service
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        assertTrue("Service should be bound.", mManager.isServiceBoundForTesting());
        ShadowLooper.idleMainLooper();

        // Complete task
        when(mTask.isCompleted()).thenReturn(true);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);
        mManager.onTaskStateChanged(1, ActorTaskState.FINISHED);

        // Service shouldn't stop immediately
        assertTrue(
                "Service should still be bound before delay.", mManager.isServiceBoundForTesting());

        // Idle for 30 minutes, should still be bound
        shadowOf(Looper.getMainLooper()).idleFor(30, TimeUnit.MINUTES);
        assertTrue(
                "Service should still be bound after 30 minutes.",
                mManager.isServiceBoundForTesting());

        // Idle for another 31 minutes, should stop
        shadowOf(Looper.getMainLooper()).idleFor(31, TimeUnit.MINUTES);
        stopCallback.waitForOnly();

        assertFalse(
                "Service should be unbound after 1 hour delay.",
                mManager.isServiceBoundForTesting());
    }

    @Test
    public void testTaskCompleted_NewTaskStartedWithinHour_ServiceStaysAlive() throws Exception {
        mManager.setKeyedServiceForTesting(mKeyedService);

        // Override wait time to 1 hour
        ActorForegroundServiceManager.setWaitTimeForTesting(60 * 60 * 1000);

        // Start task 1
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        assertTrue("Service should be bound.", mManager.isServiceBoundForTesting());
        ShadowLooper.idleMainLooper();

        // Complete task 1
        when(mTask.isCompleted()).thenReturn(true);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);
        mManager.onTaskStateChanged(1, ActorTaskState.FINISHED);

        // Idle for 30 minutes
        shadowOf(Looper.getMainLooper()).idleFor(30, TimeUnit.MINUTES);
        assertTrue(
                "Service should still be bound after 30 minutes.",
                mManager.isServiceBoundForTesting());

        // Start task 2
        ActorTask task2 = org.mockito.Mockito.mock(ActorTask.class);
        when(task2.getId()).thenReturn(2);
        when(task2.isCompleted()).thenReturn(false);
        when(mKeyedService.getTask(2)).thenReturn(task2);
        when(mKeyedService.getActiveTasksCount()).thenReturn(1);
        mManager.onTaskStateChanged(2, ActorTaskState.ACTING);

        // Idle for another hour, service should still be bound because of task 2
        shadowOf(Looper.getMainLooper()).idleFor(1, TimeUnit.HOURS);
        assertTrue(
                "Service should stay alive because task 2 started within the hour.",
                mManager.isServiceBoundForTesting());
    }

    @Test
    public void testTaskPaused_KeepsServiceAliveForMVP() throws Exception {
        mManager.setKeyedServiceForTesting(mKeyedService);

        // Start service
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        assertTrue("Service should be bound.", mManager.isServiceBoundForTesting());
        ShadowLooper.idleMainLooper();

        // Pause task
        when(mTask.isCompleted()).thenReturn(false);
        when(mTask.isUnderActorControl()).thenReturn(false);
        mManager.onTaskStateChanged(1, ActorTaskState.PAUSED_BY_USER);

        ShadowLooper.idleMainLooper();

        // Verify service is still bounded.
        assertTrue(
                "Service should remain bound for paused tasks in MVP.",
                mManager.isServiceBoundForTesting());

        // Verify that startOrUpdateForegroundService was called to update the notification
        verify(mServiceController, atLeastOnce())
                .startOrUpdateForegroundService(
                        anyInt(), eq(mNotification), anyInt(), anyBoolean());
    }

    @Test
    public void testProfileManagement() {
        org.mockito.Mockito.reset(mKeyedService);
        ActorForegroundServiceManager.initialize();
        ProfileManager.onProfileAdded(mProfile);
        ShadowLooper.idleMainLooper();

        // Manager should start observing the keyed service
        verify(mKeyedService, atLeastOnce()).addObserver(mManager);

        ProfileManager.onProfileDestroyed(mProfile);
        verify(mKeyedService, atLeastOnce()).removeObserver(mManager);
    }
}

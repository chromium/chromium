// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Notification;
import android.os.Looper;

import androidx.core.app.ServiceCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;

import java.util.Collections;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link ActorForegroundServiceManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ANDROID_ACTOR_TASK_TIMEOUT)
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
        when(mNotificationService.getForegroundNotification(any(), anyBoolean(), anyBoolean()))
                .thenReturn(mNotification);

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
        verify(mServiceController).onTaskCompleted(1);
        verify(mServiceController).stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_DETACH);
        verify(mServiceController).unbindService();
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
        reset(mKeyedService);
        ActorForegroundServiceManager.initialize();
        ProfileManager.onProfileAdded(mProfile);
        ShadowLooper.idleMainLooper();

        // Manager should start observing the keyed service
        verify(mKeyedService, atLeastOnce()).addObserver(mManager);

        ProfileManager.onProfileDestroyed(mProfile);
        verify(mKeyedService, atLeastOnce()).removeObserver(mManager);
    }

    @Test
    public void testRedundantUpdatesSkipped() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        // First call should trigger startOrUpdateForegroundService
        verify(mServiceController)
                .startOrUpdateForegroundService(eq(1), eq(mNotification), anyInt(), anyBoolean());

        clearInvocations(mServiceController);
        when(mServiceController.isConnected()).thenReturn(true);

        // Second call with same notification should NOT trigger startOrUpdateForegroundService
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();
        verify(mServiceController, never())
                .startOrUpdateForegroundService(anyInt(), any(), anyInt(), anyBoolean());
    }

    @Test
    public void testOnTaskStateChanged_PinsCorrectNotification() {
        mManager.setKeyedServiceForTesting(mKeyedService);

        // Initial state
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();
        verify(mServiceController)
                .startOrUpdateForegroundService(eq(1), eq(mNotification), anyInt(), anyBoolean());

        // Update to another task
        reset(mServiceController);
        when(mServiceController.isConnected()).thenReturn(true);
        ActorTask task2 = mock(ActorTask.class);
        when(task2.getId()).thenReturn(2);
        when(mKeyedService.getTask(2)).thenReturn(task2);
        when(mKeyedService.getCurrentActiveTask()).thenReturn(task2);
        Notification notification2 = mock(Notification.class);
        when(mNotificationService.getForegroundNotification(eq(task2), anyBoolean(), anyBoolean()))
                .thenReturn(notification2);

        mManager.onTaskStateChanged(2, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        // Should update to task 2's notification
        verify(mServiceController)
                .startOrUpdateForegroundService(eq(2), eq(notification2), eq(1), eq(true));
    }

    @Test
    public void testOnPiPToTabTransition_UpdateForegroundServiceSkipped() {
        mManager.setKeyedServiceForTesting(mKeyedService);

        when(mServiceController.isActivityVisibleForTabs(any())).thenReturn(false);
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        verify(mServiceController)
                .startOrUpdateForegroundService(eq(1), eq(mNotification), anyInt(), anyBoolean());

        clearInvocations(mServiceController);
        when(mServiceController.isConnected()).thenReturn(true);

        when(mServiceController.isActivityVisibleForTabs(any())).thenReturn(true);
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        // Should skip updating foreground service when notification state doesn't change.
        verify(mServiceController, never())
                .startOrUpdateForegroundService(anyInt(), any(), anyInt(), anyBoolean());
    }

    @Test
    public void testOnTaskStateChanged_ShowsWarningNotification() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        ActorTaskTimeoutManager timeoutManager = mock(ActorTaskTimeoutManager.class);
        mManager.setTimeoutManagerForTesting(timeoutManager);

        int taskId = 1;
        when(timeoutManager.isWarningMode(taskId)).thenReturn(true);

        mManager.onTaskStateChanged(taskId, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        // Verify that the notification service receives isWarning = true
        verify(mNotificationService)
                .updateNotificationForTask(
                        eq(taskId), eq(ActorTaskState.ACTING), anyBoolean(), eq(true));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_ACTOR_TASK_TIMEOUT)
    public void testRunningTaskTimeout_TriggersWarningPause_ThenStopsOnTerminalTimeout()
            throws Exception {
        int taskId = 1;
        CallbackHelper stopCallback = new CallbackHelper();
        mManager.setStopCallbackForTesting(stopCallback::notifyCalled);

        doAnswer(
                        invocation -> {
                            mManager.onTaskStateChanged(taskId, ActorTaskState.PAUSED_BY_ACTOR);
                            return null;
                        })
                .when(mTask)
                .pause();

        doAnswer(
                        invocation -> {
                            mManager.onTaskStateChanged(taskId, ActorTaskState.FAILED);
                            return null;
                        })
                .when(mKeyedService)
                .stopTask(eq(taskId), anyInt());

        mManager.onTaskStateChanged(taskId, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        // Advance time by running timeout budget.
        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getRunningTimeoutMs(), TimeUnit.MILLISECONDS);

        verify(mTask).pause();
        verify(mNotificationService, atLeastOnce())
                .updateNotificationForTask(
                        eq(taskId), eq(ActorTaskState.PAUSED_BY_ACTOR), anyBoolean(), eq(true));

        when(mTask.isCompleted()).thenReturn(true);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);

        // Advance time by terminal warning grace period without user resumption.
        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getWarningTimeoutMs(), TimeUnit.MILLISECONDS);

        verify(mKeyedService).stopTask(taskId, StoppedReason.TIMEOUT);

        ShadowLooper.idleMainLooper();
        assertEquals(1, stopCallback.getCallCount());
        assertFalse(
                "Service should be unbound after terminal timeout.",
                mManager.isServiceBoundForTesting());
        verify(mServiceController).stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_DETACH);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_ACTOR_TASK_TIMEOUT)
    public void testPausedTaskTimeout_TriggersWarning_ResumedByUser_WarningDisappears() {
        int taskId = 1;

        mManager.onTaskStateChanged(taskId, ActorTaskState.PAUSED_BY_USER);
        ShadowLooper.idleMainLooper();

        // Advance time by paused timeout budget.
        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getPausedTimeoutMs(), TimeUnit.MILLISECONDS);

        verify(mNotificationService, atLeastOnce())
                .updateNotificationForTask(
                        eq(taskId), eq(ActorTaskState.PAUSED_BY_USER), anyBoolean(), eq(true));

        clearInvocations(mNotificationService);
        clearInvocations(mKeyedService);

        // Simulate user resuming task from Chrome UI.
        mManager.onTaskStateChanged(taskId, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        verify(mNotificationService, atLeastOnce())
                .updateNotificationForTask(
                        eq(taskId), eq(ActorTaskState.ACTING), anyBoolean(), eq(false));

        // Advance time by terminal warning grace period.
        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getWarningTimeoutMs(), TimeUnit.MILLISECONDS);

        verify(mKeyedService, never()).stopTask(eq(taskId), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ACTOR_STEP_PROGRESS_NOTIFICATION)
    public void testOnTaskStepProgressUpdated_RefreshesNotification() {
        int taskId = 1;
        mManager.setKeyedServiceForTesting(mKeyedService);
        mManager.onTaskStateChanged(taskId, ActorTaskState.ACTING);

        clearInvocations(mNotificationService);

        mManager.onTaskStepProgressUpdated(taskId, "Navigating to site");
        verify(mNotificationService).updateNotificationForStepProgress(taskId);
    }

    @Test
    public void testResendWorkingNotifications_ActiveTask_CallsNotificationService() {
        int taskId = 1;
        mManager.setKeyedServiceForTesting(mKeyedService);

        mManager.onTaskStateChanged(taskId, ActorTaskState.ACTING);
        clearInvocations(mNotificationService);

        mManager.resendWorkingNotifications();

        verify(mNotificationService).resendWorkingNotificationLoudly(taskId);
    }

    @Test
    public void testResendWorkingNotifications_TerminalTask_DoesNotCallNotificationService() {
        int taskId = 1;
        mManager.setKeyedServiceForTesting(mKeyedService);

        mManager.onTaskStateChanged(taskId, ActorTaskState.ACTING);
        mManager.onTaskStateChanged(taskId, ActorTaskState.FINISHED);
        clearInvocations(mNotificationService);

        mManager.resendWorkingNotifications();

        verify(mNotificationService, never()).resendWorkingNotificationLoudly(anyInt());
    }
}

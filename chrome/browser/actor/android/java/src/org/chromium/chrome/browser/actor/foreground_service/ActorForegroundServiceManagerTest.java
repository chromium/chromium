// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.foreground_service;

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
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Notification;
import android.content.Intent;
import android.os.Build;
import android.os.Looper;

import androidx.core.app.ServiceCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorMetrics;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.actor.ActorTaskTimeoutManager;
import org.chromium.chrome.browser.actor.ActorTaskTimeoutParameters;
import org.chromium.chrome.browser.actor.StoppedReason;
import org.chromium.chrome.browser.actor.notifications.ActorBroadcastReceiver;
import org.chromium.chrome.browser.actor.notifications.ActorNotificationFactory;
import org.chromium.chrome.browser.actor.notifications.ActorNotificationService;
import org.chromium.chrome.browser.actor.ui.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

import java.util.Collections;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link ActorForegroundServiceManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ANDROID_ACTOR_TASK_TIMEOUT)
public class ActorForegroundServiceManagerTest {
    private static final int OPT_IN_NOTIFICATION_ID =
            ActorNotificationFactory.OPT_IN_NOTIFICATION_ID;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActorKeyedService mKeyedService;
    @Mock private ActorForegroundServiceController mServiceController;
    @Mock private ActorNotificationService mNotificationService;
    @Mock private ActorTask mTask;
    @Mock private Profile mProfile;
    @Mock private Notification mNotification;
    @Mock private Runnable mCancelCallback;

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
        when(mServiceController.createTrustedPendingActorTaskNewTabIntent())
                .thenReturn(new Intent());

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

        verify(mServiceController).startOrUpdateForegroundService(anyInt(), eq(mNotification));
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
        verify(mServiceController).stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_REMOVE);
        verify(mNotificationService).repostNotification(1, /* isWarning= */ false);
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
                .startOrUpdateForegroundService(anyInt(), eq(mNotification));
    }

    @Test
    public void testProfileManagement() {
        reset(mKeyedService);
        ActorForegroundServiceManager.initialize();
        ProfileManager.onProfileAdded(mProfile);
        ShadowLooper.idleMainLooper();

        // ActorMetrics must be registered before the manager so it observes completion events
        // before background session cleanup.
        InOrder inOrder = inOrder(mKeyedService);
        inOrder.verify(mKeyedService).addObserver(ActorMetrics.getInstance());
        inOrder.verify(mKeyedService).addObserver(mManager);

        ProfileManager.onProfileDestroyed(mProfile);
        verify(mKeyedService, atLeastOnce()).removeObserver(mManager);
    }

    @Test
    public void testRedundantUpdatesSkipped() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        // First call should trigger startOrUpdateForegroundService
        verify(mServiceController).startOrUpdateForegroundService(eq(1), eq(mNotification));

        clearInvocations(mServiceController);
        when(mServiceController.isConnected()).thenReturn(true);

        // Second call with same notification should NOT trigger startOrUpdateForegroundService
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();
        verify(mServiceController, never()).startOrUpdateForegroundService(anyInt(), any());
    }

    @Test
    public void testMaybeStopServiceNow_ActiveTaskRemains_TransfersPin() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        int demotedTaskId = 1;
        int activeTaskId = 2;

        ActorTask task2 = mock(ActorTask.class);
        when(task2.getId()).thenReturn(activeTaskId);
        when(mKeyedService.getTask(activeTaskId)).thenReturn(task2);
        Notification notification2 = mock(Notification.class);
        when(mNotificationService.getForegroundNotification(eq(task2), anyBoolean(), anyBoolean()))
                .thenReturn(notification2);

        mManager.onTaskStateChanged(demotedTaskId, ActorTaskState.ACTING);
        mManager.onTaskStateChanged(activeTaskId, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();
        clearInvocations(mServiceController);
        when(mServiceController.isConnected()).thenReturn(true);
        when(mKeyedService.getCurrentActiveTask()).thenReturn(task2);

        mManager.maybeStopServiceNow();

        verify(mServiceController)
                .startOrUpdateForegroundService(eq(activeTaskId), eq(notification2));
        assertTrue(
                "Service should remain bound while another task is active.",
                mManager.isServiceBoundForTesting());
        verify(mServiceController, never()).stopActorForegroundService(anyInt());
    }

    @Test
    public void testMaybeStopServiceNow_PendingDemotionRemains_TransfersPin() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        int demotedTaskId = 1;
        int pendingTaskId = 2;

        Notification notification2 = mock(Notification.class);
        when(mNotificationService.getCachedNotification(
                        eq(pendingTaskId), anyBoolean(), anyBoolean()))
                .thenReturn(notification2);

        mManager.onTaskStateChanged(demotedTaskId, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        when(mTask.isCompleted()).thenReturn(true);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);
        when(mNotificationService.hasPendingDemotions()).thenReturn(true);
        when(mNotificationService.getPendingDemotionTaskIds())
                .thenReturn(Collections.singleton(demotedTaskId));
        mManager.onTaskStateChanged(demotedTaskId, ActorTaskState.FINISHED);
        clearInvocations(mServiceController);
        when(mServiceController.isConnected()).thenReturn(true);

        when(mNotificationService.getPendingDemotionTaskIds())
                .thenReturn(Collections.singleton(pendingTaskId));
        mManager.maybeStopServiceNow();

        verify(mServiceController)
                .startOrUpdateForegroundService(eq(pendingTaskId), eq(notification2));
        assertTrue(
                "Service should remain bound until the last notification is demoted.",
                mManager.isServiceBoundForTesting());
    }

    @Test
    public void testPinTransfer_RepostsUnpinnedTaskWithPendingDemotion() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        int completedTaskId = 1;
        int activeTaskId = 2;

        mManager.onTaskStateChanged(completedTaskId, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        ActorTask task2 = mock(ActorTask.class);
        when(task2.getId()).thenReturn(activeTaskId);
        when(mKeyedService.getTask(activeTaskId)).thenReturn(task2);
        when(mKeyedService.getCurrentActiveTask()).thenReturn(task2);
        Notification notification2 = mock(Notification.class);
        when(mNotificationService.getForegroundNotification(eq(task2), anyBoolean(), anyBoolean()))
                .thenReturn(notification2);

        mManager.onTaskStateChanged(activeTaskId, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        verify(mServiceController)
                .startOrUpdateForegroundService(eq(activeTaskId), eq(notification2));
        verify(mNotificationService).repostNotification(completedTaskId, /* isWarning= */ false);
    }

    @Test
    public void testPinTransfer_WarningTask_RepostsUnpinnedTaskWithWarning() {
        ActorTaskTimeoutManager timeoutManager = mock(ActorTaskTimeoutManager.class);
        mManager.setTimeoutManagerForTesting(timeoutManager);
        int warningTaskId = 1;
        int activeTaskId = 2;

        mManager.onTaskStateChanged(warningTaskId, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();
        verify(mNotificationService).getForegroundNotification(eq(mTask), anyBoolean(), eq(false));

        ActorTask task2 = mock(ActorTask.class);
        when(task2.getId()).thenReturn(activeTaskId);
        when(mKeyedService.getTask(activeTaskId)).thenReturn(task2);
        when(mKeyedService.getCurrentActiveTask()).thenReturn(task2);
        when(timeoutManager.isWarningMode(warningTaskId)).thenReturn(true);

        mManager.onTaskStateChanged(activeTaskId, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        verify(mNotificationService).repostNotification(warningTaskId, /* isWarning= */ true);
    }

    @Test
    public void testOnTaskStateChanged_PinsCorrectNotification() {
        mManager.setKeyedServiceForTesting(mKeyedService);

        // Initial state
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();
        verify(mServiceController).startOrUpdateForegroundService(eq(1), eq(mNotification));

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
        verify(mServiceController).startOrUpdateForegroundService(eq(2), eq(notification2));
    }

    @Test
    public void testOnPiPToTabTransition_UpdateForegroundServiceSkipped() {
        mManager.setKeyedServiceForTesting(mKeyedService);

        when(mServiceController.isActivityVisibleForTabs(any())).thenReturn(false);
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        verify(mServiceController).startOrUpdateForegroundService(eq(1), eq(mNotification));

        clearInvocations(mServiceController);
        when(mServiceController.isConnected()).thenReturn(true);

        when(mServiceController.isActivityVisibleForTabs(any())).thenReturn(true);
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        // Should skip updating foreground service when notification state doesn't change.
        verify(mServiceController, never()).startOrUpdateForegroundService(anyInt(), any());
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
        verify(mServiceController).stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_REMOVE);
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

    @Test
    public void testOnNotificationDismissed_NoActiveTasks_StopsServiceImmediately() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        int taskId = 1;

        mManager.onTaskStateChanged(taskId, ActorTaskState.ACTING);
        assertTrue(mManager.isServiceBoundForTesting());
        ShadowLooper.idleMainLooper();

        when(mTask.isCompleted()).thenReturn(true);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);
        when(mNotificationService.hasPendingDemotions()).thenReturn(true);
        mManager.onTaskStateChanged(taskId, ActorTaskState.FINISHED);

        assertTrue(mManager.isServiceBoundForTesting());

        when(mNotificationService.hasPendingDemotions()).thenReturn(false);
        mManager.onNotificationDismissed(taskId);

        verify(mNotificationService).clearTaskData(taskId);
        assertFalse(mManager.isServiceBoundForTesting());
        verify(mServiceController).stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_REMOVE);
        verify(mServiceController).unbindService();
    }

    @Test
    public void testOnNotificationDismissed_NotPinnedTask_RemovesAndRepostsNotification() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        int pinnedTaskId = 1;
        int otherTaskId = 2;

        mManager.onTaskStateChanged(pinnedTaskId, ActorTaskState.ACTING);
        assertTrue(mManager.isServiceBoundForTesting());
        ShadowLooper.idleMainLooper();

        when(mTask.isCompleted()).thenReturn(true);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);
        when(mNotificationService.hasPendingDemotions()).thenReturn(false);
        mManager.onTaskStateChanged(pinnedTaskId, ActorTaskState.FINISHED);

        // Dismiss a different task (not the pinned one)
        mManager.onNotificationDismissed(otherTaskId);

        verify(mNotificationService).clearTaskData(otherTaskId);
        assertFalse(mManager.isServiceBoundForTesting());
        verify(mServiceController).stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_REMOVE);
        verify(mNotificationService).repostNotification(pinnedTaskId, /* isWarning= */ false);
        verify(mServiceController).unbindService();
    }

    @Test
    public void testOnNotificationDismissed_ActiveTasksRemain_DoesNotStopService() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        int taskId1 = 1;
        int taskId2 = 2;

        ActorTask task2 = mock(ActorTask.class);
        when(task2.getId()).thenReturn(taskId2);
        when(task2.isCompleted()).thenReturn(false);
        when(task2.isUnderActorControl()).thenReturn(true);
        when(mKeyedService.getTask(taskId2)).thenReturn(task2);

        mManager.onTaskStateChanged(taskId1, ActorTaskState.ACTING);
        mManager.onTaskStateChanged(taskId2, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();
        assertTrue(mManager.isServiceBoundForTesting());

        when(mTask.isCompleted()).thenReturn(true);
        when(mKeyedService.getActiveTasksCount()).thenReturn(1);
        mManager.onTaskStateChanged(taskId1, ActorTaskState.FINISHED);

        mManager.onNotificationDismissed(taskId1);

        verify(mNotificationService).clearTaskData(taskId1);
        assertTrue(
                "Service should remain bound because task 2 is still active.",
                mManager.isServiceBoundForTesting());
        verify(mServiceController, never()).stopActorForegroundService(anyInt());
        verify(mServiceController, never()).unbindService();
    }

    @Test
    public void testMaybeStopServiceNow_NoActiveTasks_StopsService() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        int taskId = 1;

        mManager.onTaskStateChanged(taskId, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        when(mTask.isCompleted()).thenReturn(true);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);
        when(mNotificationService.hasPendingDemotions()).thenReturn(true);
        mManager.onTaskStateChanged(taskId, ActorTaskState.FINISHED);

        assertTrue(mManager.isServiceBoundForTesting());

        when(mNotificationService.hasPendingDemotions()).thenReturn(false);
        mManager.maybeStopServiceNow();

        assertFalse(
                "Service should be unbound after maybeStopServiceNow.",
                mManager.isServiceBoundForTesting());
        verify(mServiceController).stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_REMOVE);
        verify(mNotificationService).repostNotification(taskId, /* isWarning= */ false);
        verify(mServiceController).unbindService();
    }

    @Test
    public void testStopAndUnbindService_WarningTask_RepostsWithWarning() {
        ActorTaskTimeoutManager timeoutManager = mock(ActorTaskTimeoutManager.class);
        mManager.setTimeoutManagerForTesting(timeoutManager);
        int taskId = 1;

        mManager.onTaskStateChanged(taskId, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();
        when(timeoutManager.isWarningMode(taskId)).thenReturn(true);

        mManager.stopAndUnbindService();

        verify(mNotificationService).repostNotification(taskId, /* isWarning= */ true);
    }

    @Test
    public void testTaskCompleted_PendingDemotions_DelaysStopServiceForDemotionDelay() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        ActorNotificationService.setDemotionDelayMsForTesting(30000);
        int taskId = 1;

        mManager.onTaskStateChanged(taskId, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        when(mTask.isCompleted()).thenReturn(true);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);
        when(mNotificationService.hasPendingDemotions()).thenReturn(true);
        mManager.onTaskStateChanged(taskId, ActorTaskState.FINISHED);

        // Service should still be bound at 10 seconds.
        ShadowLooper.idleMainLooper(10, TimeUnit.SECONDS);
        assertTrue(
                "Service should still be bound before demotion delay passes.",
                mManager.isServiceBoundForTesting());

        // Advance by remaining 20 seconds; now demotion delay has passed.
        when(mNotificationService.hasPendingDemotions()).thenReturn(false);
        ShadowLooper.idleMainLooper(20, TimeUnit.SECONDS);
        assertFalse(
                "Service should be unbound after demotion delay.",
                mManager.isServiceBoundForTesting());
    }

    @Test
    public void testMaybeStopServiceRunnable_PendingDemotions_PostponesStop() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        ActorNotificationService.setDemotionDelayMsForTesting(30000);
        int taskId = 1;

        mManager.onTaskStateChanged(taskId, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        when(mTask.isCompleted()).thenReturn(true);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);
        when(mNotificationService.hasPendingDemotions()).thenReturn(true);
        mManager.onTaskStateChanged(taskId, ActorTaskState.FINISHED);

        assertTrue(mManager.isServiceBoundForTesting());

        // When delay passes but pending demotions still exist, service should remain bound.
        ShadowLooper.idleMainLooper(30, TimeUnit.SECONDS);
        assertTrue(mManager.isServiceBoundForTesting());
        verify(mServiceController, never()).stopActorForegroundService(anyInt());

        // Once pending demotions are cleared, service stops.
        when(mNotificationService.hasPendingDemotions()).thenReturn(false);
        ShadowLooper.idleMainLooper(30, TimeUnit.SECONDS);
        assertFalse(mManager.isServiceBoundForTesting());
        verify(mServiceController).stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_REMOVE);
    }

    @Test
    public void testOnPendingTaskCountChanged_StartsServiceWithPlaceholderNotification() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);

        mManager.onPendingTaskCountChanged(1);
        ShadowLooper.idleMainLooper();

        assertEquals(1, mManager.getPendingTaskCountForTesting());
        assertTrue(mManager.hasActiveOrPendingTasks());
        assertFalse(mManager.hasActiveTasks());
        assertTrue(mManager.hasPendingTasks());
        assertTrue("Service should be bound on pending task.", mManager.isServiceBoundForTesting());
        verify(mServiceController).startAndBindService(any());
        verify(mServiceController)
                .startOrUpdateForegroundService(
                        eq(ActorNotificationFactory.TASK_STARTS_SOON_NOTIFICATION_ID), any());
    }

    @Test
    public void testHandoffFromPendingToActive_UpdatesNotification() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);

        mManager.onPendingTaskCountChanged(1);
        ShadowLooper.idleMainLooper();
        assertTrue(mManager.isServiceBoundForTesting());

        when(mKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mTask.isCompleted()).thenReturn(false);
        mManager.onPendingTaskCountChanged(0);
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        assertTrue(mManager.isServiceBoundForTesting());
        verify(mServiceController).startOrUpdateForegroundService(eq(1), eq(mNotification));
        verify(mNotificationService, never())
                .onNotificationUnpinned(ActorNotificationFactory.TASK_STARTS_SOON_NOTIFICATION_ID);
    }

    @Test
    public void testTaskCompleted_PendingTaskRemaining_KeepsServiceAlive() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        int taskId = 1;

        mManager.onTaskStateChanged(taskId, ActorTaskState.ACTING);
        mManager.onPendingTaskCountChanged(1);
        ShadowLooper.idleMainLooper();

        // Active task completes, but 1 pending task remains.
        when(mTask.isCompleted()).thenReturn(true);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);
        mManager.onTaskStateChanged(taskId, ActorTaskState.FINISHED);

        ShadowLooper.idleMainLooper(30, TimeUnit.SECONDS);

        assertTrue(
                "Service should remain bound because pending tasks > 0.",
                mManager.isServiceBoundForTesting());
        verify(mServiceController, never()).stopActorForegroundService(anyInt());
    }

    @Test
    public void testPendingTaskRemoved_NoActiveTasks_StopsService() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);

        mManager.onPendingTaskCountChanged(1);
        ShadowLooper.idleMainLooper();
        assertTrue(mManager.isServiceBoundForTesting());

        // Pending task cancelled/removed
        mManager.onPendingTaskCountChanged(0);
        ShadowLooper.idleMainLooper(30, TimeUnit.SECONDS);

        assertFalse(
                "Service should be unbound when all pending and active tasks are 0.",
                mManager.isServiceBoundForTesting());
        verify(mServiceController).stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_REMOVE);
    }

    @Test
    public void testMaybeStopServiceNow_PendingTasksRemain_DoesNotStopService() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);

        mManager.onPendingTaskCountChanged(1);
        ShadowLooper.idleMainLooper();
        assertTrue(mManager.isServiceBoundForTesting());

        // Attempt immediate stop (e.g. from demotion or notification dismissal) while pending task
        // exists.
        mManager.maybeStopServiceNow();

        assertTrue(
                "Service should remain bound because pending tasks > 0.",
                mManager.isServiceBoundForTesting());
        verify(mServiceController, never()).stopActorForegroundService(anyInt());
        verify(mServiceController, never()).unbindService();
    }

    @Test
    public void testOnPendingTaskCountChanged_MultipleTimes_DoesNotRecreatePlaceholder() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);

        mManager.onPendingTaskCountChanged(1);
        ShadowLooper.idleMainLooper();

        verify(mServiceController, times(1))
                .startOrUpdateForegroundService(
                        eq(ActorNotificationFactory.TASK_STARTS_SOON_NOTIFICATION_ID), any());

        // Incrementing pending count while placeholder is already pinned should not re-trigger
        // startOrUpdateForegroundService for the placeholder notification.
        mManager.onPendingTaskCountChanged(2);
        ShadowLooper.idleMainLooper();

        verify(mServiceController, times(1))
                .startOrUpdateForegroundService(
                        eq(ActorNotificationFactory.TASK_STARTS_SOON_NOTIFICATION_ID), any());
    }

    @Test
    public void testTaskCompleted_PendingTaskRemaining_RepostsCompletedNotification() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        int taskId = 1;

        mManager.onTaskStateChanged(taskId, ActorTaskState.ACTING);
        mManager.onPendingTaskCountChanged(1);
        ShadowLooper.idleMainLooper();

        // Active task completes while pending tasks remain.
        when(mTask.isCompleted()).thenReturn(true);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);
        mManager.onTaskStateChanged(taskId, ActorTaskState.FINISHED);
        ShadowLooper.idleMainLooper();

        // Pin transfers to the placeholder notification. The previously pinned task notification
        // is removed with the foreground state and reposted.
        verify(mServiceController)
                .startOrUpdateForegroundService(
                        eq(ActorNotificationFactory.TASK_STARTS_SOON_NOTIFICATION_ID), any());
        verify(mNotificationService).repostNotification(taskId, /* isWarning= */ false);
    }

    @Test
    public void testExternalTriggerPendingTask_NoFcmBeforeTimeout_StopsService() {
        mManager.setKeyedServiceForTesting(mKeyedService);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);

        // The external trigger intent adds a pending task, which starts the service with the
        // placeholder notification.
        mManager.onPendingTaskCountChanged(1);
        ShadowLooper.idleMainLooper();
        assertTrue(
                "Service should be bound while a pending task exists.",
                mManager.isServiceBoundForTesting());
        verify(mServiceController)
                .startOrUpdateForegroundService(
                        eq(ActorNotificationFactory.TASK_STARTS_SOON_NOTIFICATION_ID), any());

        // The manager has no timeout of its own. The service stays bound for the whole window
        // that ActorKeyedService waits for the FCM message.
        ShadowLooper.idleMainLooper(
                ActorTaskTimeoutParameters.getPreparingToStartTaskTimeoutMs() - 1,
                TimeUnit.MILLISECONDS);
        assertTrue(
                "Service should stay bound while the pending task remains.",
                mManager.isServiceBoundForTesting());
        verify(mServiceController, never()).stopActorForegroundService(anyInt());
        verify(mServiceController, never()).unbindService();

        // No FCM message arrived, so ActorKeyedService removes the pending task.
        mManager.onPendingTaskCountChanged(0);
        ShadowLooper.idleMainLooper();

        assertFalse(
                "Service should stop and unbind once the pending count drops to 0.",
                mManager.isServiceBoundForTesting());
        verify(mServiceController).stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_REMOVE);
        verify(mServiceController).unbindService();
    }

    @Test
    public void testPinAndUnpinOptInNotification_StartsAndStopsService() {
        MockNotificationManagerProxy mockNotificationManager = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mockNotificationManager);
        try {
            when(mKeyedService.getActiveTasksCount()).thenReturn(0);
            mockNotificationManager.notify(
                    new NotificationWrapper(
                            mNotification,
                            new NotificationMetadata(
                                    0, /* notificationTag= */ null, OPT_IN_NOTIFICATION_ID)));
            assertEquals(1, mockNotificationManager.getNotifications().size());

            mManager.pinOptInNotification(/* cancelCallback= */ null);
            ShadowLooper.idleMainLooper();

            assertTrue(mManager.hasOptInNotification());
            assertTrue(mManager.isServiceBoundForTesting());
            verify(mServiceController).startAndBindService(any());
            ArgumentCaptor<Notification> notificationCaptor =
                    ArgumentCaptor.forClass(Notification.class);
            verify(mServiceController)
                    .startOrUpdateForegroundService(
                            eq(OPT_IN_NOTIFICATION_ID), notificationCaptor.capture());
            assertEquals(
                    ContextUtils.getApplicationContext()
                            .getString(R.string.glic_experimental_opt_in_notification_title),
                    notificationCaptor.getValue().extras.getString(Notification.EXTRA_TITLE));
            verify(mNotificationService, never()).onNotificationPinned(OPT_IN_NOTIFICATION_ID);

            mManager.unpinOptInNotification();
            ShadowLooper.idleMainLooper();

            assertFalse(mManager.hasOptInNotification());
            assertFalse(mManager.isServiceBoundForTesting());
            assertEquals(0, mockNotificationManager.getNotifications().size());
            verify(mServiceController)
                    .stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_REMOVE);
            verify(mServiceController).unbindService();
            verify(mNotificationService, never()).onNotificationUnpinned(OPT_IN_NOTIFICATION_ID);
            verify(mNotificationService, never())
                    .repostNotification(eq(OPT_IN_NOTIFICATION_ID), anyBoolean());
        } finally {
            BaseNotificationManagerProxyFactory.setInstanceForTesting(null);
        }
    }

    @Test
    public void testOptInNotificationReplacedByActiveTask_TransitionsToTaskNotification() {
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);

        mManager.pinOptInNotification(mCancelCallback);
        ShadowLooper.idleMainLooper();
        assertTrue(mManager.hasOptInNotification());
        verify(mServiceController)
                .startOrUpdateForegroundService(
                        eq(OPT_IN_NOTIFICATION_ID), any(Notification.class));

        when(mKeyedService.getActiveTasksCount()).thenReturn(1);
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        assertFalse(mManager.hasOptInNotification());
        verify(mCancelCallback, times(1)).run();
        verify(mServiceController).startOrUpdateForegroundService(eq(1), eq(mNotification));
        verify(mNotificationService, never()).onNotificationUnpinned(OPT_IN_NOTIFICATION_ID);
        verify(mNotificationService, never())
                .repostNotification(eq(OPT_IN_NOTIFICATION_ID), anyBoolean());

        clearInvocations(mServiceController);
        when(mTask.isCompleted()).thenReturn(true);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);
        when(mNotificationService.hasPendingDemotions()).thenReturn(false);
        mManager.onTaskStateChanged(1, ActorTaskState.FINISHED);
        ShadowLooper.idleMainLooper(30, TimeUnit.SECONDS);

        assertFalse(mManager.isServiceBoundForTesting());
        verify(mServiceController, never())
                .startOrUpdateForegroundService(eq(OPT_IN_NOTIFICATION_ID), any());
        verify(mServiceController).stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_REMOVE);
        verify(mServiceController).unbindService();
    }

    @Test
    public void testUnpinOptInNotificationWithPendingDemotion_ReplacesPinAndKillsOptIn() {
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);

        mManager.pinOptInNotification(/* cancelCallback= */ null);
        ShadowLooper.idleMainLooper();

        clearInvocations(mServiceController);
        when(mNotificationService.hasPendingDemotions()).thenReturn(true);
        when(mNotificationService.getPendingDemotionTaskIds()).thenReturn(Collections.singleton(1));
        when(mNotificationService.getCachedNotification(eq(1), anyBoolean(), anyBoolean()))
                .thenReturn(mNotification);

        mManager.unpinOptInNotification();
        ShadowLooper.idleMainLooper();

        assertTrue(mManager.isServiceBoundForTesting());
        verify(mServiceController).startOrUpdateForegroundService(eq(1), eq(mNotification));
        verify(mNotificationService, never()).onNotificationUnpinned(OPT_IN_NOTIFICATION_ID);
        verify(mNotificationService, never())
                .repostNotification(eq(OPT_IN_NOTIFICATION_ID), anyBoolean());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testPinOptInNotification_AllowsStartForegroundWhenBackgrounded() {
        ActorForegroundServiceManager realManager = new ActorForegroundServiceManager();
        try {
            realManager.setNotificationServiceForTesting(mNotificationService);
            realManager.setKeyedServiceForTesting(mKeyedService);
            when(mKeyedService.getActiveTasksCount()).thenReturn(0);

            assertFalse(realManager.canStartForeground());

            realManager.pinOptInNotification(/* cancelCallback= */ null);
            assertTrue(realManager.isServiceBoundForTesting());
            assertTrue(realManager.canStartForeground());

            ShadowLooper.idleMainLooper();

            verify(mServiceController)
                    .startOrUpdateForegroundService(
                            eq(OPT_IN_NOTIFICATION_ID), any(Notification.class));

            realManager.unpinOptInNotification();
            ShadowLooper.idleMainLooper();

            assertFalse(realManager.canStartForeground());
            assertFalse(realManager.isServiceBoundForTesting());
        } finally {
            realManager.shutDown();
        }
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testStartAndBindService_ThrowsIllegalStateException_ResetsBoundState() {
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);
        doThrow(new IllegalStateException("ForegroundServiceStartNotAllowedException"))
                .when(mServiceController)
                .startAndBindService(any());

        mManager.pinOptInNotification(mCancelCallback);
        ShadowLooper.idleMainLooper();

        assertFalse(mManager.hasOptInNotification());
        assertFalse(mManager.isServiceBoundForTesting());
        verify(mCancelCallback, times(1)).run();
        verify(mServiceController, never()).startOrUpdateForegroundService(anyInt(), any());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void
            testStartOrUpdateForegroundService_ThrowsIllegalStateException_CancelsOptInAndStopsService() {
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);
        doThrow(new IllegalStateException("ForegroundServiceStartNotAllowedException"))
                .when(mServiceController)
                .startOrUpdateForegroundService(eq(OPT_IN_NOTIFICATION_ID), any());

        mManager.pinOptInNotification(mCancelCallback);
        ShadowLooper.idleMainLooper();

        assertFalse(mManager.hasOptInNotification());
        assertFalse(mManager.isServiceBoundForTesting());
        verify(mCancelCallback, times(1)).run();
    }

    @Test
    public void testShutDown_WithActiveOptInNotification_CancelsOptInNotification() {
        MockNotificationManagerProxy mockNotificationManager = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mockNotificationManager);
        try {
            when(mKeyedService.getActiveTasksCount()).thenReturn(0);
            mockNotificationManager.notify(
                    new NotificationWrapper(
                            mNotification,
                            new NotificationMetadata(
                                    0, /* notificationTag= */ null, OPT_IN_NOTIFICATION_ID)));
            assertEquals(1, mockNotificationManager.getNotifications().size());

            mManager.pinOptInNotification(mCancelCallback);
            ShadowLooper.idleMainLooper();
            assertTrue(mManager.hasOptInNotification());
            assertTrue(mManager.isServiceBoundForTesting());

            mManager.shutDown();
            ShadowLooper.idleMainLooper();

            assertFalse(mManager.hasOptInNotification());
            assertFalse(mManager.isServiceBoundForTesting());
            assertEquals(0, mockNotificationManager.getNotifications().size());
            verify(mCancelCallback, times(1)).run();
        } finally {
            BaseNotificationManagerProxyFactory.setInstanceForTesting(null);
        }
    }

    @Test
    public void testProfileDestroyed_WithActiveOptInNotification_CancelsOptInNotification() {
        MockNotificationManagerProxy mockNotificationManager = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mockNotificationManager);
        try {
            when(mKeyedService.getActiveTasksCount()).thenReturn(0);
            mockNotificationManager.notify(
                    new NotificationWrapper(
                            mNotification,
                            new NotificationMetadata(
                                    0, /* notificationTag= */ null, OPT_IN_NOTIFICATION_ID)));
            assertEquals(1, mockNotificationManager.getNotifications().size());

            mManager.pinOptInNotification(mCancelCallback);
            ShadowLooper.idleMainLooper();
            assertTrue(mManager.hasOptInNotification());
            assertTrue(mManager.isServiceBoundForTesting());

            mManager.setKeyedServiceForTesting(null);
            ShadowLooper.idleMainLooper();

            assertFalse(mManager.hasOptInNotification());
            assertFalse(mManager.isServiceBoundForTesting());
            assertEquals(0, mockNotificationManager.getNotifications().size());
            verify(mCancelCallback, times(1)).run();
        } finally {
            BaseNotificationManagerProxyFactory.setInstanceForTesting(null);
        }
    }

    @Test
    public void
            testOnServiceDestroyed_WithActiveOptInNotification_CancelsWithoutReentrantServiceStop() {
        MockNotificationManagerProxy mockNotificationManager = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mockNotificationManager);
        try {
            when(mKeyedService.getActiveTasksCount()).thenReturn(0);
            mockNotificationManager.notify(
                    new NotificationWrapper(
                            mNotification,
                            new NotificationMetadata(
                                    0, /* notificationTag= */ null, OPT_IN_NOTIFICATION_ID)));
            assertEquals(1, mockNotificationManager.getNotifications().size());

            mManager.pinOptInNotification(mCancelCallback);
            ShadowLooper.idleMainLooper();
            assertTrue(mManager.hasOptInNotification());
            clearInvocations(mServiceController);

            mManager.onServiceDestroyed();

            assertFalse(mManager.hasOptInNotification());
            assertEquals(0, mockNotificationManager.getNotifications().size());
            verify(mCancelCallback, times(1)).run();
            verify(mServiceController, never()).stopActorForegroundService(anyInt());
            verify(mServiceController, never()).unbindService();
        } finally {
            BaseNotificationManagerProxyFactory.setInstanceForTesting(null);
        }
    }

    @Test
    public void testCancelOptInNotification_DismissesStopsServiceAndRunsCallback() {
        MockNotificationManagerProxy mockNotificationManager = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mockNotificationManager);
        try {
            when(mKeyedService.getActiveTasksCount()).thenReturn(0);
            mockNotificationManager.notify(
                    new NotificationWrapper(
                            mNotification,
                            new NotificationMetadata(
                                    0, /* notificationTag= */ null, OPT_IN_NOTIFICATION_ID)));
            assertEquals(1, mockNotificationManager.getNotifications().size());

            mManager.pinOptInNotification(mCancelCallback);
            ShadowLooper.idleMainLooper();
            assertTrue(mManager.hasOptInNotification());
            assertTrue(mManager.isServiceBoundForTesting());

            mManager.cancelOptInNotification();
            ShadowLooper.idleMainLooper();

            assertFalse(mManager.hasOptInNotification());
            assertFalse(mManager.isServiceBoundForTesting());
            assertEquals(0, mockNotificationManager.getNotifications().size());
            verify(mServiceController)
                    .stopActorForegroundService(ServiceCompat.STOP_FOREGROUND_REMOVE);
            verify(mServiceController).unbindService();
            verify(mCancelCallback, times(1)).run();
        } finally {
            BaseNotificationManagerProxyFactory.setInstanceForTesting(null);
        }
    }

    @Test
    public void testActorBroadcastReceiver_ActionCancelWithoutTaskId_CancelsOptInNotification() {
        MockNotificationManagerProxy mockNotificationManager = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mockNotificationManager);
        try {
            when(mKeyedService.getActiveTasksCount()).thenReturn(0);
            mManager.pinOptInNotification(mCancelCallback);
            ShadowLooper.idleMainLooper();
            assertTrue(mManager.hasOptInNotification());

            Intent cancelIntent =
                    new Intent(ContextUtils.getApplicationContext(), ActorBroadcastReceiver.class)
                            .setAction(NotificationConstants.ACTION_ACTOR_CANCEL);
            new ActorBroadcastReceiver()
                    .onReceive(ContextUtils.getApplicationContext(), cancelIntent);
            ShadowLooper.idleMainLooper();

            assertFalse(mManager.hasOptInNotification());
            assertFalse(mManager.isServiceBoundForTesting());
            verify(mCancelCallback, times(1)).run();
        } finally {
            BaseNotificationManagerProxyFactory.setInstanceForTesting(null);
        }
    }

    @Test
    public void
            testPinOptInNotification_PreemptedByActiveTaskBeforePinning_InvokesCancelCallback() {
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);
        when(mNotificationService.hasPendingDemotions()).thenReturn(true);
        when(mNotificationService.getPendingDemotionTaskIds()).thenReturn(Collections.singleton(1));
        when(mNotificationService.getCachedNotification(eq(1), anyBoolean(), anyBoolean()))
                .thenReturn(mNotification);

        mManager.pinOptInNotification(mCancelCallback);
        ShadowLooper.idleMainLooper();
        assertTrue(mManager.hasOptInNotification());

        when(mNotificationService.hasPendingDemotions()).thenReturn(false);
        when(mNotificationService.getPendingDemotionTaskIds()).thenReturn(Collections.emptySet());
        when(mKeyedService.getActiveTasksCount()).thenReturn(1);
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        assertFalse(mManager.hasOptInNotification());
        verify(mCancelCallback, times(1)).run();

        clearInvocations(mServiceController);
        when(mTask.isCompleted()).thenReturn(true);
        when(mKeyedService.getActiveTasksCount()).thenReturn(0);
        mManager.onTaskStateChanged(1, ActorTaskState.FINISHED);
        ShadowLooper.idleMainLooper(30, TimeUnit.SECONDS);

        assertFalse(mManager.isServiceBoundForTesting());
        verify(mServiceController, never())
                .startOrUpdateForegroundService(eq(OPT_IN_NOTIFICATION_ID), any());
    }

    @Test
    public void testPinOptInNotification_WhenTaskAlreadyActive_InvokesCancelCallbackImmediately() {
        when(mKeyedService.getActiveTasksCount()).thenReturn(1);
        mManager.onTaskStateChanged(1, ActorTaskState.ACTING);
        ShadowLooper.idleMainLooper();

        mManager.pinOptInNotification(mCancelCallback);
        ShadowLooper.idleMainLooper();

        assertFalse(mManager.hasOptInNotification());
        verify(mCancelCallback, times(1)).run();
    }
}

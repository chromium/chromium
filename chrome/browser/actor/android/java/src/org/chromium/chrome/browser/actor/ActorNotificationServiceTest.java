// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.app.Notification;
import android.content.Context;
import android.content.Intent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.actor.ui.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

/** Unit tests for {@link ActorNotificationService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActorNotificationServiceTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActorKeyedService mKeyedService;
    @Mock private ActorTask mTask;
    @Mock private ActorForegroundServiceController mServiceController;

    private ActorNotificationService mNotificationService;
    private MockNotificationManagerProxy mMockNotificationManager;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mMockNotificationManager = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mMockNotificationManager);
        ActorForegroundServiceController.setInstanceForTesting(mServiceController);
        mNotificationService = new ActorNotificationService(mKeyedService);
    }

    @After
    public void tearDown() {
        mNotificationService.clearAll();
    }

    @Test
    public void testGetForegroundNotification_TaskNull() {
        assertNull(mNotificationService.getForegroundNotification(null, false, false));
    }

    @Test
    public void testGetForegroundNotification_TaskValid() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        Notification notification =
                mNotificationService.getForegroundNotification(
                        mTask, /* isSilent= */ false, /* isWarning= */ false);

        assertNotNull(notification);
        assertEquals(
                mContext.getString(R.string.actor_notification_title_working_on_task),
                notification.extras.getString(Notification.EXTRA_TITLE));
        // getForegroundNotification calls getCachedNotification, which shouldn't notify.
        assertEquals(0, mMockNotificationManager.getNotifications().size());
    }

    @Test
    public void testUpdateNotificationForTask_TaskExists() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ false);

        Notification notification =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(notification);
        assertEquals(
                mContext.getString(R.string.actor_notification_title_working_on_task),
                notification.extras.getString(Notification.EXTRA_TITLE));
        // updateNotificationForTask should have notified.
        assertEquals(1, mMockNotificationManager.getNotifications().size());
    }

    @Test
    public void testUpdateNotificationForTask_SilentAndWarning() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        // Test silent notification
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.ACTING, /* isSilent= */ true, /* isWarning= */ false);
        Notification notification =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ true, /* isWarning= */ false);
        assertNotNull(notification);

        // Test warning notification
        mNotificationService.updateNotificationForTask(
                taskId,
                ActorTaskState.PAUSED_BY_ACTOR,
                /* isSilent= */ false,
                /* isWarning= */ true);
        notification =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ true);
        assertNotNull(notification);
        assertEquals(
                mContext.getString(R.string.actor_notification_title_will_stop_task),
                notification.extras.getString(Notification.EXTRA_TITLE));
    }

    @Test
    public void testUpdateNotificationForTask_WarningWhenRunning() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        // Task is in a running state, but isWarning is true.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ true);

        Notification notification =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ true);
        assertNotNull(notification);
        assertEquals(
                mContext.getString(R.string.actor_notification_title_will_stop_task),
                notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                mContext.getString(
                        R.string.actor_notification_body_will_stop_task_long_running, "Test Task"),
                notification.extras.getString(Notification.EXTRA_TEXT));
    }

    @Test
    public void testUpdateNotificationForTask_WarningWhenPaused() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.PAUSED_BY_ACTOR);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        mNotificationService.updateNotificationForTask(
                taskId,
                ActorTaskState.PAUSED_BY_ACTOR,
                /* isSilent= */ false,
                /* isWarning= */ true);

        Notification notification =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ true);
        assertNotNull(notification);
        assertEquals(
                mContext.getString(R.string.actor_notification_title_will_stop_task),
                notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                mContext.getString(
                        R.string.actor_notification_body_will_stop_task_long_running, "Test Task"),
                notification.extras.getString(Notification.EXTRA_TEXT));
    }

    @Test
    public void testUpdateNotificationForTask_WarningWhenWaitingOnUser() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.WAITING_ON_USER);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        mNotificationService.updateNotificationForTask(
                taskId,
                ActorTaskState.WAITING_ON_USER,
                /* isSilent= */ false,
                /* isWarning= */ true);

        Notification notification =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ true);
        assertNotNull(notification);
        assertEquals(
                mContext.getString(R.string.actor_notification_title_will_stop_task),
                notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals(
                mContext.getString(
                        R.string.actor_notification_body_will_stop_task_no_response, "Test Task"),
                notification.extras.getString(Notification.EXTRA_TEXT));
    }

    @Test
    public void testUpdateNotificationForTask_TaskRemoved() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ false);
        assertEquals(1, mMockNotificationManager.getNotifications().size());

        // Task is removed from KeyedService
        when(mKeyedService.getTask(taskId)).thenReturn(null);

        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.FINISHED, /* isSilent= */ false, /* isWarning= */ false);

        // Task won't be removed from notification cache.
        assertNotNull(
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ false));
    }

    @Test
    public void testGetCachedNotification_TaskExists() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        Notification notification =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ false);

        assertNotNull(notification);
        assertEquals(
                mContext.getString(R.string.actor_notification_title_working_on_task),
                notification.extras.getString(Notification.EXTRA_TITLE));
        // getCachedNotification shouldn't notify.
        assertEquals(0, mMockNotificationManager.getNotifications().size());
    }

    @Test
    public void testGetCachedNotification_TaskDoesNotExist() {
        int taskId = 1;
        when(mKeyedService.getTask(taskId)).thenReturn(null);

        Notification notification =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ false);

        assertNull(notification);
        assertEquals(0, mMockNotificationManager.getNotifications().size());
    }

    @Test
    public void testClearAll() {
        int taskId1 = 1;
        int taskId2 = 2;

        ActorTask task1 = org.mockito.Mockito.mock(ActorTask.class);
        when(task1.getId()).thenReturn(taskId1);
        when(task1.getTitle()).thenReturn("Task 1");

        ActorTask task2 = org.mockito.Mockito.mock(ActorTask.class);
        when(task2.getId()).thenReturn(taskId2);
        when(task2.getTitle()).thenReturn("Task 2");

        when(mKeyedService.getTask(taskId1)).thenReturn(task1);
        when(mKeyedService.getTask(taskId2)).thenReturn(task2);

        mNotificationService.updateNotificationForTask(
                taskId1, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ false);
        mNotificationService.updateNotificationForTask(
                taskId2, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ false);

        assertEquals(2, mMockNotificationManager.getNotifications().size());

        mNotificationService.clearAll();

        when(mKeyedService.getTask(taskId1)).thenReturn(null);
        when(mKeyedService.getTask(taskId2)).thenReturn(null);

        assertNull(
                mNotificationService.getCachedNotification(
                        taskId1, /* isSilent= */ false, /* isWarning= */ false));
        assertNull(
                mNotificationService.getCachedNotification(
                        taskId2, /* isSilent= */ false, /* isWarning= */ false));
    }

    @Test
    public void testUpdateNotificationForTask_SkipRedundantUpdates() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        // First update.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ false);
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());

        // Update to REFLECTING should be skipped.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.REFLECTING, /* isSilent= */ false, /* isWarning= */ false);
        assertEquals(0, mMockNotificationManager.getMutationCountAndDecrement());

        // Update to PAUSED_BY_USER should NOT be skipped.
        mNotificationService.updateNotificationForTask(
                taskId,
                ActorTaskState.PAUSED_BY_USER,
                /* isSilent= */ false,
                /* isWarning= */ false);
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());

        // Update to PAUSED_BY_ACTOR should be skipped.
        mNotificationService.updateNotificationForTask(
                taskId,
                ActorTaskState.PAUSED_BY_ACTOR,
                /* isSilent= */ false,
                /* isWarning= */ false);
        assertEquals(0, mMockNotificationManager.getMutationCountAndDecrement());

        // Update with isSilent changed should be skipped because we only update on state changes.
        mNotificationService.updateNotificationForTask(
                taskId,
                ActorTaskState.PAUSED_BY_ACTOR,
                /* isSilent= */ false,
                /* isWarning= */ true);
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());

        // Update back to isWarning=false with a different category should not be skipped.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ false);
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());
    }

    @Test
    public void testGetCachedNotification_UpdatesStateCache() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        // This should populate the state cache.
        mNotificationService.getCachedNotification(
                taskId, /* isSilent= */ false, /* isWarning= */ false);
        assertEquals(0, mMockNotificationManager.getMutationCountAndDecrement());

        // Now updateNotificationForTask with REFLECTING should be skipped.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.REFLECTING, /* isSilent= */ false, /* isWarning= */ false);
        assertEquals(0, mMockNotificationManager.getMutationCountAndDecrement());
    }

    @Test
    public void testTerminalNotificationsAreDismissible() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);
        when(mServiceController.createTrustedBringTabToFrontIntent(any())).thenReturn(new Intent());

        int[] terminalStates = {
            ActorTaskState.FINISHED, ActorTaskState.FAILED, ActorTaskState.CANCELLED
        };

        for (int state : terminalStates) {
            when(mTask.getState()).thenReturn(state);
            mNotificationService.updateNotificationForTask(
                    taskId, state, /* isSilent= */ false, /* isWarning= */ false);
            Notification notification =
                    mNotificationService.getCachedNotification(
                            taskId, /* isSilent= */ false, /* isWarning= */ false);
            assertNotNull("Notification should not be null for state: " + state, notification);
            assertFalse(
                    "Notification should NOT be ongoing for state: " + state,
                    (notification.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ACTOR_STEP_PROGRESS_NOTIFICATION)
    public void testUpdateNotificationForStepProgress_IsSilentAndUpdated() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mTask.getCurrentActionName()).thenReturn("Step 1");
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        // Initial notification post.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ false);
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());

        Notification notification =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(notification);
        assertEquals(
                mContext.getString(
                        R.string.actor_notification_body_working_with_step_info,
                        "Test Task",
                        "Step 1"),
                notification.extras.getString(Notification.EXTRA_TEXT));

        // Step text changes during ACTING state and step progress update is triggered.
        when(mTask.getCurrentActionName()).thenReturn("Step 2");
        mNotificationService.updateNotificationForStepProgress(taskId);

        // Notification is updated, not skipped.
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());

        NotificationWrapper wrapper =
                mNotificationService.getCachedNotificationWrapperForTesting(taskId);
        assertNotNull(wrapper);
        assertTrue(
                "Notification should be posted silently on step text update", wrapper.isSilent());
        assertEquals(
                mContext.getString(
                        R.string.actor_notification_body_working_with_step_info,
                        "Test Task",
                        "Step 2"),
                wrapper.getNotification().extras.getString(Notification.EXTRA_TEXT));
    }
}

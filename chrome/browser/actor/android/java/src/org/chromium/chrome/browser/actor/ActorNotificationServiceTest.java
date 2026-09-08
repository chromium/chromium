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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.actor.ui.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

/** Unit tests for {@link ActorNotificationService}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ACTOR_LIVE_NOTIFICATION)
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
        assertTrue(
                "Warning notification should request promoted ongoing",
                notification.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertEquals(
                "Warning status chip should be Review",
                mContext.getString(R.string.actor_notification_live_status_review),
                notification.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));
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
        assertTrue(
                "Warning notification should request promoted ongoing",
                notification.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertEquals(
                "Warning status chip should be Review",
                mContext.getString(R.string.actor_notification_live_status_review),
                notification.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));
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
        assertTrue(
                "Warning notification should request promoted ongoing",
                notification.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertEquals(
                "Warning status chip should be Review",
                mContext.getString(R.string.actor_notification_live_status_review),
                notification.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));
    }

    @Test
    public void testNeedsUserInputToWarningTransition_StatusChipPersistsReview() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.WAITING_ON_USER);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);
        when(mServiceController.createTrustedBringTabToFrontIntent(any())).thenReturn(new Intent());

        // Post waiting on user notification.
        mNotificationService.updateNotificationForTask(
                taskId,
                ActorTaskState.WAITING_ON_USER,
                /* isSilent= */ false,
                /* isWarning= */ false);
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());

        Notification userInputNotif =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(userInputNotif);
        assertTrue(
                "Needs user attention notification should be ongoing",
                (userInputNotif.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertTrue(
                "Needs user attention notification should request promoted ongoing",
                userInputNotif.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertEquals(
                "Needs user attention status chip should be Review",
                mContext.getString(R.string.actor_notification_live_status_review),
                userInputNotif.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));

        // Update to warning mode while in WAITING_ON_USER.
        mNotificationService.updateNotificationForTask(
                taskId,
                ActorTaskState.WAITING_ON_USER,
                /* isSilent= */ false,
                /* isWarning= */ true);
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());

        Notification warningNotif =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ true);
        assertNotNull(warningNotif);
        assertEquals(
                mContext.getString(R.string.actor_notification_title_will_stop_task),
                warningNotif.extras.getString(Notification.EXTRA_TITLE));
        assertTrue(
                "Warning notification should be ongoing",
                (warningNotif.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertTrue(
                "Warning notification should request promoted ongoing",
                warningNotif.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertEquals(
                "Warning notification status chip should remain Review",
                mContext.getString(R.string.actor_notification_live_status_review),
                warningNotif.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));

        // Post stopped notification on timeout.
        when(mTask.getState()).thenReturn(ActorTaskState.FAILED);
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.FAILED, /* isSilent= */ false, /* isWarning= */ false);
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());
        assertTrue(mNotificationService.hasPendingDemotionForTesting(taskId));

        Notification stoppedNotif =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(stoppedNotif);
        assertTrue(
                "Stopped live notification should be ongoing",
                (stoppedNotif.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertTrue(
                "Stopped live notification should request promoted ongoing",
                stoppedNotif.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertEquals(
                "Stopped notification status chip should be Stopped",
                mContext.getString(R.string.actor_notification_live_status_stopped),
                stoppedNotif.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));

        // Advance looper to fire demotion runnable.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertFalse(mNotificationService.hasPendingDemotionForTesting(taskId));
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());

        Notification demotedNotif =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(demotedNotif);
        assertFalse(
                "Demoted stopped notification should not be ongoing",
                (demotedNotif.flags & Notification.FLAG_ONGOING_EVENT) != 0);
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
    public void testTerminalNotificationStates() {
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mServiceController.createTrustedBringTabToFrontIntent(any())).thenReturn(new Intent());

        int[] terminalStates = {
            ActorTaskState.FINISHED, ActorTaskState.FAILED, ActorTaskState.CANCELLED
        };

        for (int state : terminalStates) {
            int taskId = state + 10;
            ActorTask task = org.mockito.Mockito.mock(ActorTask.class);
            when(task.getId()).thenReturn(taskId);
            when(task.getTitle()).thenReturn("Test Task " + state);
            when(task.getState()).thenReturn(state);
            when(mKeyedService.getTask(taskId)).thenReturn(task);

            mNotificationService.updateNotificationForTask(
                    taskId, state, /* isSilent= */ false, /* isWarning= */ false);
            Notification notification =
                    mNotificationService.getCachedNotification(
                            taskId, /* isSilent= */ false, /* isWarning= */ false);
            assertNotNull("Notification should not be null for state: " + state, notification);
            assertTrue(
                    "Initial terminal notification should be ongoing for state: " + state,
                    (notification.flags & Notification.FLAG_ONGOING_EVENT) != 0);
            assertTrue(
                    "Initial terminal notification should request promoted ongoing for state: "
                            + state,
                    notification.extras.getBoolean(
                            ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
            assertTrue(mNotificationService.hasPendingDemotionForTesting(taskId));

            // Run delayed tasks to fire demotion runnable.
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
            assertFalse(mNotificationService.hasPendingDemotionForTesting(taskId));

            Notification demotedNotif =
                    mNotificationService.getCachedNotification(
                            taskId, /* isSilent= */ false, /* isWarning= */ false);
            assertNotNull(demotedNotif);
            assertFalse(
                    "Demoted terminal notification should NOT be ongoing for state: " + state,
                    (demotedNotif.flags & Notification.FLAG_ONGOING_EVENT) != 0);
            assertFalse(
                    "Demoted terminal notification should not request promoted ongoing for state: "
                            + state,
                    demotedNotif.extras.getBoolean(
                            ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
            assertNull(
                    (Object)
                            demotedNotif.extras.getCharSequence(
                                    ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));
        }
    }

    @Test
    public void testFinishedNotificationDemotedAfterDelay() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.FINISHED);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);
        when(mServiceController.createTrustedBringTabToFrontIntent(any())).thenReturn(new Intent());

        // Post finished notification.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.FINISHED, /* isSilent= */ false, /* isWarning= */ false);
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());
        assertTrue(mNotificationService.hasPendingDemotionForTesting(taskId));

        Notification liveNotif =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(liveNotif);
        assertTrue(
                "Initial finished notification should be ongoing",
                (liveNotif.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertTrue(
                "Initial finished notification should request promoted ongoing",
                liveNotif.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertEquals(
                "Initial finished notification chip should be Done",
                mContext.getString(R.string.actor_notification_live_status_done),
                liveNotif.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));

        // Advance looper to fire demotion runnable.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertFalse(mNotificationService.hasPendingDemotionForTesting(taskId));
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());

        Notification demotedNotif =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(demotedNotif);
        assertFalse(
                "Demoted notification should not be ongoing",
                (demotedNotif.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertFalse(
                "Demoted notification should not request promoted ongoing",
                demotedNotif.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertNull(
                (Object)
                        demotedNotif.extras.getCharSequence(
                                ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));
        assertTrue(
                "Demoted notification should have auto-cancel enabled",
                (demotedNotif.flags & Notification.FLAG_AUTO_CANCEL) != 0);
    }

    @Test
    public void testStoppedNotificationDemotedAfterDelay() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.FAILED);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);
        when(mServiceController.createTrustedBringTabToFrontIntent(any())).thenReturn(new Intent());

        // Post stopped notification.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.FAILED, /* isSilent= */ false, /* isWarning= */ false);
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());
        assertTrue(mNotificationService.hasPendingDemotionForTesting(taskId));

        Notification liveNotif =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(liveNotif);
        assertTrue(
                "Initial stopped notification should be ongoing",
                (liveNotif.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertTrue(
                "Initial stopped notification should request promoted ongoing",
                liveNotif.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertEquals(
                "Initial stopped notification chip should be Stopped",
                mContext.getString(R.string.actor_notification_live_status_stopped),
                liveNotif.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));

        // Advance looper to fire demotion runnable.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertFalse(mNotificationService.hasPendingDemotionForTesting(taskId));
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());

        Notification demotedNotif =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(demotedNotif);
        assertFalse(
                "Demoted stopped notification should not be ongoing",
                (demotedNotif.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertFalse(
                "Demoted stopped notification should not request promoted ongoing",
                demotedNotif.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertNull(
                (Object)
                        demotedNotif.extras.getCharSequence(
                                ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));
        assertTrue(
                "Demoted stopped notification should have auto-cancel enabled",
                (demotedNotif.flags & Notification.FLAG_AUTO_CANCEL) != 0);
    }

    @Test
    public void testCancelNotification_CancelsPendingDemotion() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.FINISHED);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.FINISHED, /* isSilent= */ false, /* isWarning= */ false);
        assertTrue(mNotificationService.hasPendingDemotionForTesting(taskId));

        mNotificationService.clearAll();
        assertFalse(mNotificationService.hasPendingDemotionForTesting(taskId));

        // Advancing the looper should not post any notification.
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(0, mMockNotificationManager.getMutationCountAndDecrement());
    }

    @Test
    public void testDemoteToNonLiveNotification_WhenAlreadyCleared_DoesNothing() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.FINISHED);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.FINISHED, /* isSilent= */ false, /* isWarning= */ false);
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());

        mNotificationService.clearAll();

        // Direct invocation after clearAll should safely do nothing.
        mNotificationService.demoteToNonLiveNotification(taskId);
        assertEquals(0, mMockNotificationManager.getMutationCountAndDecrement());
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

    @Test
    public void testResendWorkingNotificationLoudly_RunningTask_PostsLoudNotification() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        // Initial silent notification while in foreground.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.ACTING, /* isSilent= */ true, /* isWarning= */ false);
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());
        NotificationWrapper silentWrapper =
                mNotificationService.getCachedNotificationWrapperForTesting(taskId);
        assertNotNull(silentWrapper);
        assertTrue(silentWrapper.isSilent());

        // Resend notification loudly (e.g. user leaves Chrome to background or enters PiP).
        mNotificationService.resendWorkingNotificationLoudly(taskId);
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());
        NotificationWrapper loudWrapper =
                mNotificationService.getCachedNotificationWrapperForTesting(taskId);
        assertNotNull(loudWrapper);
        assertFalse(loudWrapper.isSilent());
    }

    @Test
    public void testResendWorkingNotificationLoudly_PausedTask_DoesNotNotify() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.PAUSED_BY_USER);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        // Initial silent notification while in foreground for paused task.
        mNotificationService.updateNotificationForTask(
                taskId,
                ActorTaskState.PAUSED_BY_USER,
                /* isSilent= */ true,
                /* isWarning= */ false);
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());

        // Attempting to resend working notification loudly for paused task does not notify.
        mNotificationService.resendWorkingNotificationLoudly(taskId);
        assertEquals(0, mMockNotificationManager.getMutationCountAndDecrement());
    }
}

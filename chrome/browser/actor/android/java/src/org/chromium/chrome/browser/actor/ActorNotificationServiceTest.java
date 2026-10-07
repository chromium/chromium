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
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.actor.ui.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationIntentInterceptor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link ActorNotificationService}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowSystemClock.class})
@EnableFeatures(ChromeFeatureList.ACTOR_LIVE_NOTIFICATION)
public class ActorNotificationServiceTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActorKeyedService mKeyedService;
    @Mock private ActorTask mTask;
    @Mock private ActorTask.Natives mActorTaskJni;
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

        ActorForegroundServiceManager fgsManager = mock(ActorForegroundServiceManager.class);
        doAnswer(
                        invocation -> {
                            int taskId = invocation.getArgument(0);
                            mNotificationService.clearTaskData(taskId);
                            return null;
                        })
                .when(fgsManager)
                .onNotificationDismissed(anyInt());
        ActorForegroundServiceManager.setInstanceForTesting(fgsManager);
    }

    @After
    public void tearDown() {
        mNotificationService.clearAll();
        ActorForegroundServiceManager.resetInstanceForTesting();
        ActorTaskJni.setInstanceForTesting(null);
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

    @Test
    public void testMaybeDismissNotificationFromIntent_CompletedTask_DismissesNotification() {
        int taskId = 101;
        int state = ActorTaskState.FINISHED;

        ActorTask mockTask = mock(ActorTask.class);
        when(mockTask.getState()).thenReturn(state);
        when(mKeyedService.getTask(taskId)).thenReturn(mockTask);

        Profile mockProfile = mock(Profile.class);
        Profile mockOriginalProfile = mock(Profile.class);
        when(mockProfile.getOriginalProfile()).thenReturn(mockOriginalProfile);
        ActorKeyedServiceFactory.setForTesting(mKeyedService);

        Intent intent = new Intent();
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, state);

        mMockNotificationManager.notify(taskId, new Notification());
        assertEquals(1, mMockNotificationManager.getNotifications().size());

        ActorNotificationService.maybeDismissNotificationFromIntent(intent, mockProfile);

        assertTrue(
                "Completed task notification should be cancelled on intent receipt",
                mMockNotificationManager.getNotifications().isEmpty());
    }

    @Test
    public void testMaybeDismissNotificationFromIntent_FallbackToIntentState() {
        int taskId = 102;
        int state = ActorTaskState.FAILED;

        // Service does not have the task in memory, uses intent state.
        when(mKeyedService.getTask(taskId)).thenReturn(null);

        Intent intent = new Intent();
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, state);

        mMockNotificationManager.notify(taskId, new Notification());
        assertEquals(1, mMockNotificationManager.getNotifications().size());

        ActorNotificationService.maybeDismissNotificationFromIntent(intent, null);

        assertTrue(
                "Stopped task notification should be cancelled on intent receipt",
                mMockNotificationManager.getNotifications().isEmpty());
    }

    @Test
    public void testMaybeDismissNotificationFromIntent_ActiveTask_DoesNotDismissNotification() {
        int taskId = 103;
        int state = ActorTaskState.ACTING;

        ActorTask mockTask = mock(ActorTask.class);
        when(mockTask.getState()).thenReturn(state);
        when(mKeyedService.getTask(taskId)).thenReturn(mockTask);

        Profile mockProfile = mock(Profile.class);
        Profile mockOriginalProfile = mock(Profile.class);
        when(mockProfile.getOriginalProfile()).thenReturn(mockOriginalProfile);
        ActorKeyedServiceFactory.setForTesting(mKeyedService);

        Intent intent = new Intent();
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, state);

        mMockNotificationManager.notify(taskId, new Notification());
        assertEquals(1, mMockNotificationManager.getNotifications().size());

        ActorNotificationService.maybeDismissNotificationFromIntent(intent, mockProfile);

        assertEquals(
                "Active task notification should not be cancelled on intent receipt",
                1,
                mMockNotificationManager.getNotifications().size());
    }

    @Test
    public void testMaybeDismissNotificationFromIntent_CancelsPendingDemoteRunnable() {
        int taskId = 104;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Finished Task");
        when(mTask.getState()).thenReturn(ActorTaskState.FINISHED);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.FINISHED, /* isSilent= */ false, /* isWarning= */ false);

        assertTrue(mNotificationService.hasPendingDemotionForTesting(taskId));
        assertEquals(1, mMockNotificationManager.getNotifications().size());

        Intent intent = new Intent();
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, ActorTaskState.FINISHED);

        ActorNotificationService.maybeDismissNotificationFromIntent(intent, null);

        assertTrue(mMockNotificationManager.getNotifications().isEmpty());
        assertFalse(mNotificationService.hasPendingDemotionForTesting(taskId));

        // Run delayed tasks; demoteRunnable was cancelled so it should not execute or re-post.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertTrue(
                "Demotion runnable should not re-post notification after dismissal",
                mMockNotificationManager.getNotifications().isEmpty());
    }

    @Test
    public void testIndividualTaskDemotion_DemotesBasedOnPerNotificationTimer() {
        int taskId1 = 1;
        int taskId2 = 2;
        ActorTask task1 = mock(ActorTask.class);
        when(task1.getId()).thenReturn(taskId1);
        when(task1.getTitle()).thenReturn("Task 1");
        when(task1.getState()).thenReturn(ActorTaskState.FINISHED);
        when(mKeyedService.getTask(taskId1)).thenReturn(task1);

        ActorTask task2 = mock(ActorTask.class);
        when(task2.getId()).thenReturn(taskId2);
        when(task2.getTitle()).thenReturn("Task 2");
        when(task2.getState()).thenReturn(ActorTaskState.FAILED);
        when(mKeyedService.getTask(taskId2)).thenReturn(task2);

        // Task 1 finishes at t=0.
        mNotificationService.updateNotificationForTask(
                taskId1, ActorTaskState.FINISHED, /* isSilent= */ false, /* isWarning= */ false);

        // Advance by 20s.
        ShadowLooper.idleMainLooper(20, TimeUnit.SECONDS);

        // Task 2 finishes at t=20.
        mNotificationService.updateNotificationForTask(
                taskId2, ActorTaskState.FAILED, /* isSilent= */ false, /* isWarning= */ false);

        assertTrue(mNotificationService.hasPendingDemotions());
        assertTrue(mNotificationService.hasPendingDemotionForTesting(taskId1));
        assertTrue(mNotificationService.hasPendingDemotionForTesting(taskId2));

        // Advance by 10s (now t=30s from task 1, 10s from task 2).
        ShadowLooper.idleMainLooper(10, TimeUnit.SECONDS);

        // Task 1 should be demoted, Task 2 should still be pending demotion.
        assertFalse(mNotificationService.hasPendingDemotionForTesting(taskId1));
        assertTrue(mNotificationService.hasPendingDemotionForTesting(taskId2));
        assertTrue(mNotificationService.hasPendingDemotions());

        Notification demoted1 =
                mNotificationService.getCachedNotification(
                        taskId1, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(demoted1);
        assertFalse((demoted1.flags & Notification.FLAG_ONGOING_EVENT) != 0);

        Notification stillLive2 =
                mNotificationService.getCachedNotification(
                        taskId2, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(stillLive2);
        assertTrue((stillLive2.flags & Notification.FLAG_ONGOING_EVENT) != 0);

        // Advance by another 20s (now t=50s from task 1, 30s from task 2).
        ShadowLooper.idleMainLooper(20, TimeUnit.SECONDS);

        // Now Task 2 should also be demoted.
        assertFalse(mNotificationService.hasPendingDemotionForTesting(taskId2));
        assertFalse(mNotificationService.hasPendingDemotions());

        Notification demoted2 =
                mNotificationService.getCachedNotification(
                        taskId2, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(demoted2);
        assertFalse((demoted2.flags & Notification.FLAG_ONGOING_EVENT) != 0);
    }

    @Test
    public void testDemoteToNonLiveNotification_CallsMaybeStopServiceNowBeforeNotify() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.FINISHED);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        ActorForegroundServiceManager fgsManager = mock(ActorForegroundServiceManager.class);
        ActorForegroundServiceManager.setInstanceForTesting(fgsManager);

        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.FINISHED, /* isSilent= */ false, /* isWarning= */ false);
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());

        AtomicBoolean notifiedDuringMaybeStop = new AtomicBoolean(false);
        doAnswer(
                        invocation -> {
                            // At the moment maybeStopServiceNow is called, nonLiveWrapper
                            // should NOT yet be notified.
                            notifiedDuringMaybeStop.set(
                                    mMockNotificationManager
                                            .getNotifications()
                                            .get(0)
                                            .notification
                                            .extras
                                            .getBoolean(
                                                    ActorNotificationFactory
                                                            .EXTRA_REQUEST_PROMOTED_ONGOING,
                                                    false));
                            return null;
                        })
                .when(fgsManager)
                .maybeStopServiceNow();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(fgsManager).maybeStopServiceNow();
        assertTrue(
                "When maybeStopServiceNow was called, previous ongoing notification should"
                        + " still be in manager",
                notifiedDuringMaybeStop.get());
        Notification demoted =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(demoted);
        assertFalse(
                "After demotion, notification should not request promoted ongoing",
                demoted.extras.getBoolean(ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
    }

    @Test
    public void testDemoteNow_DoesNotCallMaybeStopServiceNow() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.FINISHED);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        ActorForegroundServiceManager fgsManager = mock(ActorForegroundServiceManager.class);
        ActorForegroundServiceManager.setInstanceForTesting(fgsManager);

        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.FINISHED, /* isSilent= */ false, /* isWarning= */ false);
        assertTrue(mNotificationService.hasPendingDemotionForTesting(taskId));

        mNotificationService.demoteNow(taskId);

        assertFalse(mNotificationService.hasPendingDemotionForTesting(taskId));
        verify(fgsManager, never()).maybeStopServiceNow();
        Notification demoted =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(demoted);
        assertFalse(
                "After demoteNow, notification should not request promoted ongoing",
                demoted.extras.getBoolean(ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
    }

    @Test
    public void testMaybeDismissNotificationFromIntent_CallsServiceBeforeCancel() {
        int taskId = 105;
        ActorForegroundServiceManager mockManager = mock(ActorForegroundServiceManager.class);
        ActorForegroundServiceManager.setInstanceForTesting(mockManager);

        MockNotificationManagerProxy spyNotificationManager = spy(mMockNotificationManager);
        BaseNotificationManagerProxyFactory.setInstanceForTesting(spyNotificationManager);

        Intent intent = new Intent();
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, taskId);
        intent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, ActorTaskState.FINISHED);

        ActorNotificationService.maybeDismissNotificationFromIntent(intent, null);

        InOrder inOrder = inOrder(mockManager, spyNotificationManager);
        inOrder.verify(mockManager).onNotificationDismissed(taskId);
        inOrder.verify(spyNotificationManager).cancel(taskId);
    }

    /**
     * Once a task completes, its native counterpart is destroyed and {@link ActorTask#getState()}
     * can no longer read a live state. The service must fall back to the last state it observed
     * rather than whatever the orphaned task reports, otherwise a completed task looks like it is
     * running again and the live notification never gets demoted.
     */
    @Test
    public void testDemotion_TaskStateUnreadableAfterCompletion_StillDemotes() {
        int taskId = 42;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.FINISHED);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.FINISHED, /* isSilent= */ false, /* isWarning= */ false);
        assertTrue(mNotificationService.hasPendingDemotionForTesting(taskId));

        // Native task is torn down; an orphaned ActorTask used to report CREATED, which
        // ActorUtils#isRunningState treats as still running.
        when(mTask.getState()).thenReturn(ActorTaskState.CREATED);

        // Re-pinning the foreground service reads the state back. This must not resurrect the
        // task as running.
        mNotificationService.getCachedNotification(
                taskId, /* isSilent= */ false, /* isWarning= */ false);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertFalse(mNotificationService.hasPendingDemotionForTesting(taskId));

        Notification demoted =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(demoted);
        assertFalse(
                "Completed task should not be rebuilt as an ongoing notification",
                (demoted.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertFalse(
                "Completed task should not be re-promoted as a live notification",
                demoted.extras.getBoolean(ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
    }

    /**
     * With more than one task in flight, the finished task's state is never re-read while its
     * native object is still alive, because the update queue reads whichever task is still active.
     * An orphaned ActorTask is therefore stuck reporting whatever it last saw - a running state -
     * so the service has to rely on the state it observed rather than asking the task.
     */
    @Test
    public void testDemotion_SecondTaskStillRunning_StillDemotesFinishedTask() {
        int finishedTaskId = 45;
        int runningTaskId = 46;
        ActorTask finishedTask = mock(ActorTask.class);
        when(finishedTask.getId()).thenReturn(finishedTaskId);
        when(finishedTask.getTitle()).thenReturn("Finished Task");
        when(finishedTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mKeyedService.getTask(finishedTaskId)).thenReturn(finishedTask);

        ActorTask runningTask = mock(ActorTask.class);
        when(runningTask.getId()).thenReturn(runningTaskId);
        when(runningTask.getTitle()).thenReturn("Running Task");
        when(runningTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mKeyedService.getTask(runningTaskId)).thenReturn(runningTask);

        // Both tasks are running and both notifications are live.
        mNotificationService.updateNotificationForTask(
                finishedTaskId,
                ActorTaskState.ACTING,
                /* isSilent= */ false,
                /* isWarning= */ false);
        mNotificationService.updateNotificationForTask(
                runningTaskId,
                ActorTaskState.ACTING,
                /* isSilent= */ false,
                /* isWarning= */ false);

        // The first task finishes. The second keeps running, so nothing re-reads the first task's
        // state before its native object goes away, leaving it frozen at ACTING.
        mNotificationService.updateNotificationForTask(
                finishedTaskId,
                ActorTaskState.FINISHED,
                /* isSilent= */ false,
                /* isWarning= */ false);
        assertTrue(mNotificationService.hasPendingDemotionForTesting(finishedTaskId));

        // Re-pinning the foreground service reads the finished task back.
        mNotificationService.getCachedNotification(
                finishedTaskId, /* isSilent= */ false, /* isWarning= */ false);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        Notification demoted =
                mNotificationService.getCachedNotification(
                        finishedTaskId, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(demoted);
        assertFalse(
                "A finished task must still demote while another task is running",
                (demoted.flags & Notification.FLAG_ONGOING_EVENT) != 0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ACTOR_STEP_PROGRESS_NOTIFICATION)
    public void testStepProgress_StateUnreadableAfterCompletion_DoesNotRebuild() {
        int taskId = 44;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.FINISHED);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.FINISHED, /* isSilent= */ false, /* isWarning= */ false);
        mMockNotificationManager.getMutationCountAndDecrement();

        when(mTask.getState()).thenReturn(ActorTaskState.CREATED);
        mNotificationService.updateNotificationForStepProgress(taskId);

        assertEquals(
                "A late step progress update must not revive a finished task's notification",
                0,
                mMockNotificationManager.getMutationCountAndDecrement());
    }

    @Test
    public void testCreatedToActingTransition_RebuildsNotification() {
        int taskId = 55;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);
        when(mServiceController.createTrustedBringTabToFrontIntent(mTask)).thenReturn(new Intent());

        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.CREATED, /* isSilent= */ false, /* isWarning= */ false);
        // Each buildNotification calls createTrustedBringTabToFrontIntent twice (content + action).
        verify(mServiceController, org.mockito.Mockito.times(2))
                .createTrustedBringTabToFrontIntent(mTask);

        // Transitioning from CREATED to ACTING must rebuild the notification so its PendingIntent
        // extras pick up the newly provisioned target tab ID and Glic conversation ID.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ false);
        verify(mServiceController, org.mockito.Mockito.times(4))
                .createTrustedBringTabToFrontIntent(mTask);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ACTOR_STEP_PROGRESS_NOTIFICATION)
    public void testTimeBetweenWorklogUpdates_SingleTask() {
        int taskId = 100;
        String histogram = ActorMetrics.ACTOR_NOTIFICATION_TIME_BETWEEN_WORKLOG_UPDATES;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ false);

        // First worklog update: establishes baseline, no period recorded.
        when(mTask.getCurrentActionName()).thenReturn("Step 1");
        var watcher1 = HistogramWatcher.newBuilder().expectNoRecords(histogram).build();
        mNotificationService.updateNotificationForStepProgress(taskId);
        watcher1.assertExpected();

        // Second worklog update after 1500ms.
        ShadowSystemClock.advanceBy(1500, TimeUnit.MILLISECONDS);
        when(mTask.getCurrentActionName()).thenReturn("Step 2");
        var watcher2 = HistogramWatcher.newSingleRecordWatcher(histogram, 1500);
        mNotificationService.updateNotificationForStepProgress(taskId);
        watcher2.assertExpected();

        // Third worklog update after 2500ms.
        ShadowSystemClock.advanceBy(2500, TimeUnit.MILLISECONDS);
        when(mTask.getCurrentActionName()).thenReturn("Step 3");
        var watcher3 = HistogramWatcher.newSingleRecordWatcher(histogram, 2500);
        mNotificationService.updateNotificationForStepProgress(taskId);
        watcher3.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ACTOR_STEP_PROGRESS_NOTIFICATION)
    public void testTimeBetweenWorklogUpdates_MultipleTasks_Independent() {
        int taskId1 = 1;
        int taskId2 = 2;
        String histogram = ActorMetrics.ACTOR_NOTIFICATION_TIME_BETWEEN_WORKLOG_UPDATES;

        ActorTask task1 = mock(ActorTask.class);
        when(task1.getId()).thenReturn(taskId1);
        when(task1.getTitle()).thenReturn("Task 1");
        when(task1.getState()).thenReturn(ActorTaskState.ACTING);
        when(mKeyedService.getTask(taskId1)).thenReturn(task1);

        ActorTask task2 = mock(ActorTask.class);
        when(task2.getId()).thenReturn(taskId2);
        when(task2.getTitle()).thenReturn("Task 2");
        when(task2.getState()).thenReturn(ActorTaskState.ACTING);
        when(mKeyedService.getTask(taskId2)).thenReturn(task2);

        mNotificationService.updateNotificationForTask(
                taskId1, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ false);
        mNotificationService.updateNotificationForTask(
                taskId2, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ false);

        // Task 1 initial update at t=0.
        when(task1.getCurrentActionName()).thenReturn("Task 1 Step 1");
        mNotificationService.updateNotificationForStepProgress(taskId1);

        // Advance 500ms -> t=500.
        ShadowSystemClock.advanceBy(500, TimeUnit.MILLISECONDS);
        // Task 2 initial update at t=500.
        when(task2.getCurrentActionName()).thenReturn("Task 2 Step 1");
        mNotificationService.updateNotificationForStepProgress(taskId2);

        // Advance 1000ms -> t=1500.
        ShadowSystemClock.advanceBy(1000, TimeUnit.MILLISECONDS);
        // Task 1 second update: interval = 1500 - 0 = 1500ms.
        when(task1.getCurrentActionName()).thenReturn("Task 1 Step 2");
        var watcher1 = HistogramWatcher.newSingleRecordWatcher(histogram, 1500);
        mNotificationService.updateNotificationForStepProgress(taskId1);
        watcher1.assertExpected();

        // Advance 1200ms -> t=2700.
        ShadowSystemClock.advanceBy(1200, TimeUnit.MILLISECONDS);
        // Task 2 second update: interval = 2700 - 500 = 2200ms.
        when(task2.getCurrentActionName()).thenReturn("Task 2 Step 2");
        var watcher2 = HistogramWatcher.newSingleRecordWatcher(histogram, 2200);
        mNotificationService.updateNotificationForStepProgress(taskId2);
        watcher2.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ACTOR_STEP_PROGRESS_NOTIFICATION)
    public void testTimeBetweenWorklogUpdates_TaskCompletion_ResetsTracking() {
        int taskId = 42;
        String histogram = ActorMetrics.ACTOR_NOTIFICATION_TIME_BETWEEN_WORKLOG_UPDATES;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ false);
        when(mTask.getCurrentActionName()).thenReturn("Step 1");
        mNotificationService.updateNotificationForStepProgress(taskId);

        ShadowSystemClock.advanceBy(1000, TimeUnit.MILLISECONDS);
        when(mTask.getCurrentActionName()).thenReturn("Step 2");
        var watcher1 = HistogramWatcher.newSingleRecordWatcher(histogram, 1000);
        mNotificationService.updateNotificationForStepProgress(taskId);
        watcher1.assertExpected();

        // Task completes: resets tracking for this task ID.
        when(mTask.getState()).thenReturn(ActorTaskState.FINISHED);
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.FINISHED, /* isSilent= */ false, /* isWarning= */ false);

        // Advance clock and simulate task restarting or a new task with the same ID.
        ShadowSystemClock.advanceBy(5000, TimeUnit.MILLISECONDS);
        when(mTask.getState()).thenReturn(ActorTaskState.ACTING);
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ false);

        // First update on the new task should establish a new baseline (no records emitted).
        when(mTask.getCurrentActionName()).thenReturn("New Task Step 1");
        var watcher2 = HistogramWatcher.newBuilder().expectNoRecords(histogram).build();
        mNotificationService.updateNotificationForStepProgress(taskId);
        watcher2.assertExpected();

        // Subsequent update should record interval relative to the new task baseline.
        ShadowSystemClock.advanceBy(800, TimeUnit.MILLISECONDS);
        when(mTask.getCurrentActionName()).thenReturn("New Task Step 2");
        var watcher3 = HistogramWatcher.newSingleRecordWatcher(histogram, 800);
        mNotificationService.updateNotificationForStepProgress(taskId);
        watcher3.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ACTOR_STEP_PROGRESS_NOTIFICATION)
    public void testTimeBetweenWorklogUpdates_Pause_ResetsTracking() {
        int taskId = 55;
        String histogram = ActorMetrics.ACTOR_NOTIFICATION_TIME_BETWEEN_WORKLOG_UPDATES;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ false);
        when(mTask.getCurrentActionName()).thenReturn("Step 1");
        mNotificationService.updateNotificationForStepProgress(taskId);

        ShadowSystemClock.advanceBy(1200, TimeUnit.MILLISECONDS);
        when(mTask.getCurrentActionName()).thenReturn("Step 2");
        var watcher1 = HistogramWatcher.newSingleRecordWatcher(histogram, 1200);
        mNotificationService.updateNotificationForStepProgress(taskId);
        watcher1.assertExpected();

        // User pauses the task.
        when(mTask.getState()).thenReturn(ActorTaskState.PAUSED_BY_USER);
        mNotificationService.updateNotificationForTask(
                taskId,
                ActorTaskState.PAUSED_BY_USER,
                /* isSilent= */ false,
                /* isWarning= */ false);
        ShadowSystemClock.advanceBy(10000, TimeUnit.MILLISECONDS);

        // Task resumes acting.
        when(mTask.getState()).thenReturn(ActorTaskState.ACTING);
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ false);

        // First worklog after resuming should establish a new baseline (no record).
        when(mTask.getCurrentActionName()).thenReturn("Step 3");
        var watcher2 = HistogramWatcher.newBuilder().expectNoRecords(histogram).build();
        mNotificationService.updateNotificationForStepProgress(taskId);
        watcher2.assertExpected();

        // Subsequent update records interval from resume baseline.
        ShadowSystemClock.advanceBy(900, TimeUnit.MILLISECONDS);
        when(mTask.getCurrentActionName()).thenReturn("Step 4");
        var watcher3 = HistogramWatcher.newSingleRecordWatcher(histogram, 900);
        mNotificationService.updateNotificationForStepProgress(taskId);
        watcher3.assertExpected();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ACTOR_STEP_PROGRESS_NOTIFICATION)
    public void testTimeBetweenWorklogUpdates_FeatureDisabled_DoesNotRecord() {
        int taskId = 66;
        String histogram = ActorMetrics.ACTOR_NOTIFICATION_TIME_BETWEEN_WORKLOG_UPDATES;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ false);

        var watcher = HistogramWatcher.newBuilder().expectNoRecords(histogram).build();
        mNotificationService.updateNotificationForStepProgress(taskId);
        ShadowSystemClock.advanceBy(1500, TimeUnit.MILLISECONDS);
        mNotificationService.updateNotificationForStepProgress(taskId);
        watcher.assertExpected();
    }

    @Test
    public void testDemotion_AfterClearNativePtr_PreservesTabIdOnPendingIntent() {
        long nativeTaskPtr = 1234L;
        int taskId = 50;
        int expectedTabId = 99;
        ActorTaskJni.setInstanceForTesting(mActorTaskJni);
        when(mActorTaskJni.getState(nativeTaskPtr)).thenReturn(ActorTaskState.FINISHED);
        when(mActorTaskJni.getLastActuatedTabId(nativeTaskPtr)).thenReturn(expectedTabId);

        ActorTask realTask =
                new ActorTask(
                        nativeTaskPtr, taskId, "Test Task", mock(Profile.class), "conv-id-50");
        when(mKeyedService.getTask(taskId)).thenReturn(realTask);
        when(mServiceController.createTrustedBringTabToFrontIntent(any()))
                .thenAnswer(
                        invocation -> {
                            ActorTask task = invocation.getArgument(0);
                            Intent intent = new Intent("BRING_TAB_TO_FRONT_ACTION");
                            intent.putExtra(
                                    IntentHandler.BRING_TAB_TO_FRONT_EXTRA, task.getTargetTabId());
                            intent.putExtra(
                                    NotificationConstants.EXTRA_ACTOR_TASK_STATE, task.getState());
                            return intent;
                        });

        // Post terminal live notification while native task is still alive.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.FINISHED, /* isSilent= */ false, /* isWarning= */ false);
        assertTrue(mNotificationService.hasPendingDemotionForTesting(taskId));

        // Native ActorTask is destroyed on the next UI message-loop tick after task completion.
        realTask.clearNativePtr();
        when(mKeyedService.getTask(taskId)).thenReturn(null);

        assertEquals(expectedTabId, realTask.getLastActuatedTabId());
        assertEquals(expectedTabId, realTask.getTargetTabId());
        assertEquals(ActorTaskState.FINISHED, realTask.getState());

        // Advance looper to fire demotion runnable after native task destruction.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertFalse(mNotificationService.hasPendingDemotionForTesting(taskId));

        Notification demoted =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(demoted);
        assertNotNull(demoted.contentIntent);

        Intent demotedIntent = shadowOf(demoted.contentIntent).getSavedIntent();
        if (NotificationIntentInterceptor.INTENT_ACTION.equals(demotedIntent.getAction())) {
            PendingIntent wrappedPendingIntent =
                    NotificationIntentInterceptor.getPendingIntentForTesting(demotedIntent);
            demotedIntent = shadowOf(wrappedPendingIntent).getSavedIntent();
        }
        assertEquals(
                "Demoted notification PendingIntent must preserve the last actuated tab ID",
                expectedTabId,
                demotedIntent.getIntExtra(
                        IntentHandler.BRING_TAB_TO_FRONT_EXTRA, Tab.INVALID_TAB_ID));
        assertEquals(
                ActorTaskState.FINISHED,
                demotedIntent.getIntExtra(
                        NotificationConstants.EXTRA_ACTOR_TASK_STATE, ActorTaskState.CREATED));
    }

    @Test
    public void testDemotion_AfterClearNativePtr_PreservesFallbackTabIdOnPendingIntent() {
        long nativeTaskPtr = 1234L;
        int taskId = 51;
        int expectedFallbackTabId = 77;
        ActorTaskJni.setInstanceForTesting(mActorTaskJni);
        when(mActorTaskJni.getState(nativeTaskPtr)).thenReturn(ActorTaskState.FINISHED);
        when(mActorTaskJni.getLastActuatedTabId(nativeTaskPtr)).thenReturn(Tab.INVALID_TAB_ID);
        when(mActorTaskJni.getTabs(nativeTaskPtr)).thenReturn(new int[] {expectedFallbackTabId});

        ActorTask realTask =
                new ActorTask(
                        nativeTaskPtr, taskId, "Test Task", mock(Profile.class), "conv-id-51");
        when(mKeyedService.getTask(taskId)).thenReturn(realTask);
        when(mServiceController.createTrustedBringTabToFrontIntent(any()))
                .thenAnswer(
                        invocation -> {
                            ActorTask task = invocation.getArgument(0);
                            Intent intent = new Intent("BRING_TAB_TO_FRONT_ACTION");
                            intent.putExtra(
                                    IntentHandler.BRING_TAB_TO_FRONT_EXTRA, task.getTargetTabId());
                            intent.putExtra(
                                    NotificationConstants.EXTRA_ACTOR_TASK_STATE, task.getState());
                            return intent;
                        });

        // Post terminal live notification while native task is still alive.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.FINISHED, /* isSilent= */ false, /* isWarning= */ false);
        assertTrue(mNotificationService.hasPendingDemotionForTesting(taskId));

        // Native ActorTask is destroyed on the next UI message-loop tick after task completion.
        realTask.clearNativePtr();
        when(mKeyedService.getTask(taskId)).thenReturn(null);

        assertEquals(Tab.INVALID_TAB_ID, realTask.getLastActuatedTabId());
        assertEquals(expectedFallbackTabId, realTask.getTargetTabId());
        assertEquals(ActorTaskState.FINISHED, realTask.getState());

        // Advance looper to fire demotion runnable after native task destruction.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertFalse(mNotificationService.hasPendingDemotionForTesting(taskId));

        Notification demoted =
                mNotificationService.getCachedNotification(
                        taskId, /* isSilent= */ false, /* isWarning= */ false);
        assertNotNull(demoted);
        assertNotNull(demoted.contentIntent);

        Intent demotedIntent = shadowOf(demoted.contentIntent).getSavedIntent();
        if (NotificationIntentInterceptor.INTENT_ACTION.equals(demotedIntent.getAction())) {
            PendingIntent wrappedPendingIntent =
                    NotificationIntentInterceptor.getPendingIntentForTesting(demotedIntent);
            demotedIntent = shadowOf(wrappedPendingIntent).getSavedIntent();
        }
        assertEquals(
                "Demoted notification PendingIntent must preserve the fallback tab ID",
                expectedFallbackTabId,
                demotedIntent.getIntExtra(
                        IntentHandler.BRING_TAB_TO_FRONT_EXTRA, Tab.INVALID_TAB_ID));
        assertEquals(
                ActorTaskState.FINISHED,
                demotedIntent.getIntExtra(
                        NotificationConstants.EXTRA_ACTOR_TASK_STATE, ActorTaskState.CREATED));
    }
}

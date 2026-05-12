// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
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
import org.chromium.chrome.browser.actor.ui.R;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;

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
        assertNull(mNotificationService.getForegroundNotification(null, false));
    }

    @Test
    public void testGetForegroundNotification_TaskValid() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        Notification notification =
                mNotificationService.getForegroundNotification(mTask, /* isSilent= */ false);

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
                taskId, ActorTaskState.ACTING, /* isSilent= */ false);

        Notification notification =
                mNotificationService.getCachedNotification(taskId, /* isSilent= */ false);
        assertNotNull(notification);
        assertEquals(
                mContext.getString(R.string.actor_notification_title_working_on_task),
                notification.extras.getString(Notification.EXTRA_TITLE));
        // updateNotificationForTask should have notified.
        assertEquals(1, mMockNotificationManager.getNotifications().size());
    }

    @Test
    public void testUpdateNotificationForTask_TaskRemoved() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.ACTING, /* isSilent= */ false);
        assertEquals(1, mMockNotificationManager.getNotifications().size());

        // Task is removed from KeyedService
        when(mKeyedService.getTask(taskId)).thenReturn(null);

        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.FINISHED, /* isSilent= */ false);

        // Task won't be removed from notification cache.
        assertNotNull(mNotificationService.getCachedNotification(taskId, /* isSilent= */ false));
    }

    @Test
    public void testGetCachedNotification_TaskExists() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        Notification notification =
                mNotificationService.getCachedNotification(taskId, /* isSilent= */ false);

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
                mNotificationService.getCachedNotification(taskId, /* isSilent= */ false);

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
                taskId1, ActorTaskState.ACTING, /* isSilent= */ false);
        mNotificationService.updateNotificationForTask(
                taskId2, ActorTaskState.ACTING, /* isSilent= */ false);

        assertEquals(2, mMockNotificationManager.getNotifications().size());

        mNotificationService.clearAll();

        when(mKeyedService.getTask(taskId1)).thenReturn(null);
        when(mKeyedService.getTask(taskId2)).thenReturn(null);

        assertNull(mNotificationService.getCachedNotification(taskId1, /* isSilent= */ false));
        assertNull(mNotificationService.getCachedNotification(taskId2, /* isSilent= */ false));
    }

    @Test
    public void testUpdateNotificationForTask_SkipRedundantUpdates() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        // First update.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.ACTING, /* isSilent= */ false);
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());

        // Update to REFLECTING should be skipped.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.REFLECTING, /* isSilent= */ false);
        assertEquals(0, mMockNotificationManager.getMutationCountAndDecrement());

        // Update to PAUSED_BY_USER should NOT be skipped.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.PAUSED_BY_USER, /* isSilent= */ false);
        assertEquals(1, mMockNotificationManager.getMutationCountAndDecrement());

        // Update to PAUSED_BY_ACTOR should be skipped.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.PAUSED_BY_ACTOR, /* isSilent= */ false);
        assertEquals(0, mMockNotificationManager.getMutationCountAndDecrement());

        // Update with isSilent changed should be skipped because we only update on state changes.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.PAUSED_BY_ACTOR, /* isSilent= */ true);
        assertEquals(0, mMockNotificationManager.getMutationCountAndDecrement());
    }

    @Test
    public void testGetCachedNotification_UpdatesStateCache() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mTask.getState()).thenReturn(ActorTaskState.ACTING);
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        // This should populate the state cache.
        mNotificationService.getCachedNotification(taskId, /* isSilent= */ false);
        assertEquals(0, mMockNotificationManager.getMutationCountAndDecrement());

        // Now updateNotificationForTask with REFLECTING should be skipped.
        mNotificationService.updateNotificationForTask(
                taskId, ActorTaskState.REFLECTING, /* isSilent= */ false);
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
            mNotificationService.updateNotificationForTask(taskId, state, /* isSilent= */ false);
            Notification notification =
                    mNotificationService.getCachedNotification(taskId, /* isSilent= */ false);
            assertNotNull("Notification should not be null for state: " + state, notification);
            assertFalse(
                    "Notification should NOT be ongoing for state: " + state,
                    (notification.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        }
    }
}

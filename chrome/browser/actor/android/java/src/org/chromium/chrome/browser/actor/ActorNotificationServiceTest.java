// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import android.app.Notification;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;

/** Unit tests for {@link ActorNotificationService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActorNotificationServiceTest {

    @Mock private ActorKeyedService mKeyedService;
    @Mock private ActorTask mTask;

    private ActorNotificationService mNotificationService;
    private MockNotificationManagerProxy mMockNotificationManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMockNotificationManager = new MockNotificationManagerProxy();
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mMockNotificationManager);
        mNotificationService = new ActorNotificationService(mKeyedService);
    }

    @After
    public void tearDown() {
        mNotificationService.clearAll();
    }

    @Test
    public void testGetForegroundNotification_TaskNull() {
        assertNull(mNotificationService.getForegroundNotification(null));
    }

    @Test
    public void testGetForegroundNotification_TaskValid() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        Notification notification = mNotificationService.getForegroundNotification(mTask);

        assertNotNull(notification);
        assertEquals("Test Task", notification.extras.getString(Notification.EXTRA_TITLE));
    }

    @Test
    public void testUpdateNotificationForTask_TaskExists() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        mNotificationService.updateNotificationForTask(taskId, ActorTaskState.ACTING);

        Notification notification = mNotificationService.getCachedNotification(taskId);
        assertNotNull(notification);
        assertEquals("Test Task", notification.extras.getString(Notification.EXTRA_TITLE));
    }

    @Test
    public void testUpdateNotificationForTask_TaskRemoved() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        mNotificationService.updateNotificationForTask(taskId, ActorTaskState.ACTING);

        // Task is removed from KeyedService
        when(mKeyedService.getTask(taskId)).thenReturn(null);

        mNotificationService.updateNotificationForTask(taskId, ActorTaskState.FINISHED);

        assertNull(mNotificationService.getCachedNotification(taskId));
        assertEquals(0, mMockNotificationManager.getNotifications().size());
    }

    @Test
    public void testGetCachedNotification_TaskExists() {
        int taskId = 1;
        when(mTask.getId()).thenReturn(taskId);
        when(mTask.getTitle()).thenReturn("Test Task");
        when(mKeyedService.getTask(taskId)).thenReturn(mTask);

        Notification notification = mNotificationService.getCachedNotification(taskId);

        assertNotNull(notification);
        assertEquals("Test Task", notification.extras.getString(Notification.EXTRA_TITLE));
    }

    @Test
    public void testGetCachedNotification_TaskDoesNotExist() {
        int taskId = 1;
        when(mKeyedService.getTask(taskId)).thenReturn(null);

        Notification notification = mNotificationService.getCachedNotification(taskId);

        assertNull(notification);
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

        mNotificationService.updateNotificationForTask(taskId1, ActorTaskState.ACTING);
        mNotificationService.updateNotificationForTask(taskId2, ActorTaskState.ACTING);

        mNotificationService.clearAll();

        when(mKeyedService.getTask(taskId1)).thenReturn(null);
        when(mKeyedService.getTask(taskId2)).thenReturn(null);

        assertNull(mNotificationService.getCachedNotification(taskId1));
        assertNull(mNotificationService.getCachedNotification(taskId2));
        assertEquals(0, mMockNotificationManager.getNotifications().size());
    }
}

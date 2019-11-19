// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.os.Bundle;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.SysUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;

/**
 * Unit tests for NotificationTriggerBackgroundTask.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NotificationTriggerBackgroundTaskTest {
    @Mock
    private BackgroundTaskScheduler mTaskScheduler;
    @Mock
    private NotificationTriggerScheduler mTriggerScheduler;
    @Mock
    private BackgroundTask.TaskFinishedCallback mTaskFinishedCallback;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mTaskScheduler);
        NotificationTriggerScheduler.setInstanceForTests(mTriggerScheduler);
    }

    @After
    public void tearDown() {
        SysUtils.resetForTesting();
    }

    private static TaskParameters createTaskParameters(long timestamp) {
        Bundle extras = new Bundle();
        extras.putLong(NotificationTriggerBackgroundTask.KEY_TIMESTAMP, timestamp);
        return TaskParameters.create(TaskIds.NOTIFICATION_TRIGGER_JOB_ID).addExtras(extras).build();
    }

    @Test
    public void testScheduleInitializesOneOffTask() {
        long delay = 1000;
        long timestamp = System.currentTimeMillis() + delay;
        ArgumentCaptor<TaskInfo> taskInfoCaptor = ArgumentCaptor.forClass(TaskInfo.class);
        NotificationTriggerBackgroundTask.schedule(timestamp, delay);
        verify(mTaskScheduler).schedule(any(), taskInfoCaptor.capture());
        TaskInfo taskInfo = taskInfoCaptor.getValue();

        assertEquals(TaskIds.NOTIFICATION_TRIGGER_JOB_ID, taskInfo.getTaskId());
        assertEquals(NotificationTriggerBackgroundTask.class, taskInfo.getBackgroundTaskClass());
        assertTrue(taskInfo.isPersisted());
        assertFalse(taskInfo.isPeriodic());
        assertTrue(taskInfo.shouldUpdateCurrent());
        assertEquals(TaskInfo.NetworkType.NONE, taskInfo.getRequiredNetworkType());
        assertEquals(delay, taskInfo.getOneOffInfo().getWindowStartTimeMs());
        assertEquals(delay, taskInfo.getOneOffInfo().getWindowEndTimeMs());
        assertEquals(timestamp,
                taskInfo.getExtras().getLong(NotificationTriggerBackgroundTask.KEY_TIMESTAMP));
    }

    @Test
    public void testCancelCancelsTask() {
        NotificationTriggerBackgroundTask.cancel();
        verify(mTaskScheduler).cancel(any(), eq(TaskIds.NOTIFICATION_TRIGGER_JOB_ID));
    }

    @Test
    public void testRescheduleCallsScheduler() {
        new NotificationTriggerBackgroundTask().reschedule(RuntimeEnvironment.application);
        verify(mTriggerScheduler).reschedule();
    }

    @Test
    public void testStartBeforeNative_ValidTrigger() {
        long timestamp = System.currentTimeMillis() + 1000;
        doReturn(true).when(mTriggerScheduler).checkAndResetTrigger(eq(timestamp));

        int result = new NotificationTriggerBackgroundTask().onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, createTaskParameters(timestamp),
                mTaskFinishedCallback);

        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.LOAD_NATIVE, result);
        verify(mTriggerScheduler).checkAndResetTrigger(eq(timestamp));
        verify(mTaskFinishedCallback, never()).taskFinished(anyBoolean());
    }

    @Test
    public void testStartBeforeNative_InvalidTrigger() {
        long timestamp = System.currentTimeMillis() + 1000;
        doReturn(false).when(mTriggerScheduler).checkAndResetTrigger(eq(timestamp));

        int result = new NotificationTriggerBackgroundTask().onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, createTaskParameters(timestamp),
                mTaskFinishedCallback);

        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.DONE, result);
        verify(mTriggerScheduler).checkAndResetTrigger(eq(timestamp));
        verify(mTaskFinishedCallback, never()).taskFinished(anyBoolean());
    }

    @Test
    public void testStartWithNativeCallsTriggerNotifications() {
        long timestamp = System.currentTimeMillis() + 1000;
        new NotificationTriggerBackgroundTask().onStartTaskWithNative(
                RuntimeEnvironment.application, createTaskParameters(timestamp),
                mTaskFinishedCallback);

        verify(mTriggerScheduler).triggerNotifications();
        verify(mTaskFinishedCallback).taskFinished(eq(false));
    }

    @Test
    public void testReschedule_BeforeNative() {
        long timestamp = System.currentTimeMillis() + 1000;
        boolean shouldReschedule =
                new NotificationTriggerBackgroundTask().onStopTaskBeforeNativeLoaded(
                        RuntimeEnvironment.application, createTaskParameters(timestamp));
        assertTrue(shouldReschedule);
    }

    @Test
    public void testReschedule_WithNative() {
        long timestamp = System.currentTimeMillis() + 1000;
        boolean shouldReschedule = new NotificationTriggerBackgroundTask().onStopTaskWithNative(
                RuntimeEnvironment.application, createTaskParameters(timestamp));
        assertTrue(shouldReschedule);
    }

    @Test
    public void testReschedule_AfterTriggerBeforeNative() {
        long timestamp = System.currentTimeMillis() + 1000;

        NotificationTriggerBackgroundTask task = new NotificationTriggerBackgroundTask();
        TaskParameters params = createTaskParameters(timestamp);
        task.onStartTaskWithNative(RuntimeEnvironment.application, params, mTaskFinishedCallback);
        boolean shouldReschedule =
                task.onStopTaskBeforeNativeLoaded(RuntimeEnvironment.application, params);

        assertFalse(shouldReschedule);
    }

    @Test
    public void testReschedule_AfterTriggerWithNative() {
        long timestamp = System.currentTimeMillis() + 1000;

        NotificationTriggerBackgroundTask task = new NotificationTriggerBackgroundTask();
        TaskParameters params = createTaskParameters(timestamp);
        task.onStartTaskWithNative(RuntimeEnvironment.application, params, mTaskFinishedCallback);
        boolean shouldReschedule =
                task.onStopTaskWithNative(RuntimeEnvironment.application, params);

        assertFalse(shouldReschedule);
    }

    @Test
    public void testReschedule_ValidTriggerBeforeNative() {
        long timestamp = System.currentTimeMillis() + 1000;
        doReturn(true).when(mTriggerScheduler).checkAndResetTrigger(eq(timestamp));

        NotificationTriggerBackgroundTask task = new NotificationTriggerBackgroundTask();
        TaskParameters params = createTaskParameters(timestamp);
        task.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, mTaskFinishedCallback);
        boolean shouldReschedule =
                task.onStopTaskBeforeNativeLoaded(RuntimeEnvironment.application, params);

        assertTrue(shouldReschedule);
    }

    @Test
    public void testReschedule_ValidTriggerWithNative() {
        long timestamp = System.currentTimeMillis() + 1000;
        doReturn(true).when(mTriggerScheduler).checkAndResetTrigger(eq(timestamp));

        NotificationTriggerBackgroundTask task = new NotificationTriggerBackgroundTask();
        TaskParameters params = createTaskParameters(timestamp);
        task.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, mTaskFinishedCallback);
        boolean shouldReschedule =
                task.onStopTaskWithNative(RuntimeEnvironment.application, params);

        assertTrue(shouldReschedule);
    }

    @Test
    public void testReschedule_InvalidTriggerBeforeNative() {
        long timestamp = System.currentTimeMillis() + 1000;
        doReturn(false).when(mTriggerScheduler).checkAndResetTrigger(eq(timestamp));

        NotificationTriggerBackgroundTask task = new NotificationTriggerBackgroundTask();
        TaskParameters params = createTaskParameters(timestamp);
        task.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, mTaskFinishedCallback);
        boolean shouldReschedule =
                task.onStopTaskBeforeNativeLoaded(RuntimeEnvironment.application, params);

        assertFalse(shouldReschedule);
    }

    @Test
    public void testReschedule_InvalidTriggerWithNative() {
        long timestamp = System.currentTimeMillis() + 1000;
        doReturn(false).when(mTriggerScheduler).checkAndResetTrigger(eq(timestamp));

        NotificationTriggerBackgroundTask task = new NotificationTriggerBackgroundTask();
        TaskParameters params = createTaskParameters(timestamp);
        task.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, mTaskFinishedCallback);
        boolean shouldReschedule =
                task.onStopTaskWithNative(RuntimeEnvironment.application, params);

        assertFalse(shouldReschedule);
    }
}

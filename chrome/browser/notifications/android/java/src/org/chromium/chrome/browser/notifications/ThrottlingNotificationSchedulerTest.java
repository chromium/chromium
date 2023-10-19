// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.os.SystemClock;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.notifications.PendingNotificationTask;
import org.chromium.components.browser_ui.notifications.ThrottlingNotificationScheduler;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/** JUnit tests for the {@link ThrottlingNotificationScheduler} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSystemClock.class})
public class ThrottlingNotificationSchedulerTest {
    private static final long CURRENT_TIME_MS = 90000000L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Map<String, Integer> mTaskInvocationMap = new HashMap<>();

    @Before
    public void setUp() throws Exception {
        SystemClock.setCurrentTimeMillis(CURRENT_TIME_MS);
        ThrottlingNotificationScheduler.getInstance().clear();
    }

    private void incrementTaskInvokeCount(String taskId) {
        mTaskInvocationMap.putIfAbsent(taskId, 0);
        mTaskInvocationMap.put(taskId, mTaskInvocationMap.get(taskId) + 1);
    }

    private void addTask(String taskId, @PendingNotificationTask.Priority int priority) {
        ThrottlingNotificationScheduler.getInstance()
                .addPendingNotificationTask(
                        new PendingNotificationTask(
                                taskId,
                                priority,
                                () -> {
                                    incrementTaskInvokeCount(taskId);
                                }));
    }

    @Test
    public void testAddPendingNotificationWhileNoTaskPending() {
        addTask("t1", PendingNotificationTask.Priority.LOW);
        Assert.assertEquals(1, (int) mTaskInvocationMap.get("t1"));
    }

    @Test
    public void testSameTaskSquashed() {
        addTask("t1", PendingNotificationTask.Priority.LOW);
        Assert.assertEquals(1, (int) mTaskInvocationMap.get("t1"));
        addTask("t2", PendingNotificationTask.Priority.LOW);
        addTask("t2", PendingNotificationTask.Priority.LOW);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(1, (int) mTaskInvocationMap.get("t2"));
    }

    @Test
    public void testCancelPendingNotificationTask() {
        addTask("t1", PendingNotificationTask.Priority.LOW);
        Assert.assertEquals(1, (int) mTaskInvocationMap.get("t1"));
        addTask("t2", PendingNotificationTask.Priority.LOW);
        ThrottlingNotificationScheduler.getInstance().cancelPendingNotificationTask("t2");
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertFalse(mTaskInvocationMap.containsKey("t2"));
    }

    @Test
    public void testHighPriorityTaskRunsFirst() {
        addTask("t1", PendingNotificationTask.Priority.LOW);
        Assert.assertEquals(1, (int) mTaskInvocationMap.get("t1"));

        // A bunch of tasks arrives in order.
        addTask("t1", PendingNotificationTask.Priority.LOW);
        SystemClock.setCurrentTimeMillis(/* milliseconds= */ CURRENT_TIME_MS + 1);

        addTask("t2", PendingNotificationTask.Priority.HIGH);
        addTask("t3", PendingNotificationTask.Priority.LOW);

        // Idle for the correct amount of time to guarantee the task is run.
        ShadowLooper.idleMainLooper(
                ThrottlingNotificationScheduler.UPDATE_DELAY_MILLIS + 1, TimeUnit.MILLISECONDS);
        Assert.assertEquals(1, (int) mTaskInvocationMap.get("t2"));
        Assert.assertFalse(mTaskInvocationMap.containsKey("t3"));
        Assert.assertEquals(1, (int) mTaskInvocationMap.get("t1"));

        // Idle for the correct amount of time to guarantee the task is run.
        ShadowLooper.idleMainLooper(
                ThrottlingNotificationScheduler.UPDATE_DELAY_MILLIS + 1, TimeUnit.MILLISECONDS);

        Assert.assertEquals(2, (int) mTaskInvocationMap.get("t1"));
        Assert.assertFalse(mTaskInvocationMap.containsKey("t3"));

        // Idle for the correct amount of time to guarantee the task is run.
        ShadowLooper.idleMainLooper(
                ThrottlingNotificationScheduler.UPDATE_DELAY_MILLIS + 1, TimeUnit.MILLISECONDS);
        Assert.assertEquals(1, (int) mTaskInvocationMap.get("t3"));
    }

    @Test
    public void testNoThrottleIfNotificationsAreSpreadOut() {
        addTask("t1", PendingNotificationTask.Priority.LOW);
        Assert.assertEquals(1, (int) mTaskInvocationMap.get("t1"));

        // Idle for the correct amount of time to guarantee the task is run.
        ShadowLooper.idleMainLooper(
                ThrottlingNotificationScheduler.UPDATE_DELAY_MILLIS + 1, TimeUnit.MILLISECONDS);

        addTask("t1", PendingNotificationTask.Priority.LOW);
        Assert.assertEquals(2, (int) mTaskInvocationMap.get("t1"));

        // Idle for the correct amount of time to guarantee the task is run.
        ShadowLooper.idleMainLooper(
                ThrottlingNotificationScheduler.UPDATE_DELAY_MILLIS + 1, TimeUnit.MILLISECONDS);
        addTask("t1", PendingNotificationTask.Priority.LOW);
        Assert.assertEquals(3, (int) mTaskInvocationMap.get("t1"));
    }
}

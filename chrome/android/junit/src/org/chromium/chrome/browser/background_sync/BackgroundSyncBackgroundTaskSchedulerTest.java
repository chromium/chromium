// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_sync;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.test.DisableHistogramsRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.background_sync.BackgroundSyncBackgroundTaskScheduler.BackgroundSyncTask;
import org.chromium.chrome.browser.background_task_scheduler.ChromeBackgroundTaskFactory;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

import java.util.HashMap;
import java.util.concurrent.TimeUnit;

/** Unit tests for BackgroundSyncBackgroundTaskScheduler. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BackgroundSyncBackgroundTaskSchedulerTest {
    @Rule
    public DisableHistogramsRule mDisableHistogramsRule = new DisableHistogramsRule();

    @Mock
    private BackgroundTaskScheduler mTaskScheduler;
    @Captor
    ArgumentCaptor<TaskInfo> mTaskInfo;

    private static final int ONE_DAY_IN_MILLISECONDS = (int) TimeUnit.DAYS.toMillis(1);
    private static final int ONE_WEEK_IN_MILLISECONDS = (int) TimeUnit.DAYS.toMillis(7);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mTaskScheduler);
        ChromeBackgroundTaskFactory.setAsDefault();
        HashMap<String, Boolean> features = new HashMap<String, Boolean>();
        features.put(ChromeFeatureList.BACKGROUND_TASK_SCHEDULER_FOR_BACKGROUND_SYNC, true);
        ChromeFeatureList.setTestFeatures(features);
        doReturn(true)
                .when(mTaskScheduler)
                .schedule(eq(ContextUtils.getApplicationContext()), mTaskInfo.capture());
    }

    private void verifyFixedTaskInfoValues(
            TaskInfo info, @BackgroundSyncBackgroundTaskScheduler.BackgroundSyncTask int taskType) {
        int taskId = BackgroundSyncBackgroundTaskScheduler.getAppropriateTaskId(taskType);
        Class<? extends NativeBackgroundTask> taskClass =
                BackgroundSyncBackgroundTaskScheduler.getAppropriateTaskClass(taskType);

        assertEquals(taskId, info.getTaskId());
        assertEquals(taskClass, info.getBackgroundTaskClass());
        assertTrue(info.isPersisted());
        assertFalse(info.isPeriodic());
        assertTrue(info.getOneOffInfo().expiresAfterWindowEndTime());
        assertEquals(TaskInfo.NetworkType.ANY, info.getRequiredNetworkType());

        long expectedSoonestDelayTime = info.getExtras().getLong(
                BackgroundSyncBackgroundTaskScheduler.SOONEST_EXPECTED_WAKETIME);
        assertTrue(expectedSoonestDelayTime > 0L);
    }

    @Test
    @Feature({"BackgroundSync"})
    public void testScheduleOneOffTask() {
        BackgroundSyncBackgroundTaskScheduler.getInstance().scheduleOneOffTask(
                BackgroundSyncTask.ONE_SHOT_SYNC_CHROME_WAKE_UP,
                /* minDelayMs= */ ONE_DAY_IN_MILLISECONDS);
        verify(mTaskScheduler, times(1))
                .schedule(eq(ContextUtils.getApplicationContext()), eq(mTaskInfo.getValue()));

        TaskInfo taskInfo = mTaskInfo.getValue();
        verifyFixedTaskInfoValues(taskInfo,
                BackgroundSyncBackgroundTaskScheduler.BackgroundSyncTask
                        .ONE_SHOT_SYNC_CHROME_WAKE_UP);

        assertEquals(ONE_DAY_IN_MILLISECONDS, taskInfo.getOneOffInfo().getWindowStartTimeMs());
    }

    @Test
    @Feature({"BackgroundSync"})
    public void testSchedulePeriodicSyncWakeUpTask() {
        BackgroundSyncBackgroundTaskScheduler.getInstance().scheduleOneOffTask(
                BackgroundSyncTask.PERIODIC_SYNC_CHROME_WAKE_UP,
                /* minDelayMs= */ ONE_DAY_IN_MILLISECONDS);
        verify(mTaskScheduler, times(1))
                .schedule(eq(ContextUtils.getApplicationContext()), eq(mTaskInfo.getValue()));

        TaskInfo taskInfo = mTaskInfo.getValue();
        verifyFixedTaskInfoValues(taskInfo,
                BackgroundSyncBackgroundTaskScheduler.BackgroundSyncTask
                        .PERIODIC_SYNC_CHROME_WAKE_UP);

        assertEquals(ONE_DAY_IN_MILLISECONDS, taskInfo.getOneOffInfo().getWindowStartTimeMs());
    }

    @Test
    @Feature({"BackgroundSync"})
    public void testCancelOneShotTask() {
        BackgroundSyncBackgroundTaskScheduler.getInstance().scheduleOneOffTask(
                BackgroundSyncTask.ONE_SHOT_SYNC_CHROME_WAKE_UP,
                /* minDelayMs= */ ONE_DAY_IN_MILLISECONDS);
        verify(mTaskScheduler, times(1))
                .schedule(eq(ContextUtils.getApplicationContext()), eq(mTaskInfo.getValue()));

        doNothing()
                .when(mTaskScheduler)
                .cancel(eq(ContextUtils.getApplicationContext()),
                        eq(TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID));

        BackgroundSyncBackgroundTaskScheduler.getInstance().cancelOneOffTask(
                BackgroundSyncTask.ONE_SHOT_SYNC_CHROME_WAKE_UP);
        verify(mTaskScheduler, times(1))
                .cancel(eq(ContextUtils.getApplicationContext()),
                        eq(TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID));
    }

    @Test
    @Feature({"BackgroundSync"})
    public void testCancelPeriodicSyncWakeUpTask() {
        BackgroundSyncBackgroundTaskScheduler.getInstance().scheduleOneOffTask(
                BackgroundSyncTask.PERIODIC_SYNC_CHROME_WAKE_UP,
                /* minDelayMs= */ ONE_DAY_IN_MILLISECONDS);
        verify(mTaskScheduler, times(1))
                .schedule(eq(ContextUtils.getApplicationContext()), eq(mTaskInfo.getValue()));

        doNothing()
                .when(mTaskScheduler)
                .cancel(eq(ContextUtils.getApplicationContext()),
                        eq(TaskIds.PERIODIC_BACKGROUND_SYNC_CHROME_WAKEUP_TASK_JOB_ID));

        BackgroundSyncBackgroundTaskScheduler.getInstance().cancelOneOffTask(
                BackgroundSyncTask.PERIODIC_SYNC_CHROME_WAKE_UP);
        verify(mTaskScheduler, times(1))
                .cancel(eq(ContextUtils.getApplicationContext()),
                        eq(TaskIds.PERIODIC_BACKGROUND_SYNC_CHROME_WAKEUP_TASK_JOB_ID));
    }

    @Test
    @Feature({"BackgroundSync"})
    public void testScheduleOneOffTaskCalledTwice() {
        BackgroundSyncBackgroundTaskScheduler.getInstance().scheduleOneOffTask(
                BackgroundSyncTask.ONE_SHOT_SYNC_CHROME_WAKE_UP,
                /* minDelayMs= */ ONE_DAY_IN_MILLISECONDS);
        verify(mTaskScheduler, times(1))
                .schedule(eq(ContextUtils.getApplicationContext()), eq(mTaskInfo.getValue()));

        TaskInfo taskInfo = mTaskInfo.getValue();
        assertEquals(ONE_DAY_IN_MILLISECONDS, taskInfo.getOneOffInfo().getWindowStartTimeMs());

        BackgroundSyncBackgroundTaskScheduler.getInstance().scheduleOneOffTask(
                BackgroundSyncTask.ONE_SHOT_SYNC_CHROME_WAKE_UP,
                /* minDelayMs= */ ONE_WEEK_IN_MILLISECONDS);
        verify(mTaskScheduler, times(1))
                .schedule(eq(ContextUtils.getApplicationContext()), eq(mTaskInfo.getValue()));

        taskInfo = mTaskInfo.getValue();
        assertEquals(ONE_WEEK_IN_MILLISECONDS, taskInfo.getOneOffInfo().getWindowStartTimeMs());
    }

    @Test
    @Feature({"BackgroundSync"})
    public void testSchedulePeriodicSyncTaskTwice() {
        BackgroundSyncBackgroundTaskScheduler.getInstance().scheduleOneOffTask(
                BackgroundSyncTask.PERIODIC_SYNC_CHROME_WAKE_UP,
                /* minDelayMs= */ ONE_DAY_IN_MILLISECONDS);
        verify(mTaskScheduler, times(1))
                .schedule(eq(ContextUtils.getApplicationContext()), eq(mTaskInfo.getValue()));

        TaskInfo taskInfo = mTaskInfo.getValue();
        assertEquals(ONE_DAY_IN_MILLISECONDS, taskInfo.getOneOffInfo().getWindowStartTimeMs());

        BackgroundSyncBackgroundTaskScheduler.getInstance().scheduleOneOffTask(
                BackgroundSyncTask.PERIODIC_SYNC_CHROME_WAKE_UP,
                /* minDelayMs= */ ONE_WEEK_IN_MILLISECONDS);
        verify(mTaskScheduler, times(1))
                .schedule(eq(ContextUtils.getApplicationContext()), eq(mTaskInfo.getValue()));

        taskInfo = mTaskInfo.getValue();
        assertEquals(ONE_WEEK_IN_MILLISECONDS, taskInfo.getOneOffInfo().getWindowStartTimeMs());
    }

    @Test
    @Feature({"BackgroundSync"})
    public void testScheduleOneShotSyncWakeUpThenCancel() {
        BackgroundSyncBackgroundTaskScheduler.getInstance().scheduleOneOffTask(
                BackgroundSyncTask.ONE_SHOT_SYNC_CHROME_WAKE_UP,
                /* minDelayMs= */ ONE_DAY_IN_MILLISECONDS);
        BackgroundSyncBackgroundTaskScheduler.getInstance().cancelOneOffTask(
                BackgroundSyncTask.ONE_SHOT_SYNC_CHROME_WAKE_UP);

        verify(mTaskScheduler, times(1))
                .schedule(eq(ContextUtils.getApplicationContext()), eq(mTaskInfo.getValue()));
        verify(mTaskScheduler, times(1))
                .cancel(eq(ContextUtils.getApplicationContext()),
                        eq(TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID));
    }

    @Test
    @Feature({"BackgroundSync"})
    public void schedulePeriodicSyncWakeUpTaskThenCancel() {
        BackgroundSyncBackgroundTaskScheduler.getInstance().scheduleOneOffTask(
                BackgroundSyncTask.PERIODIC_SYNC_CHROME_WAKE_UP,
                /* minDelayMs= */ ONE_DAY_IN_MILLISECONDS);
        BackgroundSyncBackgroundTaskScheduler.getInstance().cancelOneOffTask(
                BackgroundSyncTask.PERIODIC_SYNC_CHROME_WAKE_UP);
        verify(mTaskScheduler, times(1))
                .schedule(eq(ContextUtils.getApplicationContext()), eq(mTaskInfo.getValue()));
        verify(mTaskScheduler, times(1))
                .cancel(eq(ContextUtils.getApplicationContext()),
                        eq(TaskIds.PERIODIC_BACKGROUND_SYNC_CHROME_WAKEUP_TASK_JOB_ID));
    }
}

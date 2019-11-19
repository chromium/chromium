// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.text.format.DateUtils;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

/**
 * Unit tests for BackgroundScheduler.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BackgroundSchedulerTest {
    private TriggerConditions mConditions1 = new TriggerConditions(
            true /* power */, 10 /* battery percentage */, true /* requires unmetered */);
    private TriggerConditions mConditions2 = new TriggerConditions(
            false /* power */, 0 /* battery percentage */, false /* does not require unmetered */);

    @Mock
    private BackgroundTaskScheduler mTaskScheduler;
    @Captor
    ArgumentCaptor<TaskInfo> mTaskInfo;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mTaskScheduler);
        doReturn(true)
                .when(mTaskScheduler)
                .schedule(eq(ContextUtils.getApplicationContext()), mTaskInfo.capture());
    }

    private void verifyFixedTaskInfoValues(TaskInfo info) {
        assertEquals(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID, info.getTaskId());
        assertEquals(OfflineBackgroundTask.class, info.getBackgroundTaskClass());
        assertTrue(info.isPersisted());
        assertFalse(info.isPeriodic());
        assertEquals(DateUtils.WEEK_IN_MILLIS, info.getOneOffInfo().getWindowEndTimeMs());
        assertTrue(info.getOneOffInfo().hasWindowStartTimeConstraint());

        long scheduledTimeMillis = TaskExtrasPacker.unpackTimeFromBundle(info.getExtras());
        assertTrue(scheduledTimeMillis > 0L);
    }

    @Test
    @Feature({"OfflinePages"})
    public void testScheduleUnmeteredAndCharging() {
        BackgroundScheduler.getInstance().schedule(mConditions1);
        verify(mTaskScheduler, times(1))
                .schedule(eq(ContextUtils.getApplicationContext()), eq(mTaskInfo.getValue()));

        TaskInfo info = mTaskInfo.getValue();
        verifyFixedTaskInfoValues(info);

        assertEquals(TaskInfo.NetworkType.UNMETERED, info.getRequiredNetworkType());
        assertTrue(info.requiresCharging());

        assertTrue(info.shouldUpdateCurrent());
        assertEquals(BackgroundScheduler.NO_DELAY, info.getOneOffInfo().getWindowStartTimeMs());

        assertEquals(
                mConditions1, TaskExtrasPacker.unpackTriggerConditionsFromBundle(info.getExtras()));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testScheduleMeteredAndNotCharging() {
        BackgroundScheduler.getInstance().schedule(mConditions2);
        verify(mTaskScheduler, times(1))
                .schedule(eq(ContextUtils.getApplicationContext()), eq(mTaskInfo.getValue()));

        TaskInfo info = mTaskInfo.getValue();
        verifyFixedTaskInfoValues(info);

        assertEquals(TaskInfo.NetworkType.ANY, info.getRequiredNetworkType());
        assertFalse(info.requiresCharging());

        assertTrue(info.shouldUpdateCurrent());
        assertEquals(BackgroundScheduler.NO_DELAY, info.getOneOffInfo().getWindowStartTimeMs());

        assertEquals(
                mConditions2, TaskExtrasPacker.unpackTriggerConditionsFromBundle(info.getExtras()));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testScheduleBackup() {
        BackgroundScheduler.getInstance().scheduleBackup(
                mConditions1, 5 * DateUtils.MINUTE_IN_MILLIS);
        verify(mTaskScheduler, times(1))
                .schedule(eq(ContextUtils.getApplicationContext()), eq(mTaskInfo.getValue()));

        TaskInfo info = mTaskInfo.getValue();
        verifyFixedTaskInfoValues(info);

        assertEquals(TaskInfo.NetworkType.UNMETERED, info.getRequiredNetworkType());
        assertTrue(info.requiresCharging());

        assertFalse(info.shouldUpdateCurrent());
        assertEquals(5 * DateUtils.MINUTE_IN_MILLIS, info.getOneOffInfo().getWindowStartTimeMs());

        assertEquals(
                mConditions1, TaskExtrasPacker.unpackTriggerConditionsFromBundle(info.getExtras()));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testCancel() {
        BackgroundScheduler.getInstance().schedule(mConditions1);
        verify(mTaskScheduler, times(1))
                .schedule(eq(ContextUtils.getApplicationContext()), eq(mTaskInfo.getValue()));

        doNothing()
                .when(mTaskScheduler)
                .cancel(eq(ContextUtils.getApplicationContext()),
                        eq(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID));
        BackgroundScheduler.getInstance().cancel();
        verify(mTaskScheduler, times(1))
                .cancel(eq(ContextUtils.getApplicationContext()),
                        eq(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID));
    }
}

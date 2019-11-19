// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskInfo;

import java.util.List;

/**
 * Unit tests for NotificationTriggerScheduler.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NotificationTriggerSchedulerTest {
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private NotificationTriggerScheduler.Natives mNativeMock;
    @Mock
    private BackgroundTaskScheduler mTaskScheduler;
    @Captor
    private ArgumentCaptor<TaskInfo> mTaskInfoCaptor;

    private NotificationTriggerScheduler.Clock mClock;

    private NotificationTriggerScheduler mTriggerScheduler;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mTaskScheduler);
        mocker.mock(NotificationTriggerSchedulerJni.TEST_HOOKS, mNativeMock);
        doReturn(true).when(mTaskScheduler).schedule(any(), mTaskInfoCaptor.capture());

        mClock = () -> 1415926535;
        mTriggerScheduler = new NotificationTriggerScheduler(mClock);
    }

    private static long getTimestamp(TaskInfo taskInfo) {
        final String key = NotificationTriggerBackgroundTask.KEY_TIMESTAMP;
        return taskInfo.getExtras().getLong(key);
    }

    private static long getScheduledDelay(TaskInfo taskInfo) {
        return taskInfo.getOneOffInfo().getWindowEndTimeMs();
    }

    @Test
    public void testSchedule_OnceInFuture() {
        long timestamp = mClock.currentTimeMillis() + 1000;

        mTriggerScheduler.schedule(timestamp);

        List<TaskInfo> taskInfos = mTaskInfoCaptor.getAllValues();
        assertEquals(1, taskInfos.size());
        assertEquals(timestamp, getTimestamp(taskInfos.get(0)));
        assertEquals(1000, getScheduledDelay(taskInfos.get(0)));
    }

    @Test
    public void testSchedule_OnceInPast() {
        long timestamp = mClock.currentTimeMillis() - 1000;

        mTriggerScheduler.schedule(timestamp);

        List<TaskInfo> taskInfos = mTaskInfoCaptor.getAllValues();
        assertEquals(1, taskInfos.size());
        assertEquals(timestamp, getTimestamp(taskInfos.get(0)));
        assertEquals(0, getScheduledDelay(taskInfos.get(0)));
    }

    @Test
    public void testSchedule_MultipleTimes() {
        long now = mClock.currentTimeMillis();

        // Only the first and fourth should schedule a new trigger as the others are ahead of the
        // currently scheduled trigger timestamp.
        long[] timestamps = {now + 10000, now + 10000, now + 20000, now + 5000, now + 7000};

        for (long timestamp : timestamps) {
            mTriggerScheduler.schedule(timestamp);
        }

        List<TaskInfo> taskInfos = mTaskInfoCaptor.getAllValues();
        assertEquals(2, taskInfos.size());
        assertEquals(timestamps[0], getTimestamp(taskInfos.get(0)));
        assertEquals(timestamps[3], getTimestamp(taskInfos.get(1)));
    }

    @Test
    public void testSchedule_ExistingTriggerInPast() {
        long past = mClock.currentTimeMillis() - 10000;
        long future = mClock.currentTimeMillis() + 10000;

        mTriggerScheduler.schedule(past);
        mTriggerScheduler.schedule(future);

        List<TaskInfo> taskInfos = mTaskInfoCaptor.getAllValues();
        assertEquals(2, taskInfos.size());
        assertEquals(past, getTimestamp(taskInfos.get(0)));
        // The first trigger has not been executed yet, scheduling it again.
        assertEquals(past, getTimestamp(taskInfos.get(1)));
    }

    @Test
    public void testCheckAndResetTrigger_WrongTimestamp() {
        long timestamp = mClock.currentTimeMillis() + 10000;

        mTriggerScheduler.schedule(timestamp);

        assertFalse(mTriggerScheduler.checkAndResetTrigger(timestamp + 1));
    }

    @Test
    public void testCheckAndResetTrigger_CorrectTimestamp() {
        long timestamp = mClock.currentTimeMillis() + 10000;

        mTriggerScheduler.schedule(timestamp);

        assertTrue(mTriggerScheduler.checkAndResetTrigger(timestamp));
    }

    @Test
    public void testCheckAndResetTrigger_CorrectTimestampTwice() {
        long timestamp = mClock.currentTimeMillis() + 10000;

        mTriggerScheduler.schedule(timestamp);
        mTriggerScheduler.checkAndResetTrigger(timestamp);

        assertFalse(mTriggerScheduler.checkAndResetTrigger(timestamp));
    }

    @Test
    public void testTriggerNotifications_CallsNative() {
        mTriggerScheduler.triggerNotifications();
        verify(mNativeMock).triggerNotifications();
    }

    @Test
    public void testReschedule() {
        long timestamp =
                mClock.currentTimeMillis() + NotificationTriggerScheduler.RESCHEDULE_DELAY_TIME;

        mTriggerScheduler.reschedule();

        List<TaskInfo> taskInfos = mTaskInfoCaptor.getAllValues();
        assertEquals(1, taskInfos.size());
        assertEquals(timestamp, getTimestamp(taskInfos.get(0)));
    }
}

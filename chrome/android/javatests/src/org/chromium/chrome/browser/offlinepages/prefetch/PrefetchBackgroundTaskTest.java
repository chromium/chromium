// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.offlinepages.OfflineTestUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ReducedModeNativeTestRule;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.components.offlinepages.PrefetchBackgroundTaskRescheduleType;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.HashMap;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link PrefetchBackgroundTask}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=OfflinePagesPrefetching,InterestFeedContentSuggestions"})
public class PrefetchBackgroundTaskTest {
    @Rule
    public ReducedModeNativeTestRule mNativeTestRule = new ReducedModeNativeTestRule();

    private static final double BACKOFF_JITTER_FACTOR = 0.33;
    private static final int SEMAPHORE_TIMEOUT_MS = 5000;
    private TestBackgroundTaskScheduler mScheduler;

    private static class TestPrefetchBackgroundTask extends PrefetchBackgroundTask {
        private TaskInfo mTaskInfo;
        private Semaphore mStopSemaphore = new Semaphore(0);

        public TestPrefetchBackgroundTask(TaskInfo taskInfo) {
            mTaskInfo = taskInfo;
        }

        public void startTask(Context context, final TaskFinishedCallback callback) {
            TaskParameters.Builder builder =
                    TaskParameters.create(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
            TaskParameters params = builder.build();
            onStartTask(context, params, new TaskFinishedCallback() {
                @Override
                public void taskFinished(boolean needsReschedule) {
                    callback.taskFinished(needsReschedule);
                    mStopSemaphore.release();
                }
            });
        }

        public void signalTaskFinished() {
            TestThreadUtils.runOnUiThreadBlocking(() -> { signalTaskFinishedForTesting(); });
        }

        public void stopTask() {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                TaskParameters.Builder builder =
                        TaskParameters.create(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
                TaskParameters params = builder.build();
                onStopTask(ContextUtils.getApplicationContext(), params);
            });
        }

        public void setTaskRescheduling(int rescheduleType) {
            PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                    () -> { setTaskReschedulingForTesting(rescheduleType); });
        }

        public void waitForTaskFinished() throws Exception {
            assertTrue(mStopSemaphore.tryAcquire(SEMAPHORE_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        }

        public TaskInfo taskInfo() {
            return mTaskInfo;
        }
    }

    private static class TestBackgroundTaskScheduler implements BackgroundTaskScheduler {
        private HashMap<Integer, TestPrefetchBackgroundTask> mTasks = new HashMap<>();
        private Semaphore mStartSemaphore = new Semaphore(0);
        private int mAddCount;
        private int mRemoveCount;

        @Override
        public boolean schedule(final Context context, final TaskInfo taskInfo) {
            mAddCount++;
            PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
                TestPrefetchBackgroundTask task = new TestPrefetchBackgroundTask(taskInfo);
                mTasks.put(taskInfo.getTaskId(), task);
                task.startTask(context, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        removeTask(taskInfo.getTaskId());
                    }
                });
                mStartSemaphore.release();
            });
            return true;
        }

        @Override
        public void cancel(Context context, int taskId) {
            removeTask(taskId);
        }

        @Override
        public void checkForOSUpgrade(Context context) {}

        @Override
        public void reschedule(Context context) {}

        public void waitForTaskStarted() throws Exception {
            assertTrue(mStartSemaphore.tryAcquire(SEMAPHORE_TIMEOUT_MS, TimeUnit.MILLISECONDS));
            // Reset for next task.
            mStartSemaphore = new Semaphore(0);
        }

        public TestPrefetchBackgroundTask getTask(int taskId) {
            return mTasks.get(taskId);
        }

        public void removeTask(int taskId) {
            mRemoveCount++;
            mTasks.remove(taskId);
        }

        public int addCount() {
            return mAddCount;
        }
        public int removeCount() {
            return mRemoveCount;
        }
    }

    TestPrefetchBackgroundTask validateAndGetScheduledTask(int additionalDelaySeconds) {
        TestPrefetchBackgroundTask scheduledTask =
                mScheduler.getTask(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
        assertNotNull(scheduledTask);
        TaskInfo scheduledTaskInfo = scheduledTask.taskInfo();
        assertEquals(true, scheduledTaskInfo.isPersisted());
        assertEquals(TaskInfo.NetworkType.UNMETERED, scheduledTaskInfo.getRequiredNetworkType());

        long defaultTaskStartTimeMs = TimeUnit.SECONDS.toMillis(
                PrefetchBackgroundTaskScheduler.DEFAULT_START_DELAY_SECONDS);
        long currentTaskStartTimeMs = scheduledTaskInfo.getOneOffInfo().getWindowStartTimeMs();
        if (additionalDelaySeconds == 0) {
            assertEquals(defaultTaskStartTimeMs, currentTaskStartTimeMs);
        } else {
            long maxTaskStartTimeMs =
                    defaultTaskStartTimeMs + TimeUnit.SECONDS.toMillis(additionalDelaySeconds);
            assertTrue(currentTaskStartTimeMs <= maxTaskStartTimeMs);
            assertTrue(currentTaskStartTimeMs >= maxTaskStartTimeMs * (1 - BACKOFF_JITTER_FACTOR));
        }

        return scheduledTask;
    }

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mScheduler = new TestBackgroundTaskScheduler();
            BackgroundTaskSchedulerFactory.setSchedulerForTesting(mScheduler);
        });
        OfflineTestUtil.setPrefetchingEnabledByServer(true);
        OfflineTestUtil.setGCMTokenForTesting("dummy_gcm_token");

        PrefetchBackgroundTask.alwaysSupportServiceManagerOnlyForTesting();
    }

    @After
    public void tearDown() {
        mNativeTestRule.assertOnlyServiceManagerStarted();
    }

    private void scheduleTask(int additionalDelaySeconds) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { PrefetchBackgroundTaskScheduler.scheduleTask(additionalDelaySeconds); });
    }

    @Test
    @SmallTest
    public void testSchedule() throws Exception {
        PrefetchBackgroundTask.skipConditionCheckingForTesting();
        scheduleTask(0);
        mScheduler.waitForTaskStarted();
        TestPrefetchBackgroundTask task = validateAndGetScheduledTask(0);
        task.signalTaskFinished();
        task.waitForTaskFinished();
    }

    @Test
    @SmallTest
    public void testScheduleWithAdditionalDelay() throws Exception {
        final int additionalDelaySeconds = 15;
        PrefetchBackgroundTask.skipConditionCheckingForTesting();
        scheduleTask(additionalDelaySeconds);
        mScheduler.waitForTaskStarted();
        TestPrefetchBackgroundTask task = validateAndGetScheduledTask(additionalDelaySeconds);
        task.signalTaskFinished();
        task.waitForTaskFinished();
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/875433")
    public void testReschedule() throws Exception {
        PrefetchBackgroundTask.skipConditionCheckingForTesting();
        scheduleTask(0);
        mScheduler.waitForTaskStarted();
        TestPrefetchBackgroundTask task = validateAndGetScheduledTask(0);

        // Requests a reschedule without backoff.
        task.setTaskRescheduling(PrefetchBackgroundTaskRescheduleType.RESCHEDULE_WITHOUT_BACKOFF);
        task.signalTaskFinished();
        task.waitForTaskFinished();
        mScheduler.waitForTaskStarted();
        // No additional delay due to no backoff asked.
        task = validateAndGetScheduledTask(0);

        // Requests a reschedule with backoff.
        task.setTaskRescheduling(PrefetchBackgroundTaskRescheduleType.RESCHEDULE_WITH_BACKOFF);
        task.signalTaskFinished();
        task.waitForTaskFinished();
        mScheduler.waitForTaskStarted();
        // Adding initial delay due to backoff.
        task = validateAndGetScheduledTask(30);

        // Requests another reschedule with backoff.
        task.setTaskRescheduling(PrefetchBackgroundTaskRescheduleType.RESCHEDULE_WITH_BACKOFF);
        task.signalTaskFinished();
        task.waitForTaskFinished();
        mScheduler.waitForTaskStarted();
        // Delay doubled due to exponential backoff.
        task = validateAndGetScheduledTask(60);

        // Simulate killing the task by the system.
        task.stopTask();
        task.waitForTaskFinished();
        mScheduler.waitForTaskStarted();
        // Additional delay is removed if it is killed by the system.
        task = validateAndGetScheduledTask(0);

        // Finishes the task without rescheduling.
        task.setTaskRescheduling(PrefetchBackgroundTaskRescheduleType.NO_RESCHEDULE);
        task.signalTaskFinished();
        task.waitForTaskFinished();

        assertEquals(5, mScheduler.addCount());
        assertEquals(5, mScheduler.removeCount());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/870295")
    public void testSuspend() throws Exception {
        PrefetchBackgroundTask.skipConditionCheckingForTesting();
        scheduleTask(0);
        mScheduler.waitForTaskStarted();
        TestPrefetchBackgroundTask task = validateAndGetScheduledTask(0);

        // Requests a suspension.
        task.setTaskRescheduling(PrefetchBackgroundTaskRescheduleType.SUSPEND);
        // The suspension will not be affected by requesting retry with or without backof.
        task.setTaskRescheduling(PrefetchBackgroundTaskRescheduleType.RESCHEDULE_WITHOUT_BACKOFF);
        task.setTaskRescheduling(PrefetchBackgroundTaskRescheduleType.RESCHEDULE_WITH_BACKOFF);

        task.signalTaskFinished();
        task.waitForTaskFinished();
        mScheduler.waitForTaskStarted();
        // Delay for 1 day due to suspension.
        task = validateAndGetScheduledTask(3600 * 24);

        // The previous suspension should be removed. Rescheduling with backoff should work.
        task.setTaskRescheduling(PrefetchBackgroundTaskRescheduleType.RESCHEDULE_WITH_BACKOFF);
        task.signalTaskFinished();
        task.waitForTaskFinished();
        mScheduler.waitForTaskStarted();
        // Adding initial delay due to backoff.
        task = validateAndGetScheduledTask(30);

        task.signalTaskFinished();
        task.waitForTaskFinished();
    }
}

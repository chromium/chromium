// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offline.measurements;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;

import java.util.HashMap;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link OfflineMeasurementsBackgroundTask}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
public class OfflineMeasurementsBackgroundTaskUnitTest {
    /**
     * Fake of BackgroundTaskScheduler system service.
     */
    public static class FakeBackgroundTaskScheduler implements BackgroundTaskScheduler {
        private HashMap<Integer, TaskInfo> mTaskInfos = new HashMap<>();

        @Override
        public boolean schedule(Context context, TaskInfo taskInfo) {
            mTaskInfos.put(taskInfo.getTaskId(), taskInfo);
            return true;
        }

        @Override
        public void cancel(Context context, int taskId) {
            mTaskInfos.remove(taskId);
        }

        @Override
        public boolean isScheduled(Context context, int taskId) {
            return (mTaskInfos.get(taskId) != null);
        }

        @Override
        public void checkForOSUpgrade(Context context) {}

        @Override
        public void reschedule(Context context) {}

        public TaskInfo getTaskInfo(int taskId) {
            return mTaskInfos.get(taskId);
        }

        public Boolean containsTaskId(int taskId) {
            return mTaskInfos.containsKey(taskId);
        }
    }

    private FakeBackgroundTaskScheduler mFakeBackgroundTaskScheduler;

    /**
     * Fake of OfflineMeasurementsBackgroundTask.Clock that can be used to test the timing parts of
     * OfflineMeasurementsBackgroundTask.
     */
    public static class FakeClock implements OfflineMeasurementsBackgroundTask.Clock {
        private long mCurrentTimeMillis;

        public FakeClock() {
            mCurrentTimeMillis = 0;
        }

        @Override
        public long currentTimeMillis() {
            return mCurrentTimeMillis;
        }

        public void setCurrentTimeMillis(long currentTimeMillis) {
            mCurrentTimeMillis = currentTimeMillis;
        }

        public void advanceCurrentTimeMillis(long millis) {
            mCurrentTimeMillis += millis;
        }
    }

    private FakeClock mFakeClock;

    @Before
    public void setUp() {
        mFakeBackgroundTaskScheduler = new FakeBackgroundTaskScheduler();
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mFakeBackgroundTaskScheduler);

        mFakeClock = new FakeClock();
        OfflineMeasurementsBackgroundTask.setClockForTesting(mFakeClock);

        ShadowRecordHistogram.reset();

        // Clears the testing override for the measurement interval.
        OfflineMeasurementsBackgroundTask.setNewMeasurementIntervalInMinutesForTesting(
                0); // IN-TEST
    }

    private void setFeatureStatusForTest(boolean isEnabled) {
        HashMap<String, Boolean> testFeatures = new HashMap<String, Boolean>();
        testFeatures.put(ChromeFeatureList.OFFLINE_MEASUREMENTS_BACKGROUND_TASK, isEnabled);
        FeatureList.setTestFeatures(testFeatures);

        CachedFeatureFlags.setForTesting(
                ChromeFeatureList.OFFLINE_MEASUREMENTS_BACKGROUND_TASK, isEnabled);
    }

    private void assertUmaHistogramHasUniqueSample(
            String messagePrefix, String umaHistogram, int sample, int numSamples) {
        assertEquals(String.format("%s the UMA histogram %s should have %d total entries",
                             messagePrefix, umaHistogram, numSamples),
                numSamples, ShadowRecordHistogram.getHistogramTotalCountForTesting(umaHistogram));

        if (numSamples > 0) {
            assertEquals(
                    String.format(
                            "%s the UMA histogram %s should have %d samples with a value of %d",
                            messagePrefix, umaHistogram, numSamples, sample),
                    numSamples,
                    ShadowRecordHistogram.getHistogramValueCountForTesting(umaHistogram, sample));
        }
    }

    private void assertTaskScheduledWithCorrectInterval(int expectedIntervalInMinutes) {
        // Check that the TimingInfo of the scheduled task is: 1) an instance of PeriodicInfo, and
        // 2) the periodic interval is the default interval specified in
        // OfflineMeasurementBackgroundTask.
        TaskInfo thisTaskInfo =
                mFakeBackgroundTaskScheduler.getTaskInfo(TaskIds.OFFLINE_MEASUREMENT_JOB_ID);
        TaskInfo.TimingInfo thisTimingInfo = thisTaskInfo.getTimingInfo();
        assertTrue("Task's TimingInfo should be a PeriodicInfo",
                thisTimingInfo instanceof TaskInfo.PeriodicInfo);

        TaskInfo.PeriodicInfo thisPeriodicInfo = (TaskInfo.PeriodicInfo) thisTimingInfo;
        assertEquals("Task should be periodically scheduled with the given interval",
                TimeUnit.MINUTES.toMillis(expectedIntervalInMinutes),
                thisPeriodicInfo.getIntervalMs());
    }

    @Test
    public void scheduleTaskWhenFeatureDisabled() {
        // Disable the Offline Measurements feature for this test.
        setFeatureStatusForTest(false);

        // Tries to schedule task.
        OfflineMeasurementsBackgroundTask.maybeScheduleTaskAndReportMetrics();

        // Check that mFakeTaskScheduler doesn't have an entry for this task.
        assertFalse("Task shouldn't be scheduled when feature is disabled",
                mFakeBackgroundTaskScheduler.containsTaskId(TaskIds.OFFLINE_MEASUREMENT_JOB_ID));

        // Check that there are no entries in Offline.Measurements.MeasurementInterval.
        assertUmaHistogramHasUniqueSample("When the feature is disabled,",
                OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL, 0, 0);
    }

    @Test
    public void scheduleTaskWhenFeatureEnabled() {
        // Enable the Offline Measurements feature for this test.
        setFeatureStatusForTest(true);

        // Tries to schedule the task.
        OfflineMeasurementsBackgroundTask.maybeScheduleTaskAndReportMetrics();

        // Check that mFakeTaskScheduler has an entry for this task with the correct taskInfo.
        assertTrue("Task should be scheduled when the feature is enabled",
                mFakeBackgroundTaskScheduler.containsTaskId(TaskIds.OFFLINE_MEASUREMENT_JOB_ID));

        // Check that the task is scheduled with the default measurement interval.
        assertTaskScheduledWithCorrectInterval(
                OfflineMeasurementsBackgroundTask.DEFAULT_MEASUREMENT_INTERVAL_IN_MINUTES);

        // Check that Offline.Measurements.MeasurementInterval has one entry of the default
        // interval.
        assertUmaHistogramHasUniqueSample("When the feature is enabled,",
                OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL,
                (int) TimeUnit.MINUTES.toMillis(
                        OfflineMeasurementsBackgroundTask.DEFAULT_MEASUREMENT_INTERVAL_IN_MINUTES),
                1);
    }

    @Test
    public void scheduleTaskWithDifferentInterval() {
        // Enables the feature for this test.
        setFeatureStatusForTest(true);

        // Establish test constants.
        final int measurementInterval1 = 15;
        final int measurementInterval2 = 30;

        // Schedule the task with the first measurement interval.
        OfflineMeasurementsBackgroundTask.setNewMeasurementIntervalInMinutesForTesting(
                measurementInterval1);
        OfflineMeasurementsBackgroundTask.maybeScheduleTaskAndReportMetrics();

        // Check that task was correctly scheduled with the first measurement interval.
        assertTrue("Task should be scheduled when the feature is enabled",
                mFakeBackgroundTaskScheduler.containsTaskId(TaskIds.OFFLINE_MEASUREMENT_JOB_ID));
        assertTaskScheduledWithCorrectInterval(measurementInterval1);
        assertUmaHistogramHasUniqueSample("When the feature is enabled,",
                OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL,
                (int) TimeUnit.MINUTES.toMillis(measurementInterval1), 1);

        // Try scheduling again with the same measurement interval.
        OfflineMeasurementsBackgroundTask.maybeScheduleTaskAndReportMetrics();

        // If we schedule again with the same measurement interval, nothing should change.
        assertTrue("Task should be scheduled when the feature is enabled",
                mFakeBackgroundTaskScheduler.containsTaskId(TaskIds.OFFLINE_MEASUREMENT_JOB_ID));
        assertTaskScheduledWithCorrectInterval(measurementInterval1);
        assertUmaHistogramHasUniqueSample("When the feature is enabled,",
                OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL,
                (int) TimeUnit.MINUTES.toMillis(measurementInterval1), 1);

        // Schedule the task with the second measurement interval.
        OfflineMeasurementsBackgroundTask.setNewMeasurementIntervalInMinutesForTesting(
                measurementInterval2);
        OfflineMeasurementsBackgroundTask.maybeScheduleTaskAndReportMetrics();

        // Check that the task is now scheduled with the second measurement interval
        assertTrue("Task should be scheduled when the feature is enabled",
                mFakeBackgroundTaskScheduler.containsTaskId(TaskIds.OFFLINE_MEASUREMENT_JOB_ID));
        assertTaskScheduledWithCorrectInterval(measurementInterval2);

        // Check that the UMA histogram has one entry for each measurement interval.
        assertEquals("There should be one entry for each time task was scheduled", 2,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        OfflineMeasurementsBackgroundTask
                                .OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL));
        assertEquals("There should be one entry for the first measurement interval", 1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL,
                        (int) TimeUnit.MINUTES.toMillis(measurementInterval1)));
        assertEquals("There should be one entry for the first measurement interval", 1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL,
                        (int) TimeUnit.MINUTES.toMillis(measurementInterval2)));

        // Disable the feature and try to reschedule.
        setFeatureStatusForTest(false);
        OfflineMeasurementsBackgroundTask.maybeScheduleTaskAndReportMetrics();

        // Check that the task is no longer scheduled
        assertFalse("Task shouldn't be scheduled when feature is disabled",
                mFakeBackgroundTaskScheduler.containsTaskId(TaskIds.OFFLINE_MEASUREMENT_JOB_ID));
    }

    @Test
    public void runTask() {
        setFeatureStatusForTest(true);

        final long[] intervals = {100, 200, 300, 400, 500};
        TaskParameters testParameters =
                TaskParameters.create(TaskIds.OFFLINE_MEASUREMENT_JOB_ID).build();
        BackgroundTask.TaskFinishedCallback testCallback = needsReschedule -> {
            fail("Task shouldn't need to use callback");
        };

        mFakeClock.setCurrentTimeMillis(1000);

        OfflineMeasurementsBackgroundTask task = new OfflineMeasurementsBackgroundTask();
        task.onStartTask(null, testParameters, testCallback);
        for (long interval : intervals) {
            // Increment clock and run task again.
            mFakeClock.advanceCurrentTimeMillis(interval);

            task = new OfflineMeasurementsBackgroundTask();
            task.onStartTask(null, testParameters, testCallback);
        }

        // Report the persisted metrics.
        OfflineMeasurementsBackgroundTask.maybeScheduleTaskAndReportMetrics();

        // Check that the metrics were reported as expected.
        assertEquals(
                "The total number of entries should equal to the number of intervals between tasks",
                intervals.length,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        OfflineMeasurementsBackgroundTask
                                .OFFLINE_MEASUREMENTS_TIME_BETWEEN_CHECKS));
        for (long interval : intervals) {
            assertEquals("There should be one entry for each interval between tasks", 1,
                    ShadowRecordHistogram.getHistogramValueCountForTesting(
                            OfflineMeasurementsBackgroundTask
                                    .OFFLINE_MEASUREMENTS_TIME_BETWEEN_CHECKS,
                            (int) interval));
        }
    }
}

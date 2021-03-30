// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offline.measurements;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.browser.content.ContentUtils;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.HashMap;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Tests for {@link OfflineMeasurementsBackgroundTask}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class OfflineMeasurementsBackgroundTaskTest {
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

    private static final int TIMEOUT_MS = 10000;
    private Semaphore mSemaphore = new Semaphore(0);
    private int mNumTaskFinishedCallbacksTriggered;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mFakeBackgroundTaskScheduler = new FakeBackgroundTaskScheduler();
            BackgroundTaskSchedulerFactory.setSchedulerForTesting(mFakeBackgroundTaskScheduler);
        });

        mFakeClock = new FakeClock();
        OfflineMeasurementsBackgroundTask.setClockForTesting(mFakeClock);

        // Clears the testing override for the measurement interval.
        OfflineMeasurementsBackgroundTask.setNewMeasurementIntervalInMinutesForTesting(
                0); // IN-TEST

        mNumTaskFinishedCallbacksTriggered = 0;

        // Overrides the checks for airplane mode and roaming so that we don't run the full checks
        // in any tests.
        OfflineMeasurementsBackgroundTask.setIsAirplaneModeEnabledForTesting(false); // IN-TEST
        OfflineMeasurementsBackgroundTask.setIsRoamingForTesting(false); // IN-TEST
    }

    private void maybeScheduleTaskAndReportMetrics() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { OfflineMeasurementsBackgroundTask.maybeScheduleTaskAndReportMetrics(); });
    }

    private void setFeatureStatusForTest(boolean isEnabled) {
        HashMap<String, Boolean> testFeatures = new HashMap<String, Boolean>();
        testFeatures.put(ChromeFeatureList.OFFLINE_MEASUREMENTS_BACKGROUND_TASK, isEnabled);
        FeatureList.setTestFeatures(testFeatures);

        CachedFeatureFlags.setForTesting(
                ChromeFeatureList.OFFLINE_MEASUREMENTS_BACKGROUND_TASK, isEnabled);
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

    /**
     * Tests scheduling the Offline Measurements background task when the feature is disabled.
     * Checks that task is not scheduled, no samples are recorded to the
     * OfflineMeasurements.MeasurementInterval histogram and no HTTP probe parameters are written to
     * prefs.
     */
    @Test
    @SmallTest
    public void scheduleTaskWhenFeatureDisabled() {
        // Disable the Offline Measurements feature for this test.
        setFeatureStatusForTest(false);

        // Tries to schedule task.
        maybeScheduleTaskAndReportMetrics();

        // Check that mFakeTaskScheduler doesn't have an entry for this task.
        assertFalse("Task shouldn't be scheduled when feature is disabled",
                mFakeBackgroundTaskScheduler.containsTaskId(TaskIds.OFFLINE_MEASUREMENT_JOB_ID));

        // Check that there are no entries in Offline.Measurements.MeasurementInterval.
        assertEquals("No samples should be written to histogram when feature is disabled", 0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OfflineMeasurementsBackgroundTask
                                .OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL));

        // Check that the HTTP params are not in Prefs
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        assertFalse("HTTP probe parameters shouldn't be written to prefs when feature is disabled",
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys.OFFLINE_MEASUREMENTS_USER_AGENT_STRING));
        assertFalse("HTTP probe parameters shouldn't be written to prefs when feature is disabled",
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_URL));
        assertFalse("HTTP probe parameters shouldn't be written to prefs when feature is disabled",
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_TIMEOUT_MS));
        assertFalse("HTTP probe parameters shouldn't be written to prefs when feature is disabled",
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_METHOD));
    }

    /**
     * Tests scheduling the Offline Measurements background task when the feature is enabled. Checks
     * that the feature is scheduled with the expected parameters, the expected value is recorded to
     * the OfflineMeasurements.MeasurementInterval histogram, and the default HTTP probe parameters
     * are used.
     */
    @Test
    @SmallTest
    public void scheduleTaskWhenFeatureEnabled() {
        // Enable the Offline Measurements feature for this test.
        setFeatureStatusForTest(true);

        // Tries to schedule the task.
        maybeScheduleTaskAndReportMetrics();

        // Check that mFakeTaskScheduler has an entry for this task with the correct taskInfo.
        assertTrue("Task should be scheduled when the feature is enabled",
                mFakeBackgroundTaskScheduler.containsTaskId(TaskIds.OFFLINE_MEASUREMENT_JOB_ID));

        // Check that the task is scheduled with the default measurement interval.
        assertTaskScheduledWithCorrectInterval(
                OfflineMeasurementsBackgroundTask.DEFAULT_MEASUREMENT_INTERVAL_IN_MINUTES);

        // Check that Offline.Measurements.MeasurementInterval has one entry of the default
        // interval.
        assertEquals("Only the default measurement interval should be recorded to the histogram", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OfflineMeasurementsBackgroundTask
                                .OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL));
        assertEquals("Only the default measurement interval should be recorded to the histogram", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL,
                        (int) TimeUnit.MINUTES.toMillis(
                                OfflineMeasurementsBackgroundTask
                                        .DEFAULT_MEASUREMENT_INTERVAL_IN_MINUTES)));

        // Check that the HTTP params are in prefs
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        assertEquals("The user agent string should always be written to prefs",
                ContentUtils.getBrowserUserAgent(),
                sharedPreferencesManager.readString(
                        ChromePreferenceKeys.OFFLINE_MEASUREMENTS_USER_AGENT_STRING, ""));
        assertFalse("HTTP probe parameters shouldn't be written to prefs when using default values",
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_URL));
        assertFalse("HTTP probe parameters shouldn't be written to prefs when using default values",
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_TIMEOUT_MS));
        assertFalse("HTTP probe parameters shouldn't be written to prefs when using default values",
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_METHOD));
    }

    /**
     * Tests scheduling the Offline Measurements background task when an instance is already
     * running, but with a different measurement interval. when this happens, we should cancel the
     * already running task, and schedule a new instance of the task with the new parameter.
     */
    @Test
    @SmallTest
    public void scheduleTaskWithDifferentInterval() {
        // Enables the feature for this test.
        setFeatureStatusForTest(true);

        // Establish test constants.
        final int measurementInterval1 = 15;
        final int measurementInterval2 = 30;

        // Schedule the task with the first measurement interval.
        OfflineMeasurementsBackgroundTask.setNewMeasurementIntervalInMinutesForTesting(
                measurementInterval1);
        maybeScheduleTaskAndReportMetrics();

        // Check that task was correctly scheduled with the first measurement interval.
        assertTrue("Task should be scheduled when the feature is enabled",
                mFakeBackgroundTaskScheduler.containsTaskId(TaskIds.OFFLINE_MEASUREMENT_JOB_ID));
        assertTaskScheduledWithCorrectInterval(measurementInterval1);

        assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OfflineMeasurementsBackgroundTask
                                .OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL,
                        (int) TimeUnit.MINUTES.toMillis(measurementInterval1)));

        // Try scheduling again with the same measurement interval.
        maybeScheduleTaskAndReportMetrics();

        // If we schedule again with the same measurement interval, nothing should change.
        assertTrue("Task should be scheduled when the feature is enabled",
                mFakeBackgroundTaskScheduler.containsTaskId(TaskIds.OFFLINE_MEASUREMENT_JOB_ID));
        assertTaskScheduledWithCorrectInterval(measurementInterval1);

        assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OfflineMeasurementsBackgroundTask
                                .OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL));

        // Schedule the task with the second measurement interval.
        OfflineMeasurementsBackgroundTask.setNewMeasurementIntervalInMinutesForTesting(
                measurementInterval2);
        maybeScheduleTaskAndReportMetrics();

        // Check that the task is now scheduled with the second measurement interval
        assertTrue("Task should be scheduled when the feature is enabled",
                mFakeBackgroundTaskScheduler.containsTaskId(TaskIds.OFFLINE_MEASUREMENT_JOB_ID));
        assertTaskScheduledWithCorrectInterval(measurementInterval2);

        assertEquals(2,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OfflineMeasurementsBackgroundTask
                                .OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL,
                        (int) TimeUnit.MINUTES.toMillis(measurementInterval1)));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL,
                        (int) TimeUnit.MINUTES.toMillis(measurementInterval2)));

        // Disable the feature and try to reschedule.
        setFeatureStatusForTest(false);
        maybeScheduleTaskAndReportMetrics();

        // Check that the task is no longer scheduled
        assertFalse("Task shouldn't be scheduled when feature is disabled",
                mFakeBackgroundTaskScheduler.containsTaskId(TaskIds.OFFLINE_MEASUREMENT_JOB_ID));
    }

    /**
     * Tests running the Offline Measurements background task multiple times in a row. Checks that
     * we record the expected values to the histograms that track the time between each check and
     * the result of each HTTP probe.
     */
    @Test
    @MediumTest
    public void runTask() throws Exception {
        // Enable feature and initialize the HTTP probe parameters
        setFeatureStatusForTest(true);
        maybeScheduleTaskAndReportMetrics();

        // Start the test server, and give the URL to the background task.
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL("/nocontent");
        OfflineMeasurementsBackgroundTask.setHttpProbeUrlForTesting(testUrl);

        // Set task parameters.
        final long[] intervals = {100000, 200000, 300000, 400000, 500000};
        TaskParameters testParameters =
                TaskParameters.create(TaskIds.OFFLINE_MEASUREMENT_JOB_ID).build();
        BackgroundTask.TaskFinishedCallback testCallback = needsReschedule -> {
            mSemaphore.release();
            mNumTaskFinishedCallbacksTriggered++;
        };
        mFakeClock.setCurrentTimeMillis(100000);

        // Run the initial task.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OfflineMeasurementsBackgroundTask task = new OfflineMeasurementsBackgroundTask();
            task.onStartTask(null, testParameters, testCallback);
        });
        assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));

        for (long interval : intervals) {
            // Increment clock and run task again.
            mFakeClock.advanceCurrentTimeMillis(interval);
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                OfflineMeasurementsBackgroundTask task = new OfflineMeasurementsBackgroundTask();
                task.onStartTask(null, testParameters, testCallback);
            });
            assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        }

        // Report the persisted metrics.
        maybeScheduleTaskAndReportMetrics();

        // Check that the intervals were reported as expected.
        assertEquals(
                "The total number of entries should equal to the number of intervals between tasks",
                intervals.length,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OfflineMeasurementsBackgroundTask
                                .OFFLINE_MEASUREMENTS_TIME_BETWEEN_CHECKS));
        for (long interval : intervals) {
            assertEquals("There should be one entry for each interval between tasks", 1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            OfflineMeasurementsBackgroundTask
                                    .OFFLINE_MEASUREMENTS_TIME_BETWEEN_CHECKS,
                            (int) interval));
        }

        // Check HTTP probe results.
        assertEquals("One entry should be recorded for each time the task ran",
                intervals.length + 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULT));
        assertEquals("All HTTP probes should have a result of VALIDATED", intervals.length + 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULT,
                        OfflineMeasurementsBackgroundTask.ProbeResult.VALIDATED));

        // Check that the callback was triggered each time the task was run.
        assertEquals("Each task should have called the task finished callback",
                intervals.length + 1, mNumTaskFinishedCallbacksTriggered);
    }

    /** Tests running the HTTP probe with a response with a code of 204. */
    @Test
    @MediumTest
    public void runHttpProbe_ExpectedResponseCode() throws Exception {
        // Enable feature and initialize the HTTP probe parameters
        setFeatureStatusForTest(true);
        maybeScheduleTaskAndReportMetrics();

        // Start the test server, and give the URL to the background task.
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL("/nocontent");
        OfflineMeasurementsBackgroundTask.setHttpProbeUrlForTesting(testUrl);

        // Set the task parameters.
        TaskParameters testParameters =
                TaskParameters.create(TaskIds.OFFLINE_MEASUREMENT_JOB_ID).build();
        BackgroundTask.TaskFinishedCallback testCallback = needsReschedule -> {
            mSemaphore.release();
            mNumTaskFinishedCallbacksTriggered++;
        };

        // Run the task.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OfflineMeasurementsBackgroundTask task = new OfflineMeasurementsBackgroundTask();
            task.onStartTask(null, testParameters, testCallback);
        });
        assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));

        // Report the persisted metrics.
        maybeScheduleTaskAndReportMetrics();

        // Check HTTP probe results.
        assertEquals("The HTTP probe should have only been run once", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULT));
        assertEquals("The HTTP probe should have a result of VALIDATED", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULT,
                        OfflineMeasurementsBackgroundTask.ProbeResult.VALIDATED));

        // Check that the callback was triggered.
        assertEquals("The task should call the task finished callback", 1,
                mNumTaskFinishedCallbacksTriggered);
    }

    /**
     * Tests running the HTTP probe with a response with an unexpected code, but still no content.
     * This can happen in the case of a broken transparent proxy.
     */
    @Test
    @MediumTest
    public void runHttpProbe_UnexpectedCodeWithoutContent() throws Exception {
        // Enable feature and initialize the HTTP probe parameters
        setFeatureStatusForTest(true);
        maybeScheduleTaskAndReportMetrics();

        // Start the test server, and give the URL to the background task.
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL("/echo?status=200");
        OfflineMeasurementsBackgroundTask.setHttpProbeUrlForTesting(testUrl);

        // This will cause the test server to return empty content.
        OfflineMeasurementsBackgroundTask.setHttpProbeMethodForTesting("POST");

        // Set the task parameters.
        TaskParameters testParameters =
                TaskParameters.create(TaskIds.OFFLINE_MEASUREMENT_JOB_ID).build();
        BackgroundTask.TaskFinishedCallback testCallback = needsReschedule -> {
            mSemaphore.release();
            mNumTaskFinishedCallbacksTriggered++;
        };

        // Run the task.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OfflineMeasurementsBackgroundTask task = new OfflineMeasurementsBackgroundTask();
            task.onStartTask(null, testParameters, testCallback);
        });
        assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));

        // Report the persisted metrics.
        maybeScheduleTaskAndReportMetrics();

        // Check HTTP probe results.
        assertEquals("The HTTP probe should have only been run once", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULT));
        assertEquals("The HTTP probe should have a result of UNEXPECTED_RESPONSE", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULT,
                        OfflineMeasurementsBackgroundTask.ProbeResult.UNEXPECTED_RESPONSE));

        // Check that the callback was triggered.
        assertEquals("The task should call the task finished callback", 1,
                mNumTaskFinishedCallbacksTriggered);
    }

    /**
     * Tests running the HTTP probe with a response with an unexpected code and non-zero content.
     * This happens in cases with a captive portal.
     */
    @Test
    @MediumTest
    public void runHttpProbe_UnexpectedCodeWithContent() throws Exception {
        // Enable feature and initialize the HTTP probe parameters.
        setFeatureStatusForTest(true);
        maybeScheduleTaskAndReportMetrics();

        // Start the test server, and give the URL to the background task.
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL("/echo?status=200");
        OfflineMeasurementsBackgroundTask.setHttpProbeUrlForTesting(testUrl);

        // Set the task parameters.
        TaskParameters testParameters =
                TaskParameters.create(TaskIds.OFFLINE_MEASUREMENT_JOB_ID).build();
        BackgroundTask.TaskFinishedCallback testCallback = needsReschedule -> {
            mSemaphore.release();
            mNumTaskFinishedCallbacksTriggered++;
        };

        // Run the task.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OfflineMeasurementsBackgroundTask task = new OfflineMeasurementsBackgroundTask();
            task.onStartTask(null, testParameters, testCallback);
        });
        assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));

        // Report the persisted metrics.
        maybeScheduleTaskAndReportMetrics();

        // Check HTTP probe results.
        assertEquals("The HTTP probe should have only been run once", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULT));
        assertEquals("The HTTP probe should have a result of UNEXPECTED_RESPONSE", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULT,
                        OfflineMeasurementsBackgroundTask.ProbeResult.UNEXPECTED_RESPONSE));

        // Check that the callback was triggered.
        assertEquals("The task should call the task finished callback", 1,
                mNumTaskFinishedCallbacksTriggered);
    }

    /** Tests running the HTTP probe and getting a server error response. */
    @Test
    @MediumTest
    public void runHttpProbe_ServerError() throws Exception {
        // Enable feature and initialize the HTTP probe parameters.
        setFeatureStatusForTest(true);
        maybeScheduleTaskAndReportMetrics();

        // Start the test server, and give the URL to the background task.
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL("/echo?status=500");
        OfflineMeasurementsBackgroundTask.setHttpProbeUrlForTesting(testUrl);

        // Set the task parameters.
        TaskParameters testParameters =
                TaskParameters.create(TaskIds.OFFLINE_MEASUREMENT_JOB_ID).build();
        BackgroundTask.TaskFinishedCallback testCallback = needsReschedule -> {
            mSemaphore.release();
            mNumTaskFinishedCallbacksTriggered++;
        };

        // Run the task.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OfflineMeasurementsBackgroundTask task = new OfflineMeasurementsBackgroundTask();
            task.onStartTask(null, testParameters, testCallback);
        });
        assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));

        // Report the persisted metrics.
        maybeScheduleTaskAndReportMetrics();

        // Check HTTP probe results.
        assertEquals("The HTTP probe should have only been run once", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULT));
        assertEquals("The HTTP probe should have a result of SERVER_ERROR", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULT,
                        OfflineMeasurementsBackgroundTask.ProbeResult.SERVER_ERROR));

        // Check that the callback was triggered.
        assertEquals("The task should call the task finished callback", 1,
                mNumTaskFinishedCallbacksTriggered);
    }

    /** Tests running the HTTP probe and not getting a response from the server. */
    @Test
    @MediumTest
    public void runHttpProbe_NoInternet() throws Exception {
        // Enable feature and initialize the HTTP probe parameters.
        setFeatureStatusForTest(true);
        maybeScheduleTaskAndReportMetrics();

        // Start the test server, and give the URL to the background task.
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL("/hung");
        OfflineMeasurementsBackgroundTask.setHttpProbeUrlForTesting(testUrl);

        // Set the task parameters.
        TaskParameters testParameters =
                TaskParameters.create(TaskIds.OFFLINE_MEASUREMENT_JOB_ID).build();
        BackgroundTask.TaskFinishedCallback testCallback = needsReschedule -> {
            mSemaphore.release();
            mNumTaskFinishedCallbacksTriggered++;
        };

        // Run the task, then immediately cancel it.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OfflineMeasurementsBackgroundTask task = new OfflineMeasurementsBackgroundTask();
            task.onStartTask(null, testParameters, testCallback);
        });
        assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));

        // Report the persisted metrics.
        maybeScheduleTaskAndReportMetrics();

        // Check HTTP probe results.
        assertEquals("The HTTP probe should have only been run once", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULT));
        assertEquals("The HTTP probe should have a result of NO_INTERNET", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULT,
                        OfflineMeasurementsBackgroundTask.ProbeResult.NO_INTERNET));

        // Check that the callback was not triggered.
        assertEquals("The task should call the task finished callback", 1,
                mNumTaskFinishedCallbacksTriggered);
    }

    /** Tests running the HTTP probe and canceling the task before the probe finishes. */
    @Test
    @MediumTest
    public void runHttpProbe_CancelTask() throws Exception {
        // Enable feature and initialize the HTTP probe parameters.
        setFeatureStatusForTest(true);
        maybeScheduleTaskAndReportMetrics();

        // Start the test server, and give the URL to the background task.
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL("/hung");
        OfflineMeasurementsBackgroundTask.setHttpProbeUrlForTesting(testUrl);

        // Set the task parameters.
        TaskParameters testParameters =
                TaskParameters.create(TaskIds.OFFLINE_MEASUREMENT_JOB_ID).build();
        BackgroundTask.TaskFinishedCallback testCallback = needsReschedule -> {
            mNumTaskFinishedCallbacksTriggered++;
        };

        // Run the task, then immediately cancel it.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OfflineMeasurementsBackgroundTask task = new OfflineMeasurementsBackgroundTask();
            task.onStartTask(null, testParameters, testCallback);
            task.onStopTask(null, testParameters);
        });

        // Report the persisted metrics.
        maybeScheduleTaskAndReportMetrics();

        // Check HTTP probe results.
        assertEquals("The HTTP probe should have only been run once", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULT));
        assertEquals("The HTTP probe should have a result of CANCELLED", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULT,
                        OfflineMeasurementsBackgroundTask.ProbeResult.CANCELLED));

        // Check that the callback was not triggered.
        assertEquals("The task should not call the task finished callback", 0,
                mNumTaskFinishedCallbacksTriggered);
    }

    /**
     * Tests running the background task with airplane mode enabled and disabled. Checks that the
     * expected values are recorded to the UMA histogram Offline.Measurements.IsAirplaneModeEnabled.
     */
    @Test
    @MediumTest
    public void recordIsAirplaneModeEnabled() throws Exception {
        // Enable feature and initialize the HTTP probe parameters.
        setFeatureStatusForTest(true);

        // Set the task parameters.
        TaskParameters testParameters =
                TaskParameters.create(TaskIds.OFFLINE_MEASUREMENT_JOB_ID).build();
        BackgroundTask.TaskFinishedCallback testCallback = needsReschedule -> {};

        // Runs the task with airplane mode disabled, then runs it again with airplane mode enabled.
        OfflineMeasurementsBackgroundTask.setIsAirplaneModeEnabledForTesting(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OfflineMeasurementsBackgroundTask task = new OfflineMeasurementsBackgroundTask();
            task.onStartTask(null, testParameters, testCallback);
        });

        OfflineMeasurementsBackgroundTask.setIsAirplaneModeEnabledForTesting(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OfflineMeasurementsBackgroundTask task = new OfflineMeasurementsBackgroundTask();
            task.onStartTask(null, testParameters, testCallback);
        });

        // Reports the metrics stored in Prefs.
        maybeScheduleTaskAndReportMetrics();

        // Check histogram
        assertEquals("There should be one sample for each time the task was ran", 2,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OfflineMeasurementsBackgroundTask
                                .OFFLINE_MEASUREMENTS_IS_AIRPLANE_MODE_ENABLED));

        assertEquals("There should be one entry where airplane mode is disabled", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        OfflineMeasurementsBackgroundTask
                                .OFFLINE_MEASUREMENTS_IS_AIRPLANE_MODE_ENABLED,
                        /*false*/ 0));
        assertEquals("There should be one entry where airplane mode is enabled", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        OfflineMeasurementsBackgroundTask
                                .OFFLINE_MEASUREMENTS_IS_AIRPLANE_MODE_ENABLED,
                        /*true*/ 1));
    }

    /**
     * Tests running the background task when roaming and not roaming. Checks that the expected
     * values are recorded to the UMA histogram Offline.Measurements.IsRoaming.
     */
    @Test
    @MediumTest
    public void recordIsRoaming() throws Exception {
        // Enable feature and initialize the HTTP probe parameters.
        setFeatureStatusForTest(true);

        // Set the task parameters.
        TaskParameters testParameters =
                TaskParameters.create(TaskIds.OFFLINE_MEASUREMENT_JOB_ID).build();
        BackgroundTask.TaskFinishedCallback testCallback = needsReschedule -> {};

        // Runs the task while not roaming, then runs it again while roaming.
        OfflineMeasurementsBackgroundTask.setIsRoamingForTesting(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OfflineMeasurementsBackgroundTask task = new OfflineMeasurementsBackgroundTask();
            task.onStartTask(null, testParameters, testCallback);
        });

        OfflineMeasurementsBackgroundTask.setIsRoamingForTesting(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OfflineMeasurementsBackgroundTask task = new OfflineMeasurementsBackgroundTask();
            task.onStartTask(null, testParameters, testCallback);
        });

        // Reports the metrics stored in Prefs.
        maybeScheduleTaskAndReportMetrics();

        // Check histogram
        assertEquals("There should be one sample for each time the task was ran", 2,
                RecordHistogram.getHistogramTotalCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_IS_ROAMING));

        assertEquals("There should be one entry where not roaming", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_IS_ROAMING,
                        /*false*/ 0));
        assertEquals("There should be one entry where roaming", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        OfflineMeasurementsBackgroundTask.OFFLINE_MEASUREMENTS_IS_ROAMING,
                        /*true*/ 1));
    }
}

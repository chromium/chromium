// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.service;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;

import android.content.Context;
import android.os.Build;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.background_task_scheduler.ChromeNativeBackgroundTaskDelegate;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.DownloadNotificationService;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskInfo.NetworkType;
import org.chromium.components.background_task_scheduler.TaskInfo.OneOffInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.components.download.DownloadTaskType;

/** Unit tests for {@link org.chromium.chrome.browser.download.service.DownloadBackgroundTask}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
@EnableFeatures(ChromeFeatureList.DOWNLOADS_MIGRATE_TO_JOBS_API)
public class DownloadBackgroundTaskTest {
    @Rule public JniMocker mJniMocker = new JniMocker();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DownloadBackgroundTask.Natives mNativeMock;

    @Mock private ProfileKey mProfileKey;

    @Mock private DownloadManagerService mDownloadManagerService;
    @Mock private DownloadNotificationService mDownloadNotificationService;

    @Mock private Context mContext;
    @Mock private TaskFinishedCallback mTaskFinishedCallback;
    private TestDownloadBackgroundTask mTask;

    @Mock private BackgroundTaskScheduler mTaskScheduler;
    @Captor private ArgumentCaptor<TaskInfo> mTaskInfoCapture;

    private class TestDownloadBackgroundTask extends DownloadBackgroundTask {
        public TestDownloadBackgroundTask() {
            super();
            setDelegate(new ChromeNativeBackgroundTaskDelegate());
        }

        @Override
        protected ProfileKey getProfileKey() {
            return mProfileKey;
        }

        @Override
        protected void ensureNotificationBridgeInitialized() {}
    }

    private static TaskParameters getTaskParameters(int taskId) {
        return TaskParameters.create(taskId).build();
    }

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(DownloadBackgroundTaskJni.TEST_HOOKS, mNativeMock);
        DownloadManagerService.setDownloadManagerService(mDownloadManagerService);
        DownloadNotificationService.setInstanceForTests(mDownloadNotificationService);
        mTask = new TestDownloadBackgroundTask();
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mTaskScheduler);
    }

    @Test
    @Feature({"Download"})
    public void testNotificationCallbacksAreSetAndClearCorrectlyForUserInitiatedJobs() {
        TaskParameters taskParameters =
                getTaskParameters(TaskIds.DOWNLOAD_AUTO_RESUMPTION_UNMETERED_JOB_ID);
        mTask.onStartTask(mContext, taskParameters, mTaskFinishedCallback);
        mTask.onStartTaskBeforeNativeLoaded(mContext, taskParameters, mTaskFinishedCallback);
        mTask.onStartTaskWithNative(mContext, taskParameters, mTaskFinishedCallback);
        Mockito.verify(mDownloadNotificationService)
                .setBackgroundTaskNotificationCallback(eq(taskParameters.getTaskId()), any());

        Mockito.clearInvocations(mDownloadNotificationService);
        mTask.finishTask(taskParameters, mTaskFinishedCallback, false);
        Mockito.verify(mTaskFinishedCallback, times(1)).taskFinished(eq(false));
        Mockito.verify(mDownloadNotificationService)
                .setBackgroundTaskNotificationCallback(eq(taskParameters.getTaskId()), eq(null));

        Mockito.clearInvocations(mDownloadNotificationService);
        mTask.onStopTaskWithNative(mContext, taskParameters);
        Mockito.verify(mDownloadNotificationService)
                .setBackgroundTaskNotificationCallback(eq(taskParameters.getTaskId()), eq(null));
    }

    @Test
    @Feature({"Download"})
    public void testNotificationCallbacksNotSetForNonUserInitiatedJobs() {
        TaskParameters taskParameters = getTaskParameters(TaskIds.DOWNLOAD_AUTO_RESUMPTION_JOB_ID);
        mTask.onStartTask(mContext, taskParameters, mTaskFinishedCallback);
        mTask.onStartTaskBeforeNativeLoaded(mContext, taskParameters, mTaskFinishedCallback);
        mTask.onStartTaskWithNative(mContext, taskParameters, mTaskFinishedCallback);
        Mockito.verify(mDownloadNotificationService, times(0))
                .setBackgroundTaskNotificationCallback(eq(taskParameters.getTaskId()), any());

        Mockito.clearInvocations(mDownloadNotificationService);
        mTask.finishTask(taskParameters, mTaskFinishedCallback, false);
        Mockito.verify(mTaskFinishedCallback, times(1)).taskFinished(eq(false));
        Mockito.verify(mDownloadNotificationService, times(0))
                .setBackgroundTaskNotificationCallback(eq(taskParameters.getTaskId()), eq(null));

        Mockito.clearInvocations(mDownloadNotificationService);
        mTask.onStopTaskWithNative(mContext, taskParameters);
        Mockito.verify(mDownloadNotificationService, times(0))
                .setBackgroundTaskNotificationCallback(eq(taskParameters.getTaskId()), eq(null));
    }

    @Test
    @Feature({"Download"})
    public void testIsUserInitiatedJob() {
        Assert.assertTrue(
                DownloadUtils.isUserInitiatedJob(
                        TaskIds.DOWNLOAD_AUTO_RESUMPTION_UNMETERED_JOB_ID));
        Assert.assertTrue(
                DownloadUtils.isUserInitiatedJob(
                        TaskIds.DOWNLOAD_AUTO_RESUMPTION_ANY_NETWORK_JOB_ID));
        Assert.assertFalse(
                DownloadUtils.isUserInitiatedJob(TaskIds.DOWNLOAD_AUTO_RESUMPTION_JOB_ID));
        Assert.assertFalse(DownloadUtils.isUserInitiatedJob(TaskIds.DOWNLOAD_SERVICE_JOB_ID));
        Assert.assertFalse(DownloadUtils.isUserInitiatedJob(TaskIds.DOWNLOAD_CLEANUP_JOB_ID));
        Assert.assertFalse(DownloadUtils.isUserInitiatedJob(TaskIds.DOWNLOAD_LATER_JOB_ID));
    }

    @Test
    @Feature({"Download"})
    @Config(sdk = 30)
    public void testIsUserInitiatedJobForLowerAndroidVersions() {
        Assert.assertFalse(
                DownloadUtils.isUserInitiatedJob(
                        TaskIds.DOWNLOAD_AUTO_RESUMPTION_UNMETERED_JOB_ID));
        Assert.assertFalse(
                DownloadUtils.isUserInitiatedJob(
                        TaskIds.DOWNLOAD_AUTO_RESUMPTION_ANY_NETWORK_JOB_ID));
        Assert.assertFalse(
                DownloadUtils.isUserInitiatedJob(TaskIds.DOWNLOAD_AUTO_RESUMPTION_JOB_ID));
        Assert.assertFalse(DownloadUtils.isUserInitiatedJob(TaskIds.DOWNLOAD_SERVICE_JOB_ID));
        Assert.assertFalse(DownloadUtils.isUserInitiatedJob(TaskIds.DOWNLOAD_CLEANUP_JOB_ID));
        Assert.assertFalse(DownloadUtils.isUserInitiatedJob(TaskIds.DOWNLOAD_LATER_JOB_ID));
    }

    @Test
    @Feature({"Download"})
    @DisableFeatures(ChromeFeatureList.DOWNLOADS_MIGRATE_TO_JOBS_API)
    public void testIsUserInitiatedJobForDisabledFeature() {
        Assert.assertFalse(
                DownloadUtils.isUserInitiatedJob(
                        TaskIds.DOWNLOAD_AUTO_RESUMPTION_UNMETERED_JOB_ID));
        Assert.assertFalse(
                DownloadUtils.isUserInitiatedJob(
                        TaskIds.DOWNLOAD_AUTO_RESUMPTION_ANY_NETWORK_JOB_ID));
        Assert.assertFalse(
                DownloadUtils.isUserInitiatedJob(TaskIds.DOWNLOAD_AUTO_RESUMPTION_JOB_ID));
        Assert.assertFalse(DownloadUtils.isUserInitiatedJob(TaskIds.DOWNLOAD_SERVICE_JOB_ID));
        Assert.assertFalse(DownloadUtils.isUserInitiatedJob(TaskIds.DOWNLOAD_CLEANUP_JOB_ID));
        Assert.assertFalse(DownloadUtils.isUserInitiatedJob(TaskIds.DOWNLOAD_LATER_JOB_ID));
    }

    @Test
    @Feature({"Download"})
    public void testScheduleTaskForUITask() {
        int taskType = DownloadTaskType.DOWNLOAD_AUTO_RESUMPTION_UNMETERED_TASK;
        DownloadTaskScheduler.scheduleTask(taskType, true, false, 0, 60, 120);
        Mockito.verify(mTaskScheduler, times(1)).schedule(any(), mTaskInfoCapture.capture());
        TaskInfo taskInfo = mTaskInfoCapture.getValue();

        Assert.assertTrue(taskInfo.isUserInitiated());
        Assert.assertEquals(
                TaskIds.DOWNLOAD_AUTO_RESUMPTION_UNMETERED_JOB_ID, taskInfo.getTaskId());
        Assert.assertEquals(NetworkType.UNMETERED, taskInfo.getRequiredNetworkType());
        Assert.assertFalse(taskInfo.requiresCharging());
        Assert.assertTrue(taskInfo.getTimingInfo() instanceof OneOffInfo);
        OneOffInfo oneOffInfo = (OneOffInfo) taskInfo.getTimingInfo();
        Assert.assertFalse(oneOffInfo.hasWindowStartTimeConstraint());
        Assert.assertFalse(oneOffInfo.hasWindowEndTimeConstraint());
    }

    @Test
    @Feature({"Download"})
    public void testScheduleTaskForNonUITask() {
        int taskType = DownloadTaskType.DOWNLOAD_AUTO_RESUMPTION_TASK;
        DownloadTaskScheduler.scheduleTask(taskType, true, false, 0, 60, 120);
        Mockito.verify(mTaskScheduler, times(1)).schedule(any(), mTaskInfoCapture.capture());
        TaskInfo taskInfo = mTaskInfoCapture.getValue();

        Assert.assertFalse(taskInfo.isUserInitiated());
        Assert.assertEquals(TaskIds.DOWNLOAD_AUTO_RESUMPTION_JOB_ID, taskInfo.getTaskId());
        Assert.assertEquals(NetworkType.UNMETERED, taskInfo.getRequiredNetworkType());
        Assert.assertFalse(taskInfo.requiresCharging());
        Assert.assertTrue(taskInfo.getTimingInfo() instanceof OneOffInfo);
        OneOffInfo oneOffInfo = (OneOffInfo) taskInfo.getTimingInfo();
        Assert.assertTrue(oneOffInfo.hasWindowStartTimeConstraint());
        Assert.assertTrue(oneOffInfo.hasWindowEndTimeConstraint());
        Assert.assertEquals(60000, oneOffInfo.getWindowStartTimeMs());
        Assert.assertEquals(120000, oneOffInfo.getWindowEndTimeMs());
    }

    @Test
    @Feature({"Download"})
    public void testCancelTask() {
        int taskType = DownloadTaskType.DOWNLOAD_AUTO_RESUMPTION_TASK;
        DownloadTaskScheduler.cancelTask(taskType);
        Mockito.verify(mTaskScheduler, times(1))
                .cancel(any(), eq(TaskIds.DOWNLOAD_AUTO_RESUMPTION_JOB_ID));
    }
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.service;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;

import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.background_task_scheduler.ChromeNativeBackgroundTaskDelegate;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.DownloadNotificationService;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;

/** Unit tests for {@link org.chromium.chrome.browser.download.service.DownloadBackgroundTask}. */
@RunWith(BaseRobolectricTestRunner.class)
// TODO(crbug/1483735): Update this to 34 once robolectric support is added.
@Config(manifest = Config.NONE, sdk = 33)
@EnableFeatures(ChromeFeatureList.DOWNLOADS_MIGRATE_TO_JOBS_API)
public class DownloadBackgroundTaskTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private DownloadBackgroundTask.Natives mNativeMock;

    @Mock
    private ProfileKey mProfileKey;

    @Mock
    private DownloadManagerService mDownloadManagerService;
    @Mock
    private DownloadNotificationService mDownloadNotificationService;

    @Mock
    private Context mContext;
    @Mock
    private TaskFinishedCallback mTaskFinishedCallback;
    private TestDownloadBackgroundTask mTask;

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
        DownloadUtils.setMinSdkVersionForUserInitiatedJobsForTesting(33);
        mTask = new TestDownloadBackgroundTask();
        TaskParameters taskParameters =
                getTaskParameters(TaskIds.DOWNLOAD_AUTO_RESUMPTION_UNMETERED_JOB_ID);
        mTask.onStartTask(mContext, taskParameters, mTaskFinishedCallback);
        Assert.assertTrue(mTask.isUserInitiatedJob());

        mTask = new TestDownloadBackgroundTask();
        taskParameters = getTaskParameters(TaskIds.DOWNLOAD_AUTO_RESUMPTION_JOB_ID);
        mTask.onStartTask(mContext, taskParameters, mTaskFinishedCallback);
        Assert.assertFalse(mTask.isUserInitiatedJob());

        mTask = new TestDownloadBackgroundTask();
        taskParameters = getTaskParameters(TaskIds.DOWNLOAD_SERVICE_JOB_ID);
        mTask.onStartTask(mContext, taskParameters, mTaskFinishedCallback);
        Assert.assertFalse(mTask.isUserInitiatedJob());
    }

    @Test
    @Feature({"Download"})
    @Config(sdk = 30)
    public void testIsUserInitiatedJobForLowerAndroidVersions() {
        mTask = new TestDownloadBackgroundTask();
        TaskParameters taskParameters =
                getTaskParameters(TaskIds.DOWNLOAD_AUTO_RESUMPTION_UNMETERED_JOB_ID);
        mTask.onStartTask(mContext, taskParameters, mTaskFinishedCallback);
        Assert.assertFalse(mTask.isUserInitiatedJob());

        mTask = new TestDownloadBackgroundTask();
        taskParameters = getTaskParameters(TaskIds.DOWNLOAD_AUTO_RESUMPTION_JOB_ID);
        mTask.onStartTask(mContext, taskParameters, mTaskFinishedCallback);
        Assert.assertFalse(mTask.isUserInitiatedJob());

        mTask = new TestDownloadBackgroundTask();
        taskParameters = getTaskParameters(TaskIds.DOWNLOAD_SERVICE_JOB_ID);
        mTask.onStartTask(mContext, taskParameters, mTaskFinishedCallback);
        Assert.assertFalse(mTask.isUserInitiatedJob());
    }
}

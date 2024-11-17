// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.filters.SmallTest;

import com.google.android.gms.gcm.TaskParams;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.background_sync.BackgroundSyncBackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

/** Tests {@link ChromeBackgroundService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeBackgroundServiceTest {
    private MockTaskService mTaskService;

    static class MockTaskService extends ChromeBackgroundServiceImpl {
        private boolean mDidLaunchBrowser;
        private boolean mDidCallOnPersistentSchedulerWakeUp;
        private boolean mDidCallOnBrowserUpgraded;

        @Mock private BackgroundTaskScheduler mTaskScheduler;

        @Override
        protected void launchBrowser(Context context, String tag) {
            mDidLaunchBrowser = true;
        }

        @Override
        protected void rescheduleBackgroundSyncTasksOnUpgrade() {}

        // Posts an assertion task to the UI thread. Since this is only called after the call
        // to onRunTask, it will be enqueued after any possible call to launchBrowser, and we
        // can reliably check whether launchBrowser was called.
        protected void checkExpectations(
                final boolean expectedLaunchBrowser,
                final boolean expectedDidCallOnPersistentSchedulerWakeUp,
                final boolean expectedDidCallOnBrowserUpgraded) {
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        Assert.assertEquals(
                                "StartedService", expectedLaunchBrowser, mDidLaunchBrowser);
                        Assert.assertEquals(
                                "OnPersistentSchedulerWakeUp",
                                expectedDidCallOnPersistentSchedulerWakeUp,
                                mDidCallOnPersistentSchedulerWakeUp);
                        Assert.assertEquals(
                                "OnBrowserUpgraded",
                                expectedDidCallOnBrowserUpgraded,
                                mDidCallOnBrowserUpgraded);
                    });
        }

        protected void setUpMocks() {
            mTaskScheduler = Mockito.mock(BackgroundTaskScheduler.class);
            BackgroundTaskSchedulerFactory.setSchedulerForTesting(mTaskScheduler);
            doReturn(true).when(mTaskScheduler).schedule(any(Context.class), any(TaskInfo.class));
        }

        protected void checkBackgroundTaskSchedulerInvocation(int taskId) {
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        verify(mTaskScheduler)
                                .schedule(
                                        any(Context.class),
                                        argThat(taskInfo -> taskInfo.getTaskId() == taskId));
                    });
        }
    }

    @Before
    public void setUp() {
        mTaskService = new MockTaskService();
        mTaskService.setUpMocks();
    }

    private void startOnRunTaskAndVerify(
            String taskTag, boolean shouldStart, boolean shouldCallOnPersistentSchedulerWakeUp) {
        mTaskService.onRunTask(new TaskParams(taskTag));
        mTaskService.checkExpectations(shouldStart, shouldCallOnPersistentSchedulerWakeUp, false);
    }

    @Test
    @SmallTest
    @Feature({"BackgroundSync"})
    public void testBackgroundSyncRescheduleWhenTaskRuns() {
        mTaskService.onRunTask(new TaskParams(BackgroundSyncBackgroundTaskScheduler.TASK_TAG));
        mTaskService.checkBackgroundTaskSchedulerInvocation(
                TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID);
    }

    private void startOnInitializeTasksAndVerify(
            boolean shouldStart, boolean shouldCallOnBrowserUpgraded) {
        mTaskService.onInitializeTasks();
        mTaskService.checkExpectations(shouldStart, false, shouldCallOnBrowserUpgraded);
    }
}

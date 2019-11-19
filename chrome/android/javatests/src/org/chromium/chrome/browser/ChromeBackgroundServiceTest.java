// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.support.test.filters.SmallTest;

import com.google.android.gms.gcm.TaskParams;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.background_sync.BackgroundSyncBackgroundTask;
import org.chromium.chrome.browser.background_sync.BackgroundSyncBackgroundTaskScheduler;
import org.chromium.chrome.browser.ntp.snippets.SnippetsLauncher;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Tests {@link ChromeBackgroundService}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
public class ChromeBackgroundServiceTest {
    private SnippetsLauncher mSnippetsLauncher;
    private MockTaskService mTaskService;

    class MockTaskService extends ChromeBackgroundService {
        private boolean mDidLaunchBrowser;
        private boolean mDidCallOnPersistentSchedulerWakeUp;
        private boolean mDidCallOnBrowserUpgraded;

        @Mock
        private BackgroundTaskScheduler mTaskScheduler;

        @Override
        protected void launchBrowser(Context context, String tag) {
            mDidLaunchBrowser = true;
        }

        @Override
        protected void snippetsOnPersistentSchedulerWakeUp() {
            mDidCallOnPersistentSchedulerWakeUp = true;
        }

        @Override
        protected void snippetsOnBrowserUpgraded() {
            mDidCallOnBrowserUpgraded = true;
        }

        @Override
        protected void rescheduleBackgroundSyncTasksOnUpgrade() {}

        // Posts an assertion task to the UI thread. Since this is only called after the call
        // to onRunTask, it will be enqueued after any possible call to launchBrowser, and we
        // can reliably check whether launchBrowser was called.
        protected void checkExpectations(final boolean expectedLaunchBrowser,
                final boolean expectedDidCallOnPersistentSchedulerWakeUp,
                final boolean expectedDidCallOnBrowserUpgraded) {
            PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
                Assert.assertEquals("StartedService", expectedLaunchBrowser, mDidLaunchBrowser);
                Assert.assertEquals("OnPersistentSchedulerWakeUp",
                        expectedDidCallOnPersistentSchedulerWakeUp,
                        mDidCallOnPersistentSchedulerWakeUp);
                Assert.assertEquals("OnBrowserUpgraded", expectedDidCallOnBrowserUpgraded,
                        mDidCallOnBrowserUpgraded);
            });
        }

        protected void setUpMocks() {
            mTaskScheduler = Mockito.mock(BackgroundTaskScheduler.class);
            BackgroundTaskSchedulerFactory.setSchedulerForTesting(mTaskScheduler);
            doReturn(true).when(mTaskScheduler).schedule(any(Context.class), any(TaskInfo.class));
        }

        protected void checkBackgroundTaskSchedulerInvocation(
                Class<? extends BackgroundTask> backgroundTaskClass) {
            PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
                verify(mTaskScheduler)
                        .schedule(any(Context.class),
                                argThat(taskInfo
                                        -> taskInfo.getBackgroundTaskClass()
                                                == backgroundTaskClass));
            });
        }
    }

    @Before
    public void setUp() {
        RecordHistogram.setDisabledForTests(true);
        mSnippetsLauncher = SnippetsLauncher.create();
        mTaskService = new MockTaskService();
        mTaskService.setUpMocks();
    }

    @After
    public void tearDown() {
        RecordHistogram.setDisabledForTests(false);
    }

    private void deleteSnippetsLauncherInstance() {
        mSnippetsLauncher.destroy();
        mSnippetsLauncher = null;
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
        mTaskService.checkBackgroundTaskSchedulerInvocation(BackgroundSyncBackgroundTask.class);
    }

    @Test
    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchWifiNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_WIFI, false, true);
    }

    @Test
    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchFallbackNoLaunchBrowserWhenInstanceExists() {
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_FALLBACK, false, true);
    }

    @Test
    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchWifiLaunchBrowserWhenInstanceDoesNotExist() {
        deleteSnippetsLauncherInstance();
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_WIFI, true, true);
    }

    @Test
    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsFetchFallbackLaunchBrowserWhenInstanceDoesNotExist() {
        deleteSnippetsLauncherInstance();
        startOnRunTaskAndVerify(SnippetsLauncher.TASK_TAG_FALLBACK, true, true);
    }

    private void startOnInitializeTasksAndVerify(
            boolean shouldStart, boolean shouldCallOnBrowserUpgraded) {
        mTaskService.onInitializeTasks();
        mTaskService.checkExpectations(shouldStart, false, shouldCallOnBrowserUpgraded);
    }

    @Test
    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsNoRescheduleWithoutPrefWhenInstanceExists() {
        startOnInitializeTasksAndVerify(
                /*shouldStart=*/false, /*shouldCallOnBrowserUpgraded=*/false);
    }

    @Test
    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsNoRescheduleWithoutPrefWhenInstanceDoesNotExist() {
        deleteSnippetsLauncherInstance();
        startOnInitializeTasksAndVerify(
                /*shouldStart=*/false, /*shouldCallOnBrowserUpgraded=*/false);
    }

    @Test
    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsRescheduleWithPrefWhenInstanceExists() {
        // Set the pref indicating that fetching was scheduled before.
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(SnippetsLauncher.PREF_IS_SCHEDULED, true)
                .apply();

        startOnInitializeTasksAndVerify(
                /*shouldStart=*/false, /*shouldCallOnBrowserUpgraded=*/true);
    }

    @Test
    @SmallTest
    @Feature({"NTPSnippets"})
    public void testNTPSnippetsRescheduleAndLaunchBrowserWithPrefWhenInstanceDoesNotExist() {
        deleteSnippetsLauncherInstance();
        // Set the pref indicating that fetching was scheduled before.
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(SnippetsLauncher.PREF_IS_SCHEDULED, true)
                .apply();

        startOnInitializeTasksAndVerify(/*shouldStart=*/true, /*shouldCallOnBrowserUpgraded=*/true);
    }
}

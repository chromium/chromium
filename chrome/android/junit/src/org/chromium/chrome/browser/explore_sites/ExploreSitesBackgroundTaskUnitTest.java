// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.mockito.invocation.InvocationOnMock;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.multidex.ShadowMultiDex;

import org.chromium.base.Callback;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.DeviceConditions;
import org.chromium.chrome.browser.ShadowDeviceConditions;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.net.ConnectionType;

import java.util.HashMap;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link ExploreSitesBackgroundTask}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowMultiDex.class, ShadowDeviceConditions.class, ShadowRecordHistogram.class,
                ExploreSitesBackgroundTaskUnitTest.ShadowExploreSitesBridge.class})
public class ExploreSitesBackgroundTaskUnitTest {
    /** Implementation of ExploreSitesBridge which does not rely on native. */
    @Implements(ExploreSitesBridge.class)
    public static class ShadowExploreSitesBridge {
        public static Callback<Void> mUpdateCatalogFinishedCallback;
        public static int mVariation = ExploreSitesVariation.ENABLED;

        @Implementation
        public static void getEspCatalog(
                Profile profile, Callback<List<ExploreSitesCategory>> callback) {}

        @Implementation
        public static void updateCatalogFromNetwork(
                Profile profile, boolean isImmediateFetch, Callback<Void> finishedCallback) {
            mUpdateCatalogFinishedCallback = finishedCallback;
        }

        @Implementation
        @ExploreSitesVariation
        public static int getVariation() {
            return mVariation;
        }
    }

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
        public void checkForOSUpgrade(Context context) {}

        @Override
        public void reschedule(Context context) {}

        public TaskInfo getTaskInfo(int taskId) {
            return mTaskInfos.get(taskId);
        }
    }

    void initDeviceConditions(@ConnectionType int connectionType) {
        boolean powerConnected = true;
        boolean powerSaveModeOn = true;
        int highBatteryLevel = 75;
        boolean metered = true;
        boolean screenOnAndUnlocked = true;

        DeviceConditions deviceConditions = new DeviceConditions(!powerConnected, highBatteryLevel,
                connectionType, !powerSaveModeOn, !metered, screenOnAndUnlocked);
        ShadowDeviceConditions.setCurrentConditions(deviceConditions);
    }

    @Spy
    private ExploreSitesBackgroundTask mExploreSitesBackgroundTask =
            new ExploreSitesBackgroundTask();
    @Mock
    private ChromeBrowserInitializer mChromeBrowserInitializer;
    @Captor
    ArgumentCaptor<BrowserParts> mBrowserParts;
    private FakeBackgroundTaskScheduler mFakeTaskScheduler;

    public void disableExploreSites() {
        ShadowExploreSitesBridge.mVariation = ExploreSitesVariation.DISABLED;
    }

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        MockitoAnnotations.initMocks(this);
        doNothing().when(mChromeBrowserInitializer).handlePreNativeStartup(any(BrowserParts.class));
        doAnswer((InvocationOnMock invocation) -> {
            mBrowserParts.getValue().finishNativeInitialization();
            return null;
        })
                .when(mChromeBrowserInitializer)
                .handlePostNativeStartup(eq(true), mBrowserParts.capture());

        ChromeBrowserInitializer.setForTesting(mChromeBrowserInitializer);

        mFakeTaskScheduler = new FakeBackgroundTaskScheduler();
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mFakeTaskScheduler);
        doReturn(null).when(mExploreSitesBackgroundTask).getProfile();

        ShadowExploreSitesBridge.mVariation = ExploreSitesVariation.ENABLED;
    }

    @Test
    public void scheduleTask() {
        ExploreSitesBackgroundTask.schedule(false /* updateCurrent */);
        TaskInfo scheduledTask =
                mFakeTaskScheduler.getTaskInfo(TaskIds.EXPLORE_SITES_REFRESH_JOB_ID);
        assertNotNull(scheduledTask);
        assertEquals(TimeUnit.HOURS.toMillis(ExploreSitesBackgroundTask.DEFAULT_DELAY_HOURS),
                scheduledTask.getPeriodicInfo().getIntervalMs());
        assertEquals(true, scheduledTask.isPersisted());
        assertEquals(TaskInfo.NetworkType.ANY, scheduledTask.getRequiredNetworkType());
    }

    @Test
    public void cancelTask() {
        TaskInfo scheduledTask =
                mFakeTaskScheduler.getTaskInfo(TaskIds.EXPLORE_SITES_REFRESH_JOB_ID);
        assertNull(scheduledTask);

        ExploreSitesBackgroundTask.schedule(false /* updateCurrent */);
        scheduledTask = mFakeTaskScheduler.getTaskInfo(TaskIds.EXPLORE_SITES_REFRESH_JOB_ID);
        assertNotNull(scheduledTask);
        assertEquals(TimeUnit.HOURS.toMillis(ExploreSitesBackgroundTask.DEFAULT_DELAY_HOURS),
                scheduledTask.getPeriodicInfo().getIntervalMs());

        ExploreSitesBackgroundTask.cancelTask();
        scheduledTask = mFakeTaskScheduler.getTaskInfo(TaskIds.EXPLORE_SITES_REFRESH_JOB_ID);
        assertNull(scheduledTask);
    }

    @Test
    public void testNoNetwork() {
        initDeviceConditions(ConnectionType.CONNECTION_NONE);
        TaskParameters params = TaskParameters.create(TaskIds.EXPLORE_SITES_REFRESH_JOB_ID).build();

        int result = mExploreSitesBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, (boolean needsReschedule) -> {
                    fail("Finished callback should not be run, network conditions not met.");
                });
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.RESCHEDULE, result);
    }

    @Test
    public void testRemovesDeprecatedJobId() {
        TaskInfo.Builder deprecatedTaskInfoBuilder =
                TaskInfo.createPeriodicTask(TaskIds.DEPRECATED_EXPLORE_SITES_REFRESH_JOB_ID,
                                ExploreSitesBackgroundTask.class, TimeUnit.HOURS.toMillis(4),
                                TimeUnit.HOURS.toMillis(1))
                        .setRequiredNetworkType(TaskInfo.NetworkType.ANY)
                        .setIsPersisted(true)
                        .setUpdateCurrent(false);
        mFakeTaskScheduler.schedule(
                RuntimeEnvironment.application, deprecatedTaskInfoBuilder.build());
        TaskInfo deprecatedTask =
                mFakeTaskScheduler.getTaskInfo(TaskIds.DEPRECATED_EXPLORE_SITES_REFRESH_JOB_ID);
        assertNotNull(deprecatedTask);

        ExploreSitesBackgroundTask.schedule(false /* updateCurrent */);
        TaskInfo scheduledTask =
                mFakeTaskScheduler.getTaskInfo(TaskIds.EXPLORE_SITES_REFRESH_JOB_ID);
        assertNotNull(scheduledTask);

        deprecatedTask =
                mFakeTaskScheduler.getTaskInfo(TaskIds.DEPRECATED_EXPLORE_SITES_REFRESH_JOB_ID);
        assertNull(deprecatedTask);
    }

    @Test
    public void testRemovesTaskIfFeatureIsDisabled() {
        disableExploreSites();

        TaskInfo.Builder taskInfoBuilder =
                TaskInfo.createPeriodicTask(TaskIds.EXPLORE_SITES_REFRESH_JOB_ID,
                                ExploreSitesBackgroundTask.class, TimeUnit.HOURS.toMillis(4),
                                TimeUnit.HOURS.toMillis(1))
                        .setRequiredNetworkType(TaskInfo.NetworkType.ANY)
                        .setIsPersisted(true)
                        .setUpdateCurrent(false);
        mFakeTaskScheduler.schedule(RuntimeEnvironment.application, taskInfoBuilder.build());
        TaskInfo task = mFakeTaskScheduler.getTaskInfo(TaskIds.EXPLORE_SITES_REFRESH_JOB_ID);
        assertNotNull(task);

        TaskParameters params = TaskParameters.create(TaskIds.EXPLORE_SITES_REFRESH_JOB_ID).build();
        mExploreSitesBackgroundTask.onStartTaskWithNative(
                RuntimeEnvironment.application, params, (boolean needsReschedule) -> {
                    fail("Finished callback should not be run, the task should be cancelled.");
                });

        TaskInfo scheduledTask =
                mFakeTaskScheduler.getTaskInfo(TaskIds.EXPLORE_SITES_REFRESH_JOB_ID);
        assertNull(scheduledTask);
    }

    @Test
    public void testDoesNotRemoveTaskIfFeatureIsEnabled() {
        TaskInfo.Builder taskInfoBuilder =
                TaskInfo.createPeriodicTask(TaskIds.EXPLORE_SITES_REFRESH_JOB_ID,
                                ExploreSitesBackgroundTask.class, TimeUnit.HOURS.toMillis(4),
                                TimeUnit.HOURS.toMillis(1))
                        .setRequiredNetworkType(TaskInfo.NetworkType.ANY)
                        .setIsPersisted(true)
                        .setUpdateCurrent(false);
        mFakeTaskScheduler.schedule(RuntimeEnvironment.application, taskInfoBuilder.build());
        TaskInfo task = mFakeTaskScheduler.getTaskInfo(TaskIds.EXPLORE_SITES_REFRESH_JOB_ID);
        assertNotNull(task);

        TaskParameters params = TaskParameters.create(TaskIds.EXPLORE_SITES_REFRESH_JOB_ID).build();
        mExploreSitesBackgroundTask.onStartTaskWithNative(RuntimeEnvironment.application, params,
                (boolean needsReschedule) -> { fail("Finished callback should not be run"); });

        TaskInfo scheduledTask =
                mFakeTaskScheduler.getTaskInfo(TaskIds.EXPLORE_SITES_REFRESH_JOB_ID);
        assertNotNull(scheduledTask);
    }
}

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
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.offlinepages.DeviceConditions;
import org.chromium.chrome.browser.offlinepages.ShadowDeviceConditions;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.net.ConnectionType;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link ExploreSitesBackgroundTask}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowMultiDex.class, ShadowDeviceConditions.class,
                ExploreSitesBackgroundTaskUnitTest.ShadowExploreSitesBridge.class})
public class ExploreSitesBackgroundTaskUnitTest {
    /** Implementation of ExploreSitesBridge which does not rely on native. */
    @Implements(ExploreSitesBridge.class)
    public static class ShadowExploreSitesBridge {
        public static Callback<Void> mUpdateCatalogFinishedCallback;
        @Implementation
        public static void getEspCatalog(
                Profile profile, Callback<List<ExploreSitesCategory>> callback) {}

        @Implementation
        public static void updateCatalogFromNetwork(
                Profile profile, boolean isImmediateFetch, Callback<Void> finishedCallback) {
            mUpdateCatalogFinishedCallback = finishedCallback;
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

        DeviceConditions deviceConditions = new DeviceConditions(
                !powerConnected, highBatteryLevel, connectionType, !powerSaveModeOn, !metered);
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

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doNothing().when(mChromeBrowserInitializer).handlePreNativeStartup(any(BrowserParts.class));
        try {
            doAnswer((InvocationOnMock invocation) -> {
                mBrowserParts.getValue().finishNativeInitialization();
                return null;
            })
                    .when(mChromeBrowserInitializer)
                    .handlePostNativeStartup(eq(true), mBrowserParts.capture());
        } catch (ProcessInitException ex) {
            fail("Unexpected exception while initializing mock of ChromeBrowserInitializer.");
        }

        ChromeBrowserInitializer.setForTesting(mChromeBrowserInitializer);

        mFakeTaskScheduler = new FakeBackgroundTaskScheduler();
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mFakeTaskScheduler);
        doReturn(null).when(mExploreSitesBackgroundTask).getProfile();
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
        assertEquals(TaskInfo.NETWORK_TYPE_ANY, scheduledTask.getRequiredNetworkType());
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
    public void testNoNetwork() throws Exception {
        initDeviceConditions(ConnectionType.CONNECTION_NONE);
        TaskParameters params = TaskParameters.create(TaskIds.EXPLORE_SITES_REFRESH_JOB_ID).build();

        int result = mExploreSitesBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, (boolean needsReschedule) -> {
                    fail("Finished callback should not be run, network conditions not met.");
                });
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.RESCHEDULE, result);
    }

    @Test
    public void testWithNetwork() throws Exception {
        initDeviceConditions(ConnectionType.CONNECTION_2G);
        TaskParameters params = TaskParameters.create(TaskIds.EXPLORE_SITES_REFRESH_JOB_ID).build();

        final ArrayList<Boolean> taskFinishedList = new ArrayList<>();
        mExploreSitesBackgroundTask.onStartTask(RuntimeEnvironment.application, params,
                (boolean needsReschedule) -> { taskFinishedList.add(needsReschedule); });

        // Simulate update finishing from the native side.
        ShadowExploreSitesBridge.mUpdateCatalogFinishedCallback.onResult(null);
        assertEquals(1, taskFinishedList.size());
        assertEquals(false, taskFinishedList.get(0));
    }
}

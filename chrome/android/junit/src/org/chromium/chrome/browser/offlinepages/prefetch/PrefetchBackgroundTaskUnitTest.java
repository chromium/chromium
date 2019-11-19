// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

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
import android.os.Bundle;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.multidex.ShadowMultiDex;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.DeviceConditions;
import org.chromium.chrome.browser.ShadowDeviceConditions;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.net.ConnectionType;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link PrefetchBackgroundTask}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowMultiDex.class, ShadowDeviceConditions.class})
public class PrefetchBackgroundTaskUnitTest {
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

        public void clear() {
            mTaskInfos = new HashMap<>();
        }
    }
    public static final boolean POWER_CONNECTED = true;
    public static final boolean POWER_SAVE_MODE_ON = true;
    public static final int HIGH_BATTERY_LEVEL = 75;
    public static final int LOW_BATTERY_LEVEL = 25;
    public static final boolean METERED = true;
    public static final boolean SCREEN_ON_AND_UNLOCKED = true;

    @Rule
    public JniMocker mocker = new JniMocker();
    @Spy
    private PrefetchBackgroundTask mPrefetchBackgroundTask = new PrefetchBackgroundTask();
    @Mock
    private ChromeBrowserInitializer mChromeBrowserInitializer;
    @Mock
    private PrefetchBackgroundTask.Natives mPrefetchBackgroundTaskJniMock;
    @Captor
    ArgumentCaptor<BrowserParts> mBrowserParts;
    private FakeBackgroundTaskScheduler mFakeTaskScheduler;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(PrefetchBackgroundTaskJni.TEST_HOOKS, mPrefetchBackgroundTaskJniMock);
        doNothing().when(mChromeBrowserInitializer).handlePreNativeStartup(any(BrowserParts.class));
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                mBrowserParts.getValue().finishNativeInitialization();
                return null;
            }
        })
                .when(mChromeBrowserInitializer)
                .handlePostNativeStartup(eq(true), mBrowserParts.capture());

        ChromeBrowserInitializer.setForTesting(mChromeBrowserInitializer);

        doAnswer(new Answer() {
            @Override
            public Object answer(InvocationOnMock invocation) {
                mPrefetchBackgroundTask.setNativeTask(1);
                return Boolean.TRUE;
            }
        })
                .when(mPrefetchBackgroundTaskJniMock)
                .startPrefetchTask(mPrefetchBackgroundTask);

        doReturn(true).when(mPrefetchBackgroundTaskJniMock).onStopTask(1, mPrefetchBackgroundTask);

        mFakeTaskScheduler = new FakeBackgroundTaskScheduler();
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mFakeTaskScheduler);
    }

    @Test
    public void scheduleTask() {
        final int additionalDelaySeconds = 15;
        PrefetchBackgroundTaskScheduler.scheduleTask(additionalDelaySeconds);
        TaskInfo scheduledTask =
                mFakeTaskScheduler.getTaskInfo(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
        assertNotNull(scheduledTask);
        assertEquals(TimeUnit.SECONDS.toMillis(
                             PrefetchBackgroundTaskScheduler.DEFAULT_START_DELAY_SECONDS
                             + additionalDelaySeconds),
                scheduledTask.getOneOffInfo().getWindowStartTimeMs());
        assertEquals(true, scheduledTask.isPersisted());
        assertEquals(TaskInfo.NetworkType.UNMETERED, scheduledTask.getRequiredNetworkType());
    }

    /**
     * Tests that the background task is scheduled when limitless prefetching is enabled:
     * the waiting delay is shorter but the provided backoff time should be respected.
     */
    @Test
    public void scheduleTaskLimitless() {
        final int additionalDelaySeconds = 20;
        PrefetchBackgroundTaskScheduler.scheduleTaskLimitless(additionalDelaySeconds);
        TaskInfo scheduledTask =
                mFakeTaskScheduler.getTaskInfo(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
        assertNotNull(scheduledTask);
        assertEquals(TimeUnit.SECONDS.toMillis(
                             PrefetchBackgroundTaskScheduler.LIMITLESS_START_DELAY_SECONDS
                             + additionalDelaySeconds),
                scheduledTask.getOneOffInfo().getWindowStartTimeMs());
        assertEquals(true, scheduledTask.isPersisted());
        assertEquals(TaskInfo.NetworkType.ANY, scheduledTask.getRequiredNetworkType());
    }

    @Test
    public void cancelTask() {
        TaskInfo scheduledTask =
                mFakeTaskScheduler.getTaskInfo(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
        assertNull(scheduledTask);

        PrefetchBackgroundTaskScheduler.scheduleTask(0);
        scheduledTask = mFakeTaskScheduler.getTaskInfo(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
        assertNotNull(scheduledTask);
        assertEquals(TimeUnit.SECONDS.toMillis(
                             PrefetchBackgroundTaskScheduler.DEFAULT_START_DELAY_SECONDS),
                scheduledTask.getOneOffInfo().getWindowStartTimeMs());

        PrefetchBackgroundTaskScheduler.cancelTask();
        scheduledTask = mFakeTaskScheduler.getTaskInfo(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
        assertNull(scheduledTask);
    }

    @Test
    public void createNativeTask() {
        final ArrayList<Boolean> reschedules = new ArrayList<>();
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID).build();

        // Setup battery conditions with no power connected.
        DeviceConditions deviceConditions = new DeviceConditions(!POWER_CONNECTED,
                HIGH_BATTERY_LEVEL - 1, ConnectionType.CONNECTION_WIFI, !POWER_SAVE_MODE_ON,
                !METERED, SCREEN_ON_AND_UNLOCKED);
        ShadowDeviceConditions.setCurrentConditions(deviceConditions);

        mPrefetchBackgroundTask.onStartTask(null, params, new TaskFinishedCallback() {
            @Override
            public void taskFinished(boolean needsReschedule) {
                reschedules.add(needsReschedule);
            }
        });
        mPrefetchBackgroundTask.doneProcessing(false);

        assertEquals(1, reschedules.size());
        assertEquals(false, reschedules.get(0));
    }

    /**
     * Tests that the background task is correctly started when conditions are sufficient for
     * limitless prefetching.
     */
    @Test
    public void createNativeTaskLimitless() {
        final ArrayList<Boolean> reschedules = new ArrayList<>();
        Bundle extrasBundle = new Bundle();
        extrasBundle.putBoolean(PrefetchBackgroundTask.LIMITLESS_BUNDLE_KEY, true);
        TaskParameters params = TaskParameters.create(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID)
                                        .addExtras(extrasBundle)
                                        .build();

        // Setup battery conditions with no power connected.
        DeviceConditions deviceConditions = new DeviceConditions(!POWER_CONNECTED,
                0 /* battery level */, ConnectionType.CONNECTION_2G, !POWER_SAVE_MODE_ON, METERED,
                SCREEN_ON_AND_UNLOCKED);
        ShadowDeviceConditions.setCurrentConditions(deviceConditions);

        mPrefetchBackgroundTask.onStartTask(null, params, new TaskFinishedCallback() {
            @Override
            public void taskFinished(boolean needsReschedule) {
                reschedules.add(needsReschedule);
            }
        });
        mPrefetchBackgroundTask.doneProcessing(false);

        assertEquals(1, reschedules.size());
        assertEquals(false, reschedules.get(0));
    }

    @Test
    public void testBatteryLow() {
        // Setup low battery conditions with no power connected.
        DeviceConditions deviceConditionsLowBattery = new DeviceConditions(!POWER_CONNECTED,
                LOW_BATTERY_LEVEL, ConnectionType.CONNECTION_WIFI, !POWER_SAVE_MODE_ON, METERED,
                SCREEN_ON_AND_UNLOCKED);
        ShadowDeviceConditions.setCurrentConditions(deviceConditionsLowBattery);

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID).build();

        int result = mPrefetchBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        fail("Finished callback should not be run, battery conditions not met.");
                    }
                });
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.RESCHEDULE, result);
    }

    @Test
    public void testBatteryHigh() {
        // Setup high battery conditions with no power connected.
        DeviceConditions deviceConditionsHighBattery = new DeviceConditions(!POWER_CONNECTED,
                HIGH_BATTERY_LEVEL, ConnectionType.CONNECTION_WIFI, !POWER_SAVE_MODE_ON, !METERED,
                SCREEN_ON_AND_UNLOCKED);
        ShadowDeviceConditions.setCurrentConditions(deviceConditionsHighBattery);

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID).build();

        int result = mPrefetchBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        // Nothing to do.
                    }
                });
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.LOAD_NATIVE, result);
    }

    @Test
    public void testNoNetwork() {
        // Setup no network conditions.
        DeviceConditions deviceConditionsNoNetwork = new DeviceConditions(!POWER_CONNECTED,
                HIGH_BATTERY_LEVEL, ConnectionType.CONNECTION_NONE, !POWER_SAVE_MODE_ON, !METERED,
                SCREEN_ON_AND_UNLOCKED);
        ShadowDeviceConditions.setCurrentConditions(deviceConditionsNoNetwork);

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID).build();

        int result = mPrefetchBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        fail("Finished callback should not be run, network conditions not met.");
                    }
                });
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.RESCHEDULE, result);
    }

    /**
     * Tests that the background task is not started (rescheduled) when there's no connection and
     * limitless prefetching is enabled.
     */
    @Test
    public void testNoNetworkLimitless() {
        // Setup no network conditions.
        DeviceConditions deviceConditionsNoNetwork = new DeviceConditions(!POWER_CONNECTED,
                0 /* battery level */, ConnectionType.CONNECTION_NONE, !POWER_SAVE_MODE_ON,
                !METERED, SCREEN_ON_AND_UNLOCKED);
        ShadowDeviceConditions.setCurrentConditions(deviceConditionsNoNetwork);

        // Check impact on starting before native loaded.
        Bundle extrasBundle = new Bundle();
        extrasBundle.putBoolean(PrefetchBackgroundTask.LIMITLESS_BUNDLE_KEY, true);
        TaskParameters params = TaskParameters.create(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID)
                                        .addExtras(extrasBundle)
                                        .build();

        int result = mPrefetchBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        fail("Finished callback should not be run, network conditions not met.");
                    }
                });
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.RESCHEDULE, result);
    }

    @Test
    public void testUnmeteredWifiNetwork() {
        // Setup unmetered wifi conditions.
        DeviceConditions deviceConditionsUnmeteredWifi = new DeviceConditions(!POWER_CONNECTED,
                HIGH_BATTERY_LEVEL, ConnectionType.CONNECTION_WIFI, !POWER_SAVE_MODE_ON, !METERED,
                SCREEN_ON_AND_UNLOCKED);
        ShadowDeviceConditions.setCurrentConditions(deviceConditionsUnmeteredWifi);

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID).build();

        int result = mPrefetchBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        fail("Finished callback should not be run, network conditions not met.");
                    }
                });
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.LOAD_NATIVE, result);
    }

    @Test
    public void testMeteredWifiNetwork() {
        // Setup metered wifi conditions.
        DeviceConditions deviceConditionsMeteredWifi = new DeviceConditions(!POWER_CONNECTED,
                HIGH_BATTERY_LEVEL, ConnectionType.CONNECTION_WIFI, !POWER_SAVE_MODE_ON, METERED,
                SCREEN_ON_AND_UNLOCKED);
        ShadowDeviceConditions.setCurrentConditions(deviceConditionsMeteredWifi);

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID).build();

        int result = mPrefetchBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        fail("Finished callback should not be run, network conditions not met.");
                    }
                });
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.RESCHEDULE, result);
    }

    @Test
    public void test2GNetwork() {
        // Setup metered 2g connection conditions.
        DeviceConditions deviceConditions2G = new DeviceConditions(!POWER_CONNECTED,
                HIGH_BATTERY_LEVEL, ConnectionType.CONNECTION_2G, !POWER_SAVE_MODE_ON, METERED,
                SCREEN_ON_AND_UNLOCKED);
        // TODO(petewil): this test name and the condition below do not match.
        ShadowDeviceConditions.setCurrentConditions(deviceConditions2G);

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID).build();

        int result = mPrefetchBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        fail("Finished callback should not be run, network conditions not met.");
                    }
                });
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.RESCHEDULE, result);
    }

    @Test
    public void testBluetoothNetwork() {
        // Setup bluetooth connection conditions.
        DeviceConditions deviceConditionsBluetooth = new DeviceConditions(!POWER_CONNECTED,
                HIGH_BATTERY_LEVEL, ConnectionType.CONNECTION_BLUETOOTH, !POWER_SAVE_MODE_ON,
                !METERED, SCREEN_ON_AND_UNLOCKED);
        ShadowDeviceConditions.setCurrentConditions(deviceConditionsBluetooth);

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID).build();

        int result = mPrefetchBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        fail("Finished callback should not be run, network conditions not met.");
                    }
                });
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.RESCHEDULE, result);
    }

    @Test
    public void testOnStopAfterCallback() {
        final ArrayList<Boolean> reschedules = new ArrayList<>();
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID).build();

        // Conditions should be appropriate for running the task.
        DeviceConditions deviceConditions = new DeviceConditions(POWER_CONNECTED,
                HIGH_BATTERY_LEVEL - 1, ConnectionType.CONNECTION_WIFI, !POWER_SAVE_MODE_ON,
                !METERED, SCREEN_ON_AND_UNLOCKED);
        ShadowDeviceConditions.setCurrentConditions(deviceConditions);

        mPrefetchBackgroundTask.onStartTask(null, params, new TaskFinishedCallback() {
            @Override
            public void taskFinished(boolean needsReschedule) {
                reschedules.add(needsReschedule);
            }
        });
        mPrefetchBackgroundTask.doneProcessing(false);
        mPrefetchBackgroundTask.onStopTaskWithNative(RuntimeEnvironment.application, params);

        assertEquals(1, reschedules.size());
        assertEquals(false, reschedules.get(0));
    }

    @Test
    public void testPowerSaverOn() {
        // Setup power save mode, battery is high, wifi, not plugged in.
        DeviceConditions deviceConditionsPowerSave = new DeviceConditions(!POWER_CONNECTED,
                HIGH_BATTERY_LEVEL, ConnectionType.CONNECTION_WIFI, POWER_SAVE_MODE_ON, !METERED,
                SCREEN_ON_AND_UNLOCKED);
        ShadowDeviceConditions.setCurrentConditions(deviceConditionsPowerSave);

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID).build();

        int result = mPrefetchBackgroundTask.onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        fail("Finished callback should not be run, battery conditions not met.");
                    }
                });
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.RESCHEDULE, result);
    }
}

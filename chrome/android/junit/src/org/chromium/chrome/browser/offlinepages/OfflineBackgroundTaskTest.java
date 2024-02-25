// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.os.PersistableBundle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.BaseSwitches;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.device.ShadowDeviceConditions;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.net.ConnectionType;

/** Unit tests for OfflineBackgroundTask. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowDeviceConditions.class})
@CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
public class OfflineBackgroundTaskTest {
    private static final boolean REQUIRE_POWER = true;
    private static final boolean REQUIRE_UNMETERED = true;
    private static final boolean POWER_CONNECTED = true;
    private static final boolean POWER_SAVE_MODE_ON = true;
    private static final boolean METERED = true;
    private static final boolean SCREEN_ON_AND_UNLOCKED = true;
    private static final int MINIMUM_BATTERY_LEVEL = 33;

    private PersistableBundle mTaskExtras;
    private long mTestTime;
    private TriggerConditions mTriggerConditions =
            new TriggerConditions(!REQUIRE_POWER, MINIMUM_BATTERY_LEVEL, REQUIRE_UNMETERED);
    private DeviceConditions mDeviceConditions =
            new DeviceConditions(
                    !POWER_CONNECTED,
                    MINIMUM_BATTERY_LEVEL + 5,
                    ConnectionType.CONNECTION_3G,
                    !POWER_SAVE_MODE_ON,
                    !METERED,
                    SCREEN_ON_AND_UNLOCKED);
    private Activity mTestActivity;

    @Mock private BackgroundSchedulerProcessor mBackgroundSchedulerProcessor;

    @Mock private BackgroundTaskScheduler mTaskScheduler;
    @Mock private BackgroundTask.TaskFinishedCallback mTaskFinishedCallback;
    @Mock private Callback<Boolean> mInternalBooleanCallback;
    @Captor private ArgumentCaptor<TaskInfo> mTaskInfo;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mTaskScheduler);
        doReturn(true)
                .when(mTaskScheduler)
                .schedule(eq(ContextUtils.getApplicationContext()), mTaskInfo.capture());

        ShadowDeviceConditions.setCurrentConditions(mDeviceConditions);

        // Set up background scheduler processor mock.
        BackgroundSchedulerProcessor.setInstanceForTesting(mBackgroundSchedulerProcessor);

        // Build a bundle with trigger conditions.
        mTaskExtras = new PersistableBundle();
        TaskExtrasPacker.packTimeInBundle(mTaskExtras);
        TaskExtrasPacker.packTriggerConditionsInBundle(mTaskExtras, mTriggerConditions);

        // Set up single, stopped Activity.
        mTestActivity = new Activity();
        ApplicationStatus.onStateChangeForTesting(mTestActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mTestActivity, ActivityState.STOPPED);
    }

    private void setupScheduledProcessingWithResult(boolean result) {
        doReturn(result)
                .when(mBackgroundSchedulerProcessor)
                .startScheduledProcessing(
                        any(DeviceConditions.class), ArgumentMatchers.<Callback<Boolean>>any());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testCheckConditions_BatteryConditions_LowBattery_NoPower() {
        // Setup low battery conditions with no power connected.
        DeviceConditions deviceConditionsLowBattery =
                new DeviceConditions(
                        !POWER_CONNECTED,
                        MINIMUM_BATTERY_LEVEL - 1,
                        ConnectionType.CONNECTION_WIFI,
                        !POWER_SAVE_MODE_ON,
                        !METERED,
                        SCREEN_ON_AND_UNLOCKED);
        ShadowDeviceConditions.setCurrentConditions(deviceConditionsLowBattery);

        // Verify that conditions for processing are not met.
        assertFalse(
                OfflineBackgroundTask.checkConditions(
                        ContextUtils.getApplicationContext(), mTaskExtras));

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID)
                        .addExtras(mTaskExtras)
                        .build();

        int result =
                new OfflineBackgroundTask()
                        .onStartTaskBeforeNativeLoaded(
                                ContextUtils.getApplicationContext(),
                                params,
                                mTaskFinishedCallback);
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.RESCHEDULE, result);
        // Task finished can only gets called from the native part, when async processing starts.
        verify(mTaskFinishedCallback, times(0)).taskFinished(anyBoolean());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testCheckConditions_BatteryConditions_LowBattery_WithPower() {
        // Set battery percentage below minimum level, but connect power.
        DeviceConditions deviceConditionsPowerConnected =
                new DeviceConditions(
                        POWER_CONNECTED,
                        MINIMUM_BATTERY_LEVEL - 1,
                        ConnectionType.CONNECTION_WIFI,
                        !POWER_SAVE_MODE_ON,
                        !METERED,
                        SCREEN_ON_AND_UNLOCKED);
        ShadowDeviceConditions.setCurrentConditions(deviceConditionsPowerConnected);

        // Now verify that same battery level, with power connected, will pass the conditions.
        assertTrue(
                OfflineBackgroundTask.checkConditions(
                        ContextUtils.getApplicationContext(), mTaskExtras));

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID)
                        .addExtras(mTaskExtras)
                        .build();

        int result =
                new OfflineBackgroundTask()
                        .onStartTaskBeforeNativeLoaded(
                                ContextUtils.getApplicationContext(),
                                params,
                                mTaskFinishedCallback);
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.LOAD_NATIVE, result);
        // Task finished can only gets called from the native part, when async processing starts.
        verify(mTaskFinishedCallback, times(0)).taskFinished(anyBoolean());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testCheckConditions_OnLowEndDevice_ActivityStarted() {
        // Transition the test Activity to a running state.
        ApplicationStatus.onStateChangeForTesting(mTestActivity, ActivityState.STARTED);

        // Verify that conditions for processing are not met.
        assertFalse(
                OfflineBackgroundTask.checkConditions(
                        ContextUtils.getApplicationContext(), mTaskExtras));

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID)
                        .addExtras(mTaskExtras)
                        .build();

        int result =
                new OfflineBackgroundTask()
                        .onStartTaskBeforeNativeLoaded(
                                ContextUtils.getApplicationContext(),
                                params,
                                mTaskFinishedCallback);
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.RESCHEDULE, result);
        // Task finished can only gets called from the native part, when async processing starts.
        verify(mTaskFinishedCallback, times(0)).taskFinished(anyBoolean());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testCheckConditions_OnLowEndDevice_ActivityStopped() {
        // Switch activity state to stopped.
        ApplicationStatus.onStateChangeForTesting(mTestActivity, ActivityState.STOPPED);

        // Now verify that condition check passes when Activity is stopped.
        assertTrue(
                OfflineBackgroundTask.checkConditions(
                        ContextUtils.getApplicationContext(), mTaskExtras));

        // Check impact on starting before native loaded.
        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID)
                        .addExtras(mTaskExtras)
                        .build();

        int result =
                new OfflineBackgroundTask()
                        .onStartTaskBeforeNativeLoaded(
                                ContextUtils.getApplicationContext(),
                                params,
                                mTaskFinishedCallback);
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.LOAD_NATIVE, result);
        // Task finished can only gets called from the native part, when async processing starts.
        verify(mTaskFinishedCallback, times(0)).taskFinished(anyBoolean());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnStartTaskWithNative_BackupScheduleIfExecutingTask() {
        setupScheduledProcessingWithResult(true);

        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID)
                        .addExtras(mTaskExtras)
                        .build();

        new OfflineBackgroundTask()
                .onStartTaskWithNative(
                        ContextUtils.getApplicationContext(), params, mTaskFinishedCallback);

        verify(mTaskScheduler, times(1))
                .schedule(eq(ContextUtils.getApplicationContext()), any(TaskInfo.class));
        // Task is running at this point, hence no callback issued.
        verify(mTaskFinishedCallback, times(0)).taskFinished(anyBoolean());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOnStartTaskWithNative_RescheduleThroughCallbackWhenRunning() {
        setupScheduledProcessingWithResult(false);

        TaskParameters params =
                TaskParameters.create(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID)
                        .addExtras(mTaskExtras)
                        .build();

        new OfflineBackgroundTask()
                .onStartTaskWithNative(
                        ContextUtils.getApplicationContext(), params, mTaskFinishedCallback);

        verify(mTaskScheduler, times(0)).schedule(any(Context.class), any(TaskInfo.class));
        // Task started async processing after native load, but processing refused to progress,
        // hence task finished called with reschedule request.
        verify(mTaskFinishedCallback, times(1)).taskFinished(eq(true));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testStartBackgroundRequests() {
        setupScheduledProcessingWithResult(true);

        assertTrue(
                OfflineBackgroundTask.startScheduledProcessing(
                        mBackgroundSchedulerProcessor,
                        ContextUtils.getApplicationContext(),
                        mTaskExtras,
                        mInternalBooleanCallback));

        // Check with BackgroundSchedulerProcessor that processing started.
        verify(mBackgroundSchedulerProcessor, times(1))
                .startScheduledProcessing(eq(mDeviceConditions), eq(mInternalBooleanCallback));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testStartBackgroundRequestsNotStarted() {
        // Processing will not be started here.
        setupScheduledProcessingWithResult(false);

        assertFalse(
                OfflineBackgroundTask.startScheduledProcessing(
                        mBackgroundSchedulerProcessor,
                        ContextUtils.getApplicationContext(),
                        mTaskExtras,
                        mInternalBooleanCallback));

        // Check with BackgroundSchedulerProcessor that it did not start.
        verify(mBackgroundSchedulerProcessor, times(1))
                .startScheduledProcessing(eq(mDeviceConditions), eq(mInternalBooleanCallback));
    }
}

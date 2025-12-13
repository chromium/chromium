// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_sync;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.os.PersistableBundle;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.BaseSwitches;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.net.ConnectionType;

/** Unit tests for PeriodicBackgroundSyncChromeWakeUpTask. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
public class PeriodicBackgroundSyncChromeWakeUpTaskTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private PersistableBundle mTaskExtras;
    private long mTaskTime;

    @Mock private PeriodicBackgroundSyncChromeWakeUpTask.Natives mNativeMock;
    @Mock private BackgroundTaskScheduler mTaskScheduler;
    @Mock private BackgroundTask.TaskFinishedCallback mTaskFinishedCallback;
    @Captor private ArgumentCaptor<TaskInfo> mTaskInfo;

    @Before
    public void setUp() {
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mTaskScheduler);

        mTaskExtras = new PersistableBundle();

        doReturn(true)
                .when(mTaskScheduler)
                .schedule(eq(RuntimeEnvironment.application), mTaskInfo.capture());

        DeviceConditions.setForTesting(
                new DeviceConditions(
                        false, 0, ConnectionType.CONNECTION_NONE, false, false, false));

        PeriodicBackgroundSyncChromeWakeUpTaskJni.setInstanceForTesting(mNativeMock);
    }

    @Test
    @Feature("BackgroundSync")
    public void testNetworkConditions_NoNetwork() {
        // The test has been set up with no network by default.
        TaskParameters params =
                TaskParameters.create(TaskIds.PERIODIC_BACKGROUND_SYNC_CHROME_WAKEUP_TASK_JOB_ID)
                        .addExtras(mTaskExtras)
                        .build();

        int result =
                new PeriodicBackgroundSyncChromeWakeUpTask()
                        .onStartTaskBeforeNativeLoaded(
                                RuntimeEnvironment.application, params, mTaskFinishedCallback);
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.RESCHEDULE, result);

        // TaskFinishedCallback callback is only called once native code has
        // finished processing pending Periodic Background Sync registrations.
        verify(mTaskFinishedCallback, times(0)).taskFinished(anyBoolean());
    }

    @Test
    @Feature("BackgroundSync")
    public void testNetworkConditions_Wifi() {
        DeviceConditions.setForTesting(
                new DeviceConditions(
                        false, 0, ConnectionType.CONNECTION_WIFI, false, false, false));
        TaskParameters params =
                TaskParameters.create(TaskIds.PERIODIC_BACKGROUND_SYNC_CHROME_WAKEUP_TASK_JOB_ID)
                        .addExtras(mTaskExtras)
                        .build();

        int result =
                new PeriodicBackgroundSyncChromeWakeUpTask()
                        .onStartTaskBeforeNativeLoaded(
                                RuntimeEnvironment.application, params, mTaskFinishedCallback);
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.LOAD_NATIVE, result);

        // TaskFinishedCallback callback is only called once native code has
        // finished processing pending Background Sync registrations.
        verify(mTaskFinishedCallback, times(0)).taskFinished(anyBoolean());
    }

    @Test
    @Feature("BackgroundSync")
    public void testOnStartTaskWithNative() {
        TaskParameters params =
                TaskParameters.create(TaskIds.PERIODIC_BACKGROUND_SYNC_CHROME_WAKEUP_TASK_JOB_ID)
                        .addExtras(mTaskExtras)
                        .build();

        new PeriodicBackgroundSyncChromeWakeUpTask()
                .onStartTaskWithNative(
                        RuntimeEnvironment.application, params, mTaskFinishedCallback);

        verify(mNativeMock).firePeriodicBackgroundSyncEvents(any(Runnable.class));
        verify(mTaskFinishedCallback, times(0)).taskFinished(anyBoolean());
        verify(mTaskScheduler, times(0)).schedule(any(Context.class), any(TaskInfo.class));
    }

    @Test
    @Feature("BackgroundSync")
    public void onStopTaskBeforeNativeLoaded() {
        TaskParameters params =
                TaskParameters.create(TaskIds.PERIODIC_BACKGROUND_SYNC_CHROME_WAKEUP_TASK_JOB_ID)
                        .addExtras(mTaskExtras)
                        .build();

        new PeriodicBackgroundSyncChromeWakeUpTask()
                .onStopTaskBeforeNativeLoaded(RuntimeEnvironment.application, params);

        verify(mTaskFinishedCallback, times(0)).taskFinished(anyBoolean());
        verify(mTaskScheduler, times(0)).schedule(any(Context.class), any(TaskInfo.class));
    }

    @Test
    @Feature("BackgroundSync")
    public void testOnStopTaskWithNative() {
        TaskParameters params =
                TaskParameters.create(TaskIds.PERIODIC_BACKGROUND_SYNC_CHROME_WAKEUP_TASK_JOB_ID)
                        .addExtras(mTaskExtras)
                        .build();

        new PeriodicBackgroundSyncChromeWakeUpTask()
                .onStopTaskWithNative(RuntimeEnvironment.application, params);

        verify(mTaskFinishedCallback, times(0)).taskFinished(anyBoolean());
        verify(mTaskScheduler, times(0)).schedule(any(Context.class), any(TaskInfo.class));
    }
}

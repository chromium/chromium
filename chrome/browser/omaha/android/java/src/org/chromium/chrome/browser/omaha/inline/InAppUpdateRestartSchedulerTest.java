// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.KeyguardManager;
import android.content.Context;
import android.os.PowerManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowKeyguardManager;
import org.robolectric.shadows.ShadowPowerManager;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHost;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostRegistry;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link InAppUpdateRestartScheduler}. */
@RunWith(BaseRobolectricTestRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
public class InAppUpdateRestartSchedulerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BackgroundTaskScheduler mMockScheduler;

    private Context mContext;
    private ShadowKeyguardManager mShadowKeyguardManager;
    private ShadowPowerManager mShadowPowerManager;
    private InAppUpdateRestartScheduler mScheduler;
    private @Nullable Activity mActivity;

    @Before
    public void setUp() {
        InAppUpdatePolicy.resetForTesting();
        ChromeFeatureList.sInAppUpdateRestartMode.setForTesting("background");
        IncognitoTabHostRegistry.getInstance().clearForTesting();
        // Most tests exercise a staged update; the "not downloaded" tests override this.
        InAppUpdatePolicy.setUpdateDownloaded(true);
        mContext = ContextUtils.getApplicationContext();
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mMockScheduler);
        when(mMockScheduler.schedule(any(), any())).thenReturn(true);

        KeyguardManager keyguardManager =
                (KeyguardManager) mContext.getSystemService(Context.KEYGUARD_SERVICE);
        mShadowKeyguardManager = Shadows.shadowOf(keyguardManager);
        mShadowKeyguardManager.setKeyguardLocked(false);

        PowerManager powerManager = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
        mShadowPowerManager = Shadows.shadowOf(powerManager);
        mShadowPowerManager.setIsInteractive(true);

        mScheduler = new InAppUpdateRestartScheduler();
        InAppUpdateRestartScheduler.setInstanceForTesting(mScheduler);
    }

    @After
    public void tearDown() {
        if (mActivity != null) {
            ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
        }
        IncognitoTabHostRegistry.getInstance().clearForTesting();
        InAppUpdatePolicy.resetForTesting();
    }

    private static HistogramWatcher expectEvents(@InAppUpdateBackgroundRestartEvent int... events) {
        var builder = HistogramWatcher.newBuilder();
        for (int event : events) {
            builder.expectIntRecord(InAppUpdateRestartTask.HISTOGRAM_EVENT, event);
        }
        return builder.build();
    }

    private static HistogramWatcher expectNoEvents() {
        return HistogramWatcher.newBuilder()
                .expectNoRecords(InAppUpdateRestartTask.HISTOGRAM_EVENT)
                .build();
    }

    private void stop() {
        mScheduler.onApplicationStateChange(ApplicationState.HAS_STOPPED_ACTIVITIES);
    }

    private void scheduleTask() {
        stop();
        assertTrue(mScheduler.isTaskScheduledForTesting());
    }

    private void assertTaskCanceled() {
        verify(mMockScheduler).cancel(eq(mContext), eq(TaskIds.IN_APP_UPDATE_RESTART_JOB_ID));
        assertFalse(mScheduler.isTaskScheduledForTesting());
    }

    private void assertNothingScheduled() {
        verify(mMockScheduler, never()).schedule(any(), any());
        assertFalse(mScheduler.isTaskScheduledForTesting());
    }

    private void setActivityState(@ActivityState int state) {
        if (mActivity == null) {
            mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        }
        ApplicationStatus.onStateChangeForTesting(mActivity, state);
    }

    @Test
    public void testScheduleOnStop_whenDownloaded() {
        var watcher = expectEvents(InAppUpdateBackgroundRestartEvent.SCHEDULED);

        stop();

        ArgumentCaptor<TaskInfo> captor = ArgumentCaptor.forClass(TaskInfo.class);
        verify(mMockScheduler).schedule(eq(mContext), captor.capture());
        assertEquals(TaskIds.IN_APP_UPDATE_RESTART_JOB_ID, captor.getValue().getTaskId());
        assertTrue(InAppUpdatePolicy.isRestartTaskScheduled());
        assertTrue(mScheduler.isTaskScheduledForTesting());

        TaskInfo.OneOffInfo oneOffInfo = captor.getValue().getOneOffInfo();
        assertEquals(
                TimeUnit.MINUTES.toMillis(
                        InAppUpdatePolicy.DEFAULT_BACKGROUND_RESTART_DELAY_MINUTES),
                oneOffInfo.getWindowStartTimeMs());
        // The deadline bounds the latency when JobScheduler batches the job until the end of the
        // window (observed when Chrome has no live process), so it is delay + window, not a
        // multiple of the delay.
        assertEquals(
                TimeUnit.MINUTES.toMillis(
                        InAppUpdatePolicy.DEFAULT_BACKGROUND_RESTART_DELAY_MINUTES
                                + InAppUpdatePolicy.DEFAULT_BACKGROUND_RESTART_WINDOW_MINUTES),
                oneOffInfo.getWindowEndTimeMs());
        assertEquals(
                0,
                captor.getValue()
                        .getExtras()
                        .getInt(InAppUpdateRestartScheduler.EXTRA_RETRY_COUNT, -1));
        watcher.assertExpected();
    }

    @Test
    public void testScheduleOnStop_scheduleReturnsFalse_doesNotSetScheduledOrRecordEvent() {
        when(mMockScheduler.schedule(any(), any())).thenReturn(false);

        var watcher = expectNoEvents();

        stop();

        verify(mMockScheduler).schedule(eq(mContext), any());
        assertFalse(mScheduler.isTaskScheduledForTesting());
        assertFalse(InAppUpdatePolicy.isRestartTaskScheduled());
        watcher.assertExpected();
    }

    @Test
    public void testStopThenDestroy_schedulesOnce() {
        var watcher = expectEvents(InAppUpdateBackgroundRestartEvent.SCHEDULED);

        // Dismissing Chrome from Recents stops and then destroys the last activity within
        // milliseconds; the second state change must not reset the already scheduled delay.
        stop();
        mScheduler.onApplicationStateChange(ApplicationState.HAS_DESTROYED_ACTIVITIES);

        verify(mMockScheduler, times(1)).schedule(eq(mContext), any());
        watcher.assertExpected();
    }

    @Test
    public void testScheduleOnDestroy_whenDownloaded() {
        var watcher = expectEvents(InAppUpdateBackgroundRestartEvent.SCHEDULED);

        mScheduler.onApplicationStateChange(ApplicationState.HAS_DESTROYED_ACTIVITIES);

        verify(mMockScheduler).schedule(eq(mContext), any());
        assertTrue(mScheduler.isTaskScheduledForTesting());
        watcher.assertExpected();
    }

    @Test
    public void testCancelOnRunningOrPaused_whenScheduled() {
        var watcher =
                expectEvents(
                        InAppUpdateBackgroundRestartEvent.SCHEDULED,
                        InAppUpdateBackgroundRestartEvent.CANCELED,
                        InAppUpdateBackgroundRestartEvent.SCHEDULED,
                        InAppUpdateBackgroundRestartEvent.CANCELED);

        scheduleTask();
        mScheduler.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        assertFalse(mScheduler.isTaskScheduledForTesting());

        scheduleTask();
        mScheduler.onApplicationStateChange(ApplicationState.HAS_PAUSED_ACTIVITIES);
        assertFalse(mScheduler.isTaskScheduledForTesting());

        verify(mMockScheduler, times(2)).schedule(eq(mContext), any());
        verify(mMockScheduler, times(2))
                .cancel(eq(mContext), eq(TaskIds.IN_APP_UPDATE_RESTART_JOB_ID));
        watcher.assertExpected();
    }

    @Test
    public void testRunningPausedWithScheduledTask_cancelsOnlyOnce() {
        InAppUpdatePolicy.setRestartTaskScheduled(true);
        var watcher = expectNoEvents();

        mScheduler.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        assertFalse(InAppUpdatePolicy.isRestartTaskScheduled());
        mScheduler.onApplicationStateChange(ApplicationState.HAS_PAUSED_ACTIVITIES);
        mScheduler.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);

        verify(mMockScheduler, times(1))
                .cancel(eq(mContext), eq(TaskIds.IN_APP_UPDATE_RESTART_JOB_ID));
        watcher.assertExpected();
    }

    @Test
    public void testRunningPausedWithoutScheduledTask_doesNotCancel() {
        var watcher = expectNoEvents();

        mScheduler.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        mScheduler.onApplicationStateChange(ApplicationState.HAS_PAUSED_ACTIVITIES);

        verify(mMockScheduler, never()).cancel(any(), anyInt());
        watcher.assertExpected();
    }

    @Test
    public void testDoNothing_whenUpdateNotDownloaded() {
        InAppUpdatePolicy.setUpdateDownloaded(false);
        var watcher = expectNoEvents();

        stop();

        assertNothingScheduled();
        watcher.assertExpected();
    }

    @Test
    public void testSkip_whenKeyguardLocked() {
        mShadowKeyguardManager.setKeyguardLocked(true);
        var watcher = expectEvents(InAppUpdateBackgroundRestartEvent.SKIPPED_SCREEN_LOCKED);

        stop();

        assertNothingScheduled();
        watcher.assertExpected();
    }

    @Test
    public void testSkip_whenScreenNotInteractive() {
        mShadowPowerManager.setIsInteractive(false);
        var watcher = expectEvents(InAppUpdateBackgroundRestartEvent.SKIPPED_SCREEN_LOCKED);

        stop();

        assertNothingScheduled();
        watcher.assertExpected();
    }

    @Test
    public void testSkip_whenIncognitoTabsOpen() {
        IncognitoTabHost host = mock(IncognitoTabHost.class);
        when(host.hasIncognitoTabs()).thenReturn(true);
        IncognitoTabHostRegistry.getInstance().register(host);
        var watcher = expectEvents(InAppUpdateBackgroundRestartEvent.SKIPPED_INCOGNITO);

        stop();

        assertNothingScheduled();
        watcher.assertExpected();
    }

    @Test
    public void testSnackbarMode_doesNothing() {
        ChromeFeatureList.sInAppUpdateRestartMode.setForTesting("snackbar");
        var watcher = expectNoEvents();

        stop();

        assertNothingScheduled();
        watcher.assertExpected();
    }

    @Test
    public void testCancelTask_whenNotPreviouslyScheduled_stillCallsCancel() {
        assertFalse(mScheduler.isTaskScheduledForTesting());

        var watcher = expectNoEvents();

        mScheduler.cancelTask();

        assertTaskCanceled();
        watcher.assertExpected();
    }

    @Test
    public void testStart_updateNotDownloaded_cancelsOnlyIfScheduled() {
        InAppUpdatePolicy.setUpdateDownloaded(false);
        mScheduler.start();
        assertFalse(mScheduler.isListenerRegisteredForTesting());
        assertFalse(mScheduler.isTaskScheduledForTesting());
        verify(mMockScheduler, never()).cancel(any(), anyInt());

        // If a prior session scheduled a job and Play later cleared the staged update before
        // deferred startup, start() must clear the stale flag and cancel the leftover job.
        InAppUpdatePolicy.setRestartTaskScheduled(true);
        var watcher = expectNoEvents();
        mScheduler.start();
        assertFalse(InAppUpdatePolicy.isRestartTaskScheduled());
        assertFalse(mScheduler.isListenerRegisteredForTesting());
        assertTaskCanceled();
        watcher.assertExpected();
    }

    @Test
    public void testStart_whenStateHasStoppedActivitiesAndScreenUnlocked_schedulesTask() {
        setActivityState(ActivityState.STOPPED);

        mScheduler.start();
        assertTrue(mScheduler.isListenerRegisteredForTesting());
        assertTrue(mScheduler.isTaskScheduledForTesting());
        verify(mMockScheduler).schedule(eq(mContext), any());
        verify(mMockScheduler, never()).cancel(any(), anyInt());
    }

    @Test
    public void testStart_whenActivityRunning_cancelsStaleTaskOnce() {
        InAppUpdatePolicy.setRestartTaskScheduled(true);
        setActivityState(ActivityState.RESUMED);

        mScheduler.start();
        assertTrue(mScheduler.isListenerRegisteredForTesting());
        assertFalse(InAppUpdatePolicy.isRestartTaskScheduled());
        verify(mMockScheduler, times(1))
                .cancel(eq(mContext), eq(TaskIds.IN_APP_UPDATE_RESTART_JOB_ID));
        verify(mMockScheduler, never()).schedule(any(), any());

        // Second start in the same process without a task scheduled should neither cancel nor
        // schedule.
        mScheduler.start();
        verify(mMockScheduler, times(1))
                .cancel(eq(mContext), eq(TaskIds.IN_APP_UPDATE_RESTART_JOB_ID));
        verify(mMockScheduler, never()).schedule(any(), any());
    }

    @Test
    public void testStart_modeSnackbar_cancelsOnlyIfScheduled() {
        ChromeFeatureList.sInAppUpdateRestartMode.setForTesting("snackbar");
        mScheduler.start();
        verify(mMockScheduler, never()).cancel(any(), anyInt());
        assertFalse(InAppUpdatePolicy.isRestartTaskScheduled());
        assertFalse(mScheduler.isListenerRegisteredForTesting());

        InAppUpdatePolicy.setRestartTaskScheduled(true);
        mScheduler.start();
        assertFalse(InAppUpdatePolicy.isRestartTaskScheduled());
        assertTaskCanceled();
        assertFalse(mScheduler.isListenerRegisteredForTesting());
    }

    @Test
    public void testOnFeatureDisabled_withScheduledTaskAndDownloaded_cleansUpBoth() {
        var watcher =
                expectEvents(
                        InAppUpdateBackgroundRestartEvent.SCHEDULED,
                        InAppUpdateBackgroundRestartEvent.CANCELED);
        scheduleTask();
        assertTrue(InAppUpdateRestartScheduler.hasPendingCleanup());

        InAppUpdateRestartScheduler.onFeatureDisabled();

        assertTaskCanceled();
        assertFalse(InAppUpdatePolicy.isRestartTaskScheduled());
        assertFalse(InAppUpdatePolicy.isUpdateDownloaded());
        watcher.assertExpected();
    }

    @Test
    public void testOnFeatureDisabled_coldStartWithoutInstance_cleansUpWithoutCanceledEvent() {
        InAppUpdateRestartScheduler.setInstanceForTesting(null);
        InAppUpdatePolicy.setRestartTaskScheduled(true);
        InAppUpdatePolicy.setUpdateDownloaded(true);
        var watcher = expectNoEvents();

        InAppUpdateRestartScheduler.onFeatureDisabled();

        assertFalse(InAppUpdatePolicy.isRestartTaskScheduled());
        assertFalse(InAppUpdatePolicy.isUpdateDownloaded());
        verify(mMockScheduler).cancel(eq(mContext), eq(TaskIds.IN_APP_UPDATE_RESTART_JOB_ID));
        watcher.assertExpected();
    }

    @Test
    public void testOnFeatureDisabled_withoutPendingState_doesNotTouchScheduler() {
        InAppUpdatePolicy.setRestartTaskScheduled(false);
        InAppUpdatePolicy.setUpdateDownloaded(false);
        assertFalse(InAppUpdateRestartScheduler.hasPendingCleanup());

        InAppUpdateRestartScheduler.onFeatureDisabled();

        verify(mMockScheduler, never()).cancel(any(), anyInt());
    }

    @Test
    public void testOnTaskStarted_clearsFlagSoRunningDoesNotRecordCanceled() {
        stop();
        assertTrue(mScheduler.isTaskScheduledForTesting());

        InAppUpdateRestartScheduler.onTaskStarted();
        assertFalse(mScheduler.isTaskScheduledForTesting());

        var watcher = expectNoEvents();

        mScheduler.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        verify(mMockScheduler, never()).cancel(any(), anyInt());
        watcher.assertExpected();
    }

    @Test
    public void testScheduleForegroundServiceRetry_skipsWhenPreconditionsNotMet() {
        setActivityState(ActivityState.RESUMED);
        InAppUpdateRestartScheduler.scheduleForegroundServiceRetry(0);
        assertNothingScheduled();
        setActivityState(ActivityState.STOPPED);

        InAppUpdatePolicy.setUpdateDownloaded(false);
        InAppUpdateRestartScheduler.scheduleForegroundServiceRetry(0);
        assertNothingScheduled();
        InAppUpdatePolicy.setUpdateDownloaded(true);

        IncognitoTabHost host = mock(IncognitoTabHost.class);
        when(host.hasIncognitoTabs()).thenReturn(true);
        IncognitoTabHostRegistry.getInstance().register(host);
        InAppUpdateRestartScheduler.scheduleForegroundServiceRetry(0);
        assertNothingScheduled();
        IncognitoTabHostRegistry.getInstance().unregister(host);

        // When a task is already marked scheduled in memory, retry does not double-schedule.
        scheduleTask();
        InAppUpdateRestartScheduler.scheduleForegroundServiceRetry(0);
        verify(mMockScheduler, times(1)).schedule(any(), any());
    }

    @Test
    public void testScheduleForegroundServiceRetry_exponentialBackoffCapAndResetOnStop() {
        var watcher =
                expectEvents(
                        InAppUpdateBackgroundRestartEvent.RETRY_SCHEDULED,
                        InAppUpdateBackgroundRestartEvent.RETRY_SCHEDULED,
                        InAppUpdateBackgroundRestartEvent.RETRY_SCHEDULED,
                        InAppUpdateBackgroundRestartEvent.RETRY_SCHEDULED,
                        InAppUpdateBackgroundRestartEvent.RETRY_SCHEDULED,
                        InAppUpdateBackgroundRestartEvent.RETRY_SCHEDULED,
                        InAppUpdateBackgroundRestartEvent.SCHEDULED);
        int[] expectedDelaysMinutes = {15, 30, 60, 120, 240, 240};
        for (int i = 0; i < expectedDelaysMinutes.length; i++) {
            InAppUpdateRestartScheduler.scheduleForegroundServiceRetry(i);
            InAppUpdateRestartScheduler.onTaskStarted();
        }

        // At cap: retry must not schedule.
        InAppUpdateRestartScheduler.scheduleForegroundServiceRetry(expectedDelaysMinutes.length);

        // Leaving Chrome again schedules a fresh cycle with retry count reset to 0.
        stop();

        ArgumentCaptor<TaskInfo> captor = ArgumentCaptor.forClass(TaskInfo.class);
        verify(mMockScheduler, times(expectedDelaysMinutes.length + 1))
                .schedule(eq(mContext), captor.capture());
        for (int i = 0; i < expectedDelaysMinutes.length; i++) {
            TaskInfo info = captor.getAllValues().get(i);
            long expectedStartMs = TimeUnit.MINUTES.toMillis(expectedDelaysMinutes[i]);
            assertEquals(expectedStartMs, info.getOneOffInfo().getWindowStartTimeMs());
            assertEquals(
                    expectedStartMs
                            + TimeUnit.MINUTES.toMillis(
                                    InAppUpdateRestartScheduler
                                            .FOREGROUND_SERVICE_RETRY_WINDOW_MINUTES),
                    info.getOneOffInfo().getWindowEndTimeMs());
            assertEquals(
                    i + 1, info.getExtras().getInt(InAppUpdateRestartScheduler.EXTRA_RETRY_COUNT));
        }

        TaskInfo freshSchedule = captor.getAllValues().get(expectedDelaysMinutes.length);
        assertEquals(
                0, freshSchedule.getExtras().getInt(InAppUpdateRestartScheduler.EXTRA_RETRY_COUNT));
        watcher.assertExpected();
    }
}

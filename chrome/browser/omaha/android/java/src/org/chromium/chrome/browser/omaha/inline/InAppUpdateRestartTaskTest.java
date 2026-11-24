// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.os.PersistableBundle;

import com.google.android.gms.tasks.TaskCompletionSource;
import com.google.android.gms.tasks.Tasks;
import com.google.android.play.core.appupdate.AppUpdateManager;
import com.google.android.play.core.install.InstallException;
import com.google.android.play.core.install.model.InstallErrorCode;

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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHost;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostRegistry;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link InAppUpdateRestartTask}. */
@RunWith(BaseRobolectricTestRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
public class InAppUpdateRestartTaskTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AppUpdateManager mMockAppUpdateManager;
    @Mock private BackgroundTaskScheduler mMockScheduler;
    @Mock private TaskFinishedCallback mMockCallback;

    private Context mContext;
    private InAppUpdateRestartTask mTask;
    private TaskParameters mTaskParameters;
    private @Nullable Activity mVisibleActivity;

    @Before
    public void setUp() {
        InAppUpdatePolicy.resetForTesting();
        ChromeFeatureList.sInAppUpdateRestartMode.setForTesting("background");
        IncognitoTabHostRegistry.getInstance().clearForTesting();
        mContext = ContextUtils.getApplicationContext();
        // Robolectric defaults process importance to foreground; simulate a real JobScheduler
        // task running without an active foreground service unless a test overrides it.
        InAppUpdatePolicy.setProcessImportanceForTesting(
                ActivityManager.RunningAppProcessInfo.IMPORTANCE_PERCEPTIBLE);
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mMockScheduler);
        InAppUpdateRestartTask.setAppUpdateManagerForTesting(mMockAppUpdateManager);

        when(mMockAppUpdateManager.completeUpdate()).thenReturn(Tasks.forResult(null));

        mTask = new InAppUpdateRestartTask();
        mTaskParameters = TaskParameters.create(TaskIds.IN_APP_UPDATE_RESTART_JOB_ID).build();
    }

    @After
    public void tearDown() {
        if (mVisibleActivity != null) {
            ApplicationStatus.onStateChangeForTesting(mVisibleActivity, ActivityState.DESTROYED);
        }
        IncognitoTabHostRegistry.getInstance().clearForTesting();
        InAppUpdatePolicy.resetForTesting();
    }

    private boolean startTask() {
        return mTask.onStartTask(mContext, mTaskParameters, mMockCallback);
    }

    private void showActivity() {
        mVisibleActivity = Robolectric.buildActivity(Activity.class).setup().get();
        ApplicationStatus.onStateChangeForTesting(mVisibleActivity, ActivityState.RESUMED);
    }

    private void assertSkippedWithoutCallingPlay() {
        verify(mMockAppUpdateManager, never()).getAppUpdateInfo();
        verify(mMockAppUpdateManager, never()).completeUpdate();
        verify(mMockCallback, never()).taskFinished(anyBoolean());
    }

    private static void assertScheduledRetry(
            TaskInfo taskInfo, int expectedDelayMinutes, int expectedRetryCount) {
        assertEquals(TaskIds.IN_APP_UPDATE_RESTART_JOB_ID, taskInfo.getTaskId());
        TaskInfo.OneOffInfo oneOffInfo = taskInfo.getOneOffInfo();
        assertEquals(
                TimeUnit.MINUTES.toMillis(expectedDelayMinutes), oneOffInfo.getWindowStartTimeMs());
        assertEquals(
                TimeUnit.MINUTES.toMillis(
                        expectedDelayMinutes
                                + InAppUpdateRestartScheduler
                                        .FOREGROUND_SERVICE_RETRY_WINDOW_MINUTES),
                oneOffInfo.getWindowEndTimeMs());
        assertTrue(InAppUpdatePolicy.isRestartTaskScheduled());
        assertEquals(
                expectedRetryCount,
                taskInfo.getExtras().getInt(InAppUpdateRestartScheduler.EXTRA_RETRY_COUNT, -1));
    }

    private static HistogramWatcher expectEvents(@InAppUpdateBackgroundRestartEvent int... events) {
        var builder = HistogramWatcher.newBuilder();
        for (int event : events) {
            builder.expectIntRecord(InAppUpdateRestartTask.HISTOGRAM_EVENT, event);
        }
        return builder.build();
    }

    // The task must not query getAppUpdateInfo() before installing: while offline that query
    // cannot report a downloaded update, which would stop a staged update from being installed.
    @Test
    public void testOnStartTask_downloaded_callsCompleteUpdateWithoutQueryingUpdateInfo() {
        InAppUpdatePolicy.setUpdateDownloaded(true);

        var watcher = expectEvents(InAppUpdateBackgroundRestartEvent.COMPLETE_UPDATE_CALLED);

        assertTrue(startTask());
        ShadowLooper.idleMainLooper();

        verify(mMockAppUpdateManager, never()).getAppUpdateInfo();
        verify(mMockAppUpdateManager).completeUpdate();
        assertFalse(InAppUpdatePolicy.isUpdateDownloaded());
        verify(mMockCallback).taskFinished(false);
        watcher.assertExpected();
    }

    // When Play reports that it holds no download, the flag stays cleared and the run gets its own
    // bucket, apart from both failures and skips where Play was not called.
    @Test
    public void testOnStartTask_playHoldsNoDownload_keepsFlagClearedAndRecordsDownloadNotPresent() {
        InAppUpdatePolicy.setUpdateDownloaded(true);
        when(mMockAppUpdateManager.completeUpdate())
                .thenReturn(
                        Tasks.forException(
                                new InstallException(InstallErrorCode.ERROR_DOWNLOAD_NOT_PRESENT)));

        var watcher =
                expectEvents(
                        InAppUpdateBackgroundRestartEvent.COMPLETE_UPDATE_CALLED,
                        InAppUpdateBackgroundRestartEvent.COMPLETE_UPDATE_DOWNLOAD_NOT_PRESENT);

        assertTrue(startTask());
        ShadowLooper.idleMainLooper();

        verify(mMockCallback).taskFinished(false);
        assertFalse(InAppUpdatePolicy.isUpdateDownloaded());
        watcher.assertExpected();
    }

    @Test
    public void testOnStartTask_notDownloadedInPrefs_finishesWithoutCallingPlay() {
        InAppUpdatePolicy.setUpdateDownloaded(false);

        var watcher = expectEvents(InAppUpdateBackgroundRestartEvent.SKIPPED_NOT_DOWNLOADED);

        assertFalse(startTask());
        assertSkippedWithoutCallingPlay();
        watcher.assertExpected();
    }

    @Test
    public void testOnStartTask_visibleActivities_finishesWithoutInstall() {
        InAppUpdatePolicy.setUpdateDownloaded(true);
        showActivity();

        var watcher = expectEvents(InAppUpdateBackgroundRestartEvent.SKIPPED_VISIBLE_AT_RUN);

        assertFalse(startTask());
        assertSkippedWithoutCallingPlay();
        watcher.assertExpected();
    }

    @Test
    public void testOnStartTask_incognitoTabsOpen_finishesWithoutInstall() {
        InAppUpdatePolicy.setUpdateDownloaded(true);
        IncognitoTabHost host = mock(IncognitoTabHost.class);
        when(host.hasIncognitoTabs()).thenReturn(true);
        IncognitoTabHostRegistry.getInstance().register(host);

        var watcher = expectEvents(InAppUpdateBackgroundRestartEvent.SKIPPED_INCOGNITO);

        assertFalse(startTask());
        assertSkippedWithoutCallingPlay();
        watcher.assertExpected();
    }

    // completeUpdate() is called from onStartTask() itself, so a synchronous failure ends the job
    // through the return value and taskFinished() is not called.
    @Test
    public void testOnStartTask_completeUpdateThrowsSynchronously_returnsFalseAndKeepsFlag() {
        InAppUpdatePolicy.setUpdateDownloaded(true);
        when(mMockAppUpdateManager.completeUpdate())
                .thenThrow(new RuntimeException("Synchronous completeUpdate failure"));

        var watcher =
                expectEvents(
                        InAppUpdateBackgroundRestartEvent.COMPLETE_UPDATE_CALLED,
                        InAppUpdateBackgroundRestartEvent.COMPLETE_UPDATE_FAILED);

        assertFalse(startTask());
        ShadowLooper.idleMainLooper();

        verify(mMockCallback, never()).taskFinished(anyBoolean());
        assertTrue(InAppUpdatePolicy.isUpdateDownloaded());
        watcher.assertExpected();
    }

    @Test
    public void testOnStartTask_restartModeSnackbar_finishesWithoutInstall() {
        InAppUpdatePolicy.setUpdateDownloaded(true);
        ChromeFeatureList.sInAppUpdateRestartMode.setForTesting("snackbar");

        assertFalse(startTask());
        assertSkippedWithoutCallingPlay();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnStartTask_featureDisabled_finishesWithoutInstall() {
        InAppUpdatePolicy.setUpdateDownloaded(true);

        assertFalse(startTask());
        assertSkippedWithoutCallingPlay();
    }

    @Test
    public void testOnStopTask_returnsFalse() {
        assertFalse(mTask.onStopTask(mContext, mTaskParameters));
    }

    // A Play error other than download-not-present does not say the download is gone, so it counts
    // as a failure and the flag is restored for a later attempt.
    @Test
    public void testOnStartTask_completeUpdate_clearsFlagEagerlyAndRestoresOnFailure() {
        var watcher =
                expectEvents(
                        InAppUpdateBackgroundRestartEvent.COMPLETE_UPDATE_CALLED,
                        InAppUpdateBackgroundRestartEvent.COMPLETE_UPDATE_FAILED);
        TaskCompletionSource<Void> source = startTaskWithPendingCompleteUpdate();

        source.setException(new InstallException(InstallErrorCode.ERROR_INSTALL_NOT_ALLOWED));
        ShadowLooper.idleMainLooper();

        assertTrue(InAppUpdatePolicy.isUpdateDownloaded());
        verify(mMockCallback).taskFinished(false);
        watcher.assertExpected();
    }

    // A stopped task reports nothing further, and the flag still follows Play's answer: it stays
    // cleared when Play holds no download.
    @Test
    public void testOnStopTask_duringCompleteUpdate_keepsFlagClearedWhenPlayHoldsNoDownload() {
        var watcher = expectEvents(InAppUpdateBackgroundRestartEvent.COMPLETE_UPDATE_CALLED);
        TaskCompletionSource<Void> source = startTaskWithPendingCompleteUpdate();

        mTask.onStopTask(mContext, mTaskParameters);
        source.setException(new InstallException(InstallErrorCode.ERROR_DOWNLOAD_NOT_PRESENT));
        ShadowLooper.idleMainLooper();

        assertFalse(InAppUpdatePolicy.isUpdateDownloaded());
        verify(mMockCallback, never()).taskFinished(anyBoolean());
        watcher.assertExpected();
    }

    @Test
    public void testOnStopTask_duringCompleteUpdate_stillRestoresFlagOnFailure() {
        TaskCompletionSource<Void> source = startTaskWithPendingCompleteUpdate();

        mTask.onStopTask(mContext, mTaskParameters);
        source.setException(new RuntimeException("Install failure"));
        ShadowLooper.idleMainLooper();

        // Flag restored even though the task was stopped; taskFinished() must not be called.
        assertTrue(InAppUpdatePolicy.isUpdateDownloaded());
        verify(mMockCallback, never()).taskFinished(anyBoolean());
    }

    @Test
    public void testOnStartTask_foregroundServiceActive_skipsAndSchedulesBoundedRetry() {
        InAppUpdatePolicy.setUpdateDownloaded(true);
        InAppUpdatePolicy.setProcessImportanceForTesting(
                ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND_SERVICE);
        when(mMockScheduler.schedule(any(), any())).thenReturn(true);

        var watcher =
                expectEvents(
                        InAppUpdateBackgroundRestartEvent.SKIPPED_FOREGROUND_SERVICE,
                        InAppUpdateBackgroundRestartEvent.RETRY_SCHEDULED);

        assertFalse(startTask());
        // Retry must be posted after onStartTask() returns false so BackgroundTaskJobService
        // finishes cleaning up before the same job ID is rescheduled.
        verify(mMockScheduler, never()).schedule(any(), any());

        ShadowLooper.idleMainLooper();

        assertSkippedWithoutCallingPlay();
        ArgumentCaptor<TaskInfo> captor = ArgumentCaptor.forClass(TaskInfo.class);
        verify(mMockScheduler).schedule(eq(mContext), captor.capture());
        assertScheduledRetry(
                captor.getValue(),
                InAppUpdateRestartScheduler.FOREGROUND_SERVICE_RETRY_INITIAL_DELAY_MINUTES,
                1);
        watcher.assertExpected();
    }

    // The retry count carried in the task extras must feed the backoff, so a later retry waits
    // longer than the first one.
    @Test
    public void testOnStartTask_foregroundServiceActiveOnRetry_usesRetryCountForBackoff() {
        InAppUpdatePolicy.setUpdateDownloaded(true);
        InAppUpdatePolicy.setProcessImportanceForTesting(
                ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND_SERVICE);
        when(mMockScheduler.schedule(any(), any())).thenReturn(true);
        PersistableBundle extras = new PersistableBundle();
        extras.putInt(InAppUpdateRestartScheduler.EXTRA_RETRY_COUNT, 2);
        TaskParameters params =
                TaskParameters.create(TaskIds.IN_APP_UPDATE_RESTART_JOB_ID)
                        .addExtras(extras)
                        .build();

        assertFalse(mTask.onStartTask(mContext, params, mMockCallback));
        ShadowLooper.idleMainLooper();

        assertSkippedWithoutCallingPlay();
        ArgumentCaptor<TaskInfo> captor = ArgumentCaptor.forClass(TaskInfo.class);
        verify(mMockScheduler).schedule(eq(mContext), captor.capture());
        assertScheduledRetry(captor.getValue(), 60, 3);
    }

    /**
     * Starts the task with a staged update whose completeUpdate() stays pending, and verifies the
     * downloaded flag is cleared as soon as completeUpdate() is invoked, before any result.
     */
    private TaskCompletionSource<Void> startTaskWithPendingCompleteUpdate() {
        InAppUpdatePolicy.setUpdateDownloaded(true);
        TaskCompletionSource<Void> source = new TaskCompletionSource<>();
        when(mMockAppUpdateManager.completeUpdate()).thenReturn(source.getTask());

        assertTrue(startTask());
        ShadowLooper.idleMainLooper();

        verify(mMockAppUpdateManager).completeUpdate();
        assertFalse(InAppUpdatePolicy.isUpdateDownloaded());
        verify(mMockCallback, never()).taskFinished(anyBoolean());
        return source;
    }
}

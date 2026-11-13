// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import com.google.android.gms.tasks.Task;
import com.google.android.play.core.appupdate.AppUpdateInfo;
import com.google.android.play.core.appupdate.AppUpdateManager;
import com.google.android.play.core.appupdate.AppUpdateManagerFactory;
import com.google.android.play.core.install.model.InstallStatus;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omaha.inline.InAppUpdatePolicy.RestartMode;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.TaskParameters;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Background task that checks whether an in-app update is still downloaded and Chrome has no
 * visible activities, incognito tabs, or active foreground services, then invokes {@link
 * AppUpdateManager#completeUpdate()} to silently install the staged update while Chrome is in the
 * background. An active foreground service is the only non-terminal skip: the task finishes and
 * reschedules itself with capped exponential backoff.
 *
 * <p>Note: This task runs in a cold background JobScheduler process without native libraries
 * loaded. Never call native-backed {@code ChromeFeatureList} methods (such as {@code isEnabled()}
 * or {@code getFieldTrialParam*()}) or other native/{@code Profile} code; use {@code CachedFlag},
 * {@code CachedFeatureParam}, and {@code SharedPreferences} instead.
 *
 * <p>Events recorded by this task in a cold Java-only process may be lost if Google Play replaces
 * the package and kills the process before native initializes in that process, so
 * CompleteUpdateCalled undercounts the dismiss-from-Recents case. The final install outcome should
 * be measured at the next launch.
 */
@NullMarked
public class InAppUpdateRestartTask implements BackgroundTask {
    @VisibleForTesting
    static final String HISTOGRAM_EVENT = "Android.InAppUpdate.BackgroundRestart.Event";

    // Same tag as the rest of the in-app update flow so one logcat filter covers it.
    private static final String TAG = "InAppUpdateFlow";

    @IntDef({
        PreconditionResult.OK,
        PreconditionResult.SKIPPED_TERMINAL,
        PreconditionResult.SKIPPED_FOREGROUND_SERVICE
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface PreconditionResult {
        int OK = 0;
        int SKIPPED_TERMINAL = 1;
        int SKIPPED_FOREGROUND_SERVICE = 2;
    }

    private static @Nullable AppUpdateManager sAppUpdateManagerForTesting;

    // BackgroundTaskJobService removes the task on stop; a late taskFinished() hits
    // "non-current task".
    private boolean mStopped;

    public static void setAppUpdateManagerForTesting(@Nullable AppUpdateManager manager) {
        sAppUpdateManagerForTesting = manager;
        ResettersForTesting.register(() -> sAppUpdateManagerForTesting = null);
    }

    private static AppUpdateManager getAppUpdateManager(Context context) {
        if (sAppUpdateManagerForTesting != null) {
            return sAppUpdateManagerForTesting;
        }
        return AppUpdateManagerFactory.create(context);
    }

    @Override
    public boolean onStartTask(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        InAppUpdateRestartScheduler.onTaskStarted();
        Log.i(TAG, "InAppUpdateRestartTask.onStartTask");

        int retryCount =
                taskParameters.getExtras().getInt(InAppUpdateRestartScheduler.EXTRA_RETRY_COUNT, 0);

        @PreconditionResult int preconditions = checkPreconditions();
        if (preconditions != PreconditionResult.OK) {
            if (preconditions == PreconditionResult.SKIPPED_FOREGROUND_SERVICE) {
                // Post after returning false so BackgroundTaskJobService finishes tearing down the
                // current job before the same job ID is rescheduled.
                ThreadUtils.postOnUiThread(
                        () ->
                                InAppUpdateRestartScheduler.scheduleForegroundServiceRetry(
                                        retryCount));
            }
            return false;
        }

        AppUpdateManager manager = getAppUpdateManager(context);
        try {
            manager.getAppUpdateInfo()
                    .addOnCompleteListener(
                            task -> onAppUpdateInfoResult(manager, task, callback, retryCount));
        } catch (RuntimeException e) {
            Log.w(TAG, "InAppUpdateRestartTask: Failed to request AppUpdateInfo", e);
            recordEvent(InAppUpdateBackgroundRestartEvent.GET_APP_UPDATE_INFO_FAILED);
            return false;
        }

        return true;
    }

    @Override
    public boolean onStopTask(Context context, TaskParameters taskParameters) {
        Log.i(TAG, "InAppUpdateRestartTask.onStopTask");
        mStopped = true;
        return false;
    }

    private void onAppUpdateInfoResult(
            AppUpdateManager manager,
            Task<AppUpdateInfo> task,
            TaskFinishedCallback callback,
            int retryCount) {
        if (mStopped) {
            return;
        }

        if (!task.isSuccessful()) {
            Exception e = task.getException();
            if (e != null) {
                Log.w(TAG, "InAppUpdateRestartTask: Failed to get AppUpdateInfo", e);
            } else {
                Log.w(TAG, "InAppUpdateRestartTask: Failed to get AppUpdateInfo");
            }
            recordEvent(InAppUpdateBackgroundRestartEvent.GET_APP_UPDATE_INFO_FAILED);
            callback.taskFinished(false);
            return;
        }

        AppUpdateInfo appUpdateInfo = task.getResult();
        int status = appUpdateInfo.installStatus();
        if (status != InstallStatus.DOWNLOADED) {
            Log.i(
                    TAG,
                    "InAppUpdateRestartTask: Status is %d (not DOWNLOADED) -> clearing flag.",
                    status);
            InAppUpdatePolicy.setUpdateDownloaded(false);
            recordEvent(InAppUpdateBackgroundRestartEvent.SKIPPED_NOT_DOWNLOADED);
            callback.taskFinished(false);
            return;
        }

        // Re-check preconditions immediately before invoking completeUpdate() to prevent TOCTOU
        // races (e.g. an Activity becoming visible or a foreground service starting while
        // getAppUpdateInfo() was in flight).
        @PreconditionResult int preconditions = checkPreconditions();
        if (preconditions != PreconditionResult.OK) {
            callback.taskFinished(false);
            if (preconditions == PreconditionResult.SKIPPED_FOREGROUND_SERVICE) {
                InAppUpdateRestartScheduler.scheduleForegroundServiceRetry(retryCount);
            }
            return;
        }

        invokeCompleteUpdate(manager, callback);
    }

    private void invokeCompleteUpdate(AppUpdateManager manager, TaskFinishedCallback callback) {
        Log.i(TAG, "InAppUpdateRestartTask: Status is DOWNLOADED -> calling completeUpdate().");
        recordEvent(InAppUpdateBackgroundRestartEvent.COMPLETE_UPDATE_CALLED);

        try {
            InAppUpdateInstaller.completeUpdate(manager)
                    .addOnCompleteListener(
                            completeTask -> onCompleteUpdateResult(completeTask, callback));
        } catch (RuntimeException e) {
            Log.e(TAG, "InAppUpdateRestartTask: Failed to call completeUpdate", e);
            recordEvent(InAppUpdateBackgroundRestartEvent.COMPLETE_UPDATE_FAILED);
            callback.taskFinished(false);
        }
    }

    private void onCompleteUpdateResult(Task<Void> completeTask, TaskFinishedCallback callback) {
        if (mStopped) {
            return;
        }

        if (completeTask.isSuccessful()) {
            Log.i(TAG, "InAppUpdateRestartTask: completeUpdate() succeeded.");
        } else {
            Exception completeException = completeTask.getException();
            if (completeException != null) {
                Log.e(TAG, "InAppUpdateRestartTask: completeUpdate() failed", completeException);
            } else {
                Log.e(TAG, "InAppUpdateRestartTask: completeUpdate() failed");
            }
            recordEvent(InAppUpdateBackgroundRestartEvent.COMPLETE_UPDATE_FAILED);
        }
        callback.taskFinished(false);
    }

    private static @PreconditionResult int checkPreconditions() {
        if (!InAppUpdatePolicy.isUpdateDownloaded()) {
            Log.i(TAG, "InAppUpdateRestartTask: Update not downloaded.");
            recordEvent(InAppUpdateBackgroundRestartEvent.SKIPPED_NOT_DOWNLOADED);
            return PreconditionResult.SKIPPED_TERMINAL;
        }

        @RestartMode int mode = InAppUpdatePolicy.getRestartMode();
        if (mode == RestartMode.SNACKBAR) {
            Log.i(TAG, "InAppUpdateRestartTask: Mode is snackbar, skipping.");
            return PreconditionResult.SKIPPED_TERMINAL;
        }

        // The browser process initializes ApplicationStatus in Application.onCreate, so this is
        // defensive; an uninitialized ApplicationStatus means no activity exists in this process.
        if (ApplicationStatus.isInitialized() && ApplicationStatus.hasVisibleActivities()) {
            Log.i(TAG, "InAppUpdateRestartTask: Activity visible, skipping.");
            recordEvent(InAppUpdateBackgroundRestartEvent.SKIPPED_VISIBLE_AT_RUN);
            return PreconditionResult.SKIPPED_TERMINAL;
        }

        if (IncognitoTabHostUtils.doIncognitoTabsExist()) {
            Log.i(TAG, "InAppUpdateRestartTask: Incognito tabs open, skipping.");
            recordEvent(InAppUpdateBackgroundRestartEvent.SKIPPED_INCOGNITO);
            return PreconditionResult.SKIPPED_TERMINAL;
        }

        if (InAppUpdatePolicy.isForegroundServiceActive()) {
            Log.i(TAG, "InAppUpdateRestartTask: Foreground service active, skipping.");
            recordEvent(InAppUpdateBackgroundRestartEvent.SKIPPED_FOREGROUND_SERVICE);
            return PreconditionResult.SKIPPED_FOREGROUND_SERVICE;
        }

        return PreconditionResult.OK;
    }

    /** Records to {@link #HISTOGRAM_EVENT}; shared with {@link InAppUpdateRestartScheduler}. */
    static void recordEvent(@InAppUpdateBackgroundRestartEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_EVENT, event, InAppUpdateBackgroundRestartEvent.COUNT);
    }
}

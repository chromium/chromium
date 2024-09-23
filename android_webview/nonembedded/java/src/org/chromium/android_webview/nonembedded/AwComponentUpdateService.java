// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.nonembedded;

import android.app.job.JobParameters;
import android.app.job.JobService;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.ResultReceiver;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.devui.ComponentsListFragment;
import org.chromium.android_webview.services.ComponentUpdaterSafeModeUtils;
import org.chromium.android_webview.services.ComponentsProviderPathUtil;
import org.chromium.base.Callback;
import org.chromium.base.FileUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;

import java.io.File;

/**
 * Background service that launches native component_updater::ComponentUpdateService and component
 * registration. It has to be launched via JobScheduler. This is a JobService rather just a Service
 * because the new restrictions introduced in Android O+ on background execution.
 */
// TODO(ntfschr): consider using BackgroundTaskScheduler
@JNINamespace("android_webview")
public class AwComponentUpdateService extends JobService {
    private static final String TAG = "AwCUS";

    private static SharedPreferences sSharedPreferences;

    private ResultReceiver mFinishCallback;

    // Histogram names.
    public static final String HISTOGRAM_COMPONENT_UPDATER_CPS_DIRECTORY_SIZE =
            "Android.WebView.ComponentUpdater.CPSDirectorySize";
    public static final String HISTOGRAM_COMPONENT_UPDATER_CUS_DIRECTORY_SIZE =
            "Android.WebView.ComponentUpdater.CUSDirectorySize";
    public static final String HISTOGRAM_COMPONENT_UPDATER_UPDATE_JOB_DURATION =
            "Android.WebView.ComponentUpdater.UpdateJobDuration";
    public static final String HISTOGRAM_AW_COMPONENT_UPDATE_SERVICE_FILES_CHANGED =
            "Android.WebView.ComponentUpdater.UpdateJobFilesChanged";
    public static final String HISTOGRAM_COMPONENT_UPDATER_UNEXPECTED_EXIT =
            "Android.WebView.ComponentUpdater.UnexpectedExit";

    private static final int BYTES_PER_KILOBYTE = 1024;
    private static final int DIRECTORY_SIZE_MIN_BUCKET = 100;
    private static final int DIRECTORY_SIZE_MAX_BUCKET = 500000;
    private static final int DIRECTORY_SIZE_NUM_BUCKETS = 50;

    @VisibleForTesting
    public static final String SHARED_PREFERENCES_NAME = "AwComponentUpdateServicePreferences";

    @VisibleForTesting public static final String KEY_UNEXPECTED_EXIT = "UnexpectedExit";

    /**
     * The service can be both started by {@link android.app.job.JobScheduler} as a {@link
     * JobService} and as a started service by calling {@link Context#startService}. These two
     * states can apply at the same time. The service won't stop until all necessary stop
     * methods are called:
     * - Calling jobFinished if it's launched as a JobService.
     * - Calling stopSelf if it's launched as a start service.
     */
    // If it has a non zero value, then the service is running via onStartCommand.
    private int mServiceStartedId;

    // If not null then the service is running as a Job service.
    private JobParameters mJobParameters;

    private boolean mIsUpdating;

    // Called by JobScheduler.
    @Override
    public boolean onStartJob(JobParameters params) {
        assert mJobParameters == null;
        mJobParameters = params;
        return maybeStartUpdates(/* onDemandUpdate= */ false);
    }

    // Called by JobScheduler.
    @Override
    public boolean onStopJob(JobParameters params) {
        ComponentUpdaterSafeModeUtils.executeSafeModeIfEnabled(
                new File(ComponentsProviderPathUtil.getComponentUpdateServiceDirectoryPath()));

        // TODO(crbug.com/40773291): Stop native updates when onStopJob, onDestroy are
        // called.

        setUnexpectedExit(false);
        mJobParameters = null;

        // This should only be called if the service needs to be shut down before we've called
        // jobFinished. Request reschedule so we can finish downloading component updates.
        return
        /* reschedule= */ true;
    }

    /**
     * Overridden to manually start the service via devui {@link
     * org.chromium.android_webview.devui.ComponentsListFragment}. The service isn't exported, so
     * other apps won't be able to force start the service.
     *
     * The service accepts a {@link ResultReceiver} callback in the intent which will be called
     * when the service finishes updating and/or being stopped.
     */
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // Always keep the most recent startId as this is the one that should be used to stop
        // the service.
        mServiceStartedId = startId;
        mFinishCallback =
                IntentUtils.safeGetParcelableExtra(
                        intent, ComponentsListFragment.SERVICE_FINISH_CALLBACK);
        boolean onDemandUpdate =
                IntentUtils.safeGetBooleanExtra(
                        intent, ComponentsListFragment.ON_DEMAND_UPDATE_REQUEST, false);
        if (!maybeStartUpdates(onDemandUpdate)) {
            stopSelf(startId);
            mServiceStartedId = 0;
        }
        return START_STICKY;
    }

    /**
     * Start component updates by triggerring native AwComponentUpdateService.
     *
     * @return {@code true} if it successfully triggers component updates or if component are
     *         already updating, {@code false} if it fails to trigger the updates.
     */
    @VisibleForTesting
    public boolean maybeStartUpdates(boolean onDemandUpdate) {
        if (mIsUpdating) {
            return true;
        }

        if (ComponentUpdaterSafeModeUtils.executeSafeModeIfEnabled(
                new File(ComponentsProviderPathUtil.getComponentUpdateServiceDirectoryPath()))) {
            return false;
        }

        maybeRecordUnexpectedExit();

        // TODO(http://crbug.com/1179297) look at doing this in a task on a background thread
        // instead of the main thread.
        if (WebViewApkApplication.ensureNativeInitialized()) {
            setUnexpectedExit(true);
            mIsUpdating = true;
            final long startTime = SystemClock.uptimeMillis();
            // TODO(crbug.com/40745317) Once we can log UMA from native, remove the count parameter.
            AwComponentUpdateServiceJni.get()
                    .startComponentUpdateService(
                            (count) -> {
                                recordJobDuration(SystemClock.uptimeMillis() - startTime);
                                recordFilesChanged(count);
                                recordDirectorySize();
                                setUnexpectedExit(false);
                                stopService();
                            },
                            onDemandUpdate);
            return true;
        }
        Log.e(TAG, "couldn't init native, aborting starting AwComponentUpdaterService");
        return false;
    }

    // Call the appropriate stop method according to how the service is launched.
    private void stopService() {
        mIsUpdating = false;

        if (mFinishCallback != null) {
            mFinishCallback.send(0, null);
            mFinishCallback = null;
        }

        // Service is launched as a started service.
        if (mServiceStartedId > 0) {
            stopSelf(mServiceStartedId);
            mServiceStartedId = 0;
        }
        // Service is launched as a job service.
        if (mJobParameters != null) {
            jobFinished(mJobParameters, /* needReschedule= */ false);
            mJobParameters = null;
        }
    }

    private void recordDirectorySize() {
        final long cpsSize =
                FileUtils.getFileSizeBytes(
                        new File(ComponentsProviderPathUtil.getComponentsServingDirectoryPath()));
        final long cusSize =
                FileUtils.getFileSizeBytes(
                        new File(
                                ComponentsProviderPathUtil
                                        .getComponentUpdateServiceDirectoryPath()));
        recordDirectorySize(HISTOGRAM_COMPONENT_UPDATER_CPS_DIRECTORY_SIZE, cpsSize);
        recordDirectorySize(HISTOGRAM_COMPONENT_UPDATER_CUS_DIRECTORY_SIZE, cusSize);
    }

    private void recordDirectorySize(String histogramName, long sizeBytes) {
        UmaRecorderHolder.get()
                .recordExponentialHistogram(
                        histogramName,
                        (int) (sizeBytes / BYTES_PER_KILOBYTE),
                        DIRECTORY_SIZE_MIN_BUCKET,
                        DIRECTORY_SIZE_MAX_BUCKET,
                        DIRECTORY_SIZE_NUM_BUCKETS);
    }

    private void recordJobDuration(long duration) {
        RecordHistogram.recordTimesHistogram(
                HISTOGRAM_COMPONENT_UPDATER_UPDATE_JOB_DURATION, duration);
    }

    private void recordFilesChanged(int filesChanged) {
        RecordHistogram.recordCount1000Histogram(
                HISTOGRAM_AW_COMPONENT_UPDATE_SERVICE_FILES_CHANGED, filesChanged);
    }

    private void maybeRecordUnexpectedExit() {
        final SharedPreferences sharedPreferences =
                sSharedPreferences != null
                        ? sSharedPreferences
                        : getSharedPreferences(SHARED_PREFERENCES_NAME, Context.MODE_PRIVATE);
        if (sharedPreferences.contains(KEY_UNEXPECTED_EXIT)) {
            RecordHistogram.recordBooleanHistogram(
                    HISTOGRAM_COMPONENT_UPDATER_UNEXPECTED_EXIT,
                    sharedPreferences.getBoolean(KEY_UNEXPECTED_EXIT, false));
        }
    }

    private void setUnexpectedExit(boolean unfinished) {
        final SharedPreferences sharedPreferences =
                sSharedPreferences != null
                        ? sSharedPreferences
                        : getSharedPreferences(SHARED_PREFERENCES_NAME, Context.MODE_PRIVATE);
        sharedPreferences.edit().putBoolean(KEY_UNEXPECTED_EXIT, unfinished).apply();
    }

    @VisibleForTesting
    public static void setSharedPreferences(SharedPreferences sharedPreferences) {
        sSharedPreferences = sharedPreferences;
    }

    @NativeMethods
    interface Natives {
        void startComponentUpdateService(
                Callback<Integer> finishedCallback, boolean onDemandUpdate);
    }
}

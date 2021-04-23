// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.nonembedded;

import android.app.job.JobParameters;
import android.app.job.JobService;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.SystemClock;

import org.chromium.android_webview.services.ComponentsProviderPathUtil;
import org.chromium.base.Callback;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;

import java.io.File;

/**
 * Background service that launches native component_updater::ComponentUpdateService and component
 * registration. It has to be launched via JobScheduler. This is a JobService rather just a Service
 * because the new restrictions introduced in Android O+ on background execution.
 */
@JNINamespace("android_webview")
public class AwComponentUpdateService extends JobService {
    private static final String TAG = "AwCUS";

    // Histogram names.
    public static final String HISTOGRAM_COMPONENT_UPDATER_CPS_DIRECTORY_SIZE =
            "Android.WebView.ComponentUpdater.CPSDirectorySize";
    public static final String HISTOGRAM_COMPONENT_UPDATER_CUS_DIRECTORY_SIZE =
            "Android.WebView.ComponentUpdater.CUSDirectorySize";
    public static final String HISTOGRAM_COMPONENT_UPDATER_UNEXPECTED_EXIT =
            "Android.WebView.ComponentUpdater.UnexpectedExit";
    public static final String HISTOGRAM_COMPONENT_UPDATER_UPDATE_JOB_DURATION =
            "Android.WebView.ComponentUpdater.UpdateJobDuration";
    public static final String HISTOGRAM_AW_COMPONENT_UPDATE_SERVICE_FILES_CHANGED =
            "Android.WebView.ComponentUpdater.UpdateJobFilesChanged";

    private static final int BYTES_PER_KILOBYTE = 1024;
    private static final int DIRECTORY_SIZE_MIN_BUCKET = 100;
    private static final int DIRECTORY_SIZE_MAX_BUCKET = 500000;
    private static final int DIRECTORY_SIZE_NUM_BUCKETS = 50;
    private static final String SHARED_PREFERENCES_NAME = "AwComponentUpdateServicePreferences";
    private static final String KEY_UNEXPECTED_EXIT = "UnexpectedExit";

    @Override
    public boolean onStartJob(JobParameters params) {
        maybeRecordUnexpectedExit();

        // TODO(http://crbug.com/1179297) look at doing this in a task on a background thread
        // instead of the main thread.
        if (WebViewApkApplication.initializeNative()) {
            setUnexpectedExit(true);
            final long startTime = SystemClock.uptimeMillis();
            // TODO(crbug.com/1171817) Once we can log UMA from native, remove the count parameter.
            AwComponentUpdateServiceJni.get().startComponentUpdateService((count) -> {
                recordJobDuration(SystemClock.uptimeMillis() - startTime);
                recordFilesChanged(count);
                recordDirectorySize();
                jobFinished(params, /* needReschedule= */ false);
                setUnexpectedExit(false);
            });
            return true;
        }
        Log.e(TAG, "couldn't init native, aborting starting AwComponentUpdaterService");
        return false;
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        setUnexpectedExit(false);

        // This should only be called if the service needs to be shut down before we've called
        // jobFinished. Request reschedule so we can finish downloading component updates.
        return /*reschedule= */ true;
    }

    private void recordDirectorySize() {
        final long cpsSize = FileUtils.getFileSizeBytes(
                new File(ComponentsProviderPathUtil.getComponentsServingDirectoryPath()));
        final long cusSize = FileUtils.getFileSizeBytes(
                new File(ComponentsProviderPathUtil.getComponentUpdateServiceDirectoryPath()));
        recordDirectorySize(HISTOGRAM_COMPONENT_UPDATER_CPS_DIRECTORY_SIZE, cpsSize);
        recordDirectorySize(HISTOGRAM_COMPONENT_UPDATER_CUS_DIRECTORY_SIZE, cusSize);
    }

    private void recordDirectorySize(String histogramName, long sizeBytes) {
        UmaRecorderHolder.get().recordExponentialHistogram(histogramName,
                (int) (sizeBytes / BYTES_PER_KILOBYTE), DIRECTORY_SIZE_MIN_BUCKET,
                DIRECTORY_SIZE_MAX_BUCKET, DIRECTORY_SIZE_NUM_BUCKETS);
    }

    private void maybeRecordUnexpectedExit() {
        final SharedPreferences sharedPreferences =
                getSharedPreferences(SHARED_PREFERENCES_NAME, Context.MODE_PRIVATE);
        if (sharedPreferences.contains(KEY_UNEXPECTED_EXIT)) {
            RecordHistogram.recordBooleanHistogram(HISTOGRAM_COMPONENT_UPDATER_UNEXPECTED_EXIT,
                    sharedPreferences.getBoolean(KEY_UNEXPECTED_EXIT, false));
        }
    }

    private void setUnexpectedExit(boolean unfinished) {
        final SharedPreferences sharedPreferences =
                getSharedPreferences(SHARED_PREFERENCES_NAME, Context.MODE_PRIVATE);
        sharedPreferences.edit().putBoolean(KEY_UNEXPECTED_EXIT, unfinished).apply();
    }

    private void recordJobDuration(long duration) {
        RecordHistogram.recordTimesHistogram(
                HISTOGRAM_COMPONENT_UPDATER_UPDATE_JOB_DURATION, duration);
    }

    private void recordFilesChanged(int filesChanged) {
        RecordHistogram.recordCount1000Histogram(
                HISTOGRAM_AW_COMPONENT_UPDATE_SERVICE_FILES_CHANGED, filesChanged);
    }

    @NativeMethods
    interface Natives {
        void startComponentUpdateService(Callback<Integer> finishedCallback);
    }
}

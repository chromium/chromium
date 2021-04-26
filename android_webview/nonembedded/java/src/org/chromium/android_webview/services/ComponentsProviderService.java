// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.app.Service;
import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.ResultReceiver;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.compat.ApiHelperForN;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.component_updater.IComponentsProviderService;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.Future;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

/**
 * A Service to fetch Components files in WebView and WebLayer.
 */
public class ComponentsProviderService extends Service {
    // Result receiver constants.
    public static final int RESULT_OK = 0;
    public static final int RESULT_FAILED = 1;
    public static final String KEY_RESULT = "RESULT";

    // Histogram names.
    public static final String HISTOGRAM_GET_FILES_RESULT =
            "Android.WebView.ComponentUpdater.GetFilesResult";
    public static final String HISTOGRAM_GET_FILES_DURATION =
            "Android.WebView.ComponentUpdater.GetFilesDuration";

    private static final String TAG = "AW_CPS";

    // This should be greater than or equal to the native component updater service interval.
    private static final long UPDATE_INTERVAL_MS = TimeUnit.HOURS.toMillis(5);
    private static final int JOB_ID = TaskIds.WEBVIEW_COMPONENT_UPDATE_JOB_ID;

    // UMA constants. These values are persisted to logs. Entries can't be reordered and numbers
    // can't be reused.
    @IntDef({GetFilesResultCode.SUCCESS, GetFilesResultCode.FAILED_NOT_INSTALLED,
            GetFilesResultCode.FAILED_NO_VERSIONS, GetFilesResultCode.FAILED_NO_FDS,
            GetFilesResultCode.FAILED_OPENING_FDS})
    private @interface GetFilesResultCode {
        int SUCCESS = 0;
        int FAILED_NOT_INSTALLED = 1;
        int FAILED_NO_VERSIONS = 2;
        int FAILED_NO_FDS = 3;
        int FAILED_OPENING_FDS = 4;
        // Keep this one at the end and increment appropriately when adding new entries.
        int COUNT = 5;
    }

    private File mDirectory;
    private FutureTask<Void> mDeleteTask;

    private final IBinder mBinder = new IComponentsProviderService.Stub() {
        @Override
        public void getFilesForComponent(String componentId, ResultReceiver resultReceiver) {
            final long startTime = System.currentTimeMillis();

            // Note that there's no need to sanitize input because this method will check if there
            // is an existing folder under `mDirectory` with a name that equals the received
            // `componentId`. Because `mDirectory` is inside this application's data dir, only
            // WebView can modify it.
            final File[] components = mDirectory.listFiles((dir, name) -> name.equals(componentId));
            if (components == null || components.length == 0) {
                resultReceiver.send(RESULT_FAILED, /* resultData = */ null);
                recordGetFilesResultAndDuration(GetFilesResultCode.FAILED_NOT_INSTALLED, startTime);
                return;
            }
            assert components.length
                    == 1 : "Only one directory should have the name " + componentId;

            final File[] versions =
                    ComponentsProviderPathUtil.getComponentsNewestFirst(components[0]);
            if (versions == null || versions.length == 0) {
                // This can happen if CUS created a parent directory but was killed before it could
                // move content into it. In this case there's nothing old to delete.
                resultReceiver.send(RESULT_FAILED, /* resultData = */ null);
                recordGetFilesResultAndDuration(GetFilesResultCode.FAILED_NO_VERSIONS, startTime);
                return;
            }
            final File versionDirectory = versions[0];

            final HashMap<String, ParcelFileDescriptor> resultMap = new HashMap<>();
            try {
                recursivelyGetParcelFileDescriptors(
                        versionDirectory, versionDirectory.getAbsolutePath() + "/", resultMap);

                if (resultMap.isEmpty()) {
                    Log.w(TAG, "No file descriptors found for " + componentId);
                    resultReceiver.send(RESULT_FAILED, /* resultData = */ null);
                    recordGetFilesResultAndDuration(GetFilesResultCode.FAILED_NO_FDS, startTime);
                    return;
                }

                final Bundle resultData = new Bundle();
                resultData.putSerializable(KEY_RESULT, resultMap);
                resultReceiver.send(RESULT_OK, resultData);
                recordGetFilesResultAndDuration(GetFilesResultCode.SUCCESS, startTime);
            } catch (IOException exception) {
                Log.w(TAG, exception.getMessage(), exception);
                resultReceiver.send(RESULT_FAILED, /* resultData = */ null);
                recordGetFilesResultAndDuration(GetFilesResultCode.FAILED_OPENING_FDS, startTime);
            } finally {
                closeFileDescriptors(resultMap);
            }
        }
    };

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    @Override
    public void onCreate() {
        mDirectory = new File(ComponentsProviderPathUtil.getComponentsServingDirectoryPath());
        if (!mDirectory.exists() && !mDirectory.mkdirs()) {
            Log.e(TAG, "Failed to create directory " + mDirectory.getAbsolutePath());
            return;
        }

        cleanupOlderFiles();
        maybeScheduleComponentUpdateService();
    }

    private void cleanupOlderFiles() {
        final File[] components = mDirectory.listFiles();
        if (components == null || components.length == 0) {
            return;
        }
        final List<File> oldFiles = new LinkedList<>();
        for (File component : components) {
            final File[] versions = ComponentsProviderPathUtil.getComponentsNewestFirst(component);
            if (versions == null || versions.length == 0) {
                // This can happen if CUS created a parent directory but was killed before it could
                // move content into it. In this case there's nothing old to delete.
                continue;
            }
            // Add all directories except the newest (index 0) to oldFiles.
            oldFiles.addAll(Arrays.asList(versions).subList(1, versions.length));
        }

        // Delete old files in background.
        mDeleteTask = new FutureTask<>(() -> {
            for (File file : oldFiles) {
                if (!FileUtils.recursivelyDeleteFile(file, null)) {
                    Log.w(TAG, "Failed to delete " + file.getAbsolutePath());
                }
            }
            return null;
        });
        PostTask.postTask(TaskTraits.THREAD_POOL_BEST_EFFORT, mDeleteTask);
    }

    /**
     * This must be called after {@code onCreate()}, otherwise returns a {@code null} object.
     */
    @VisibleForTesting
    public Future<Void> getDeleteTaskForTesting() {
        return mDeleteTask;
    }

    private void recursivelyGetParcelFileDescriptors(File file, String pathPrefix,
            HashMap<String, ParcelFileDescriptor> resultMap) throws IOException {
        if (file.isDirectory()) {
            File[] files = file.listFiles();
            if (files != null) {
                for (File f : files) {
                    recursivelyGetParcelFileDescriptors(f, pathPrefix, resultMap);
                }
            }
        } else {
            resultMap.put(file.getAbsolutePath().replace(pathPrefix, ""),
                    ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY));
        }
    }

    private void closeFileDescriptors(HashMap<String, ParcelFileDescriptor> map) {
        assert map != null : "closeFileDescriptors called with a null map";

        for (ParcelFileDescriptor fileDescriptor : map.values()) {
            try {
                fileDescriptor.close();
            } catch (IOException exception) {
                Log.w(TAG, exception.getMessage());
            }
        }
    }

    private void maybeScheduleComponentUpdateService() {
        Context context = ContextUtils.getApplicationContext();
        JobScheduler jobScheduler =
                (JobScheduler) context.getSystemService(Context.JOB_SCHEDULER_SERVICE);

        try {
            if (isJobScheduled(jobScheduler, JOB_ID)) {
                return;
            }

            ComponentName componentName =
                    new ComponentName(context, ServiceNames.AW_COMPONENT_UPDATE_SERVICE);

            JobInfo job = new JobInfo.Builder(JOB_ID, componentName)
                                  .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                                  .setPeriodic(UPDATE_INTERVAL_MS)
                                  .build();

            if (jobScheduler.schedule(job) != JobScheduler.RESULT_SUCCESS) {
                Log.e(TAG, "Failed to schedule job for AwComponentUpdateService");
            }
        } catch (Exception exception) {
            Log.e(TAG, "Error scheduling job for AwComponentUpdateService", exception);
        }
    }

    // Use JobScheduler.getPendingJob() if it's available. Otherwise, fall back to iterating over
    // all jobs to find the one we want.
    // TODO(crbug.com/1189126): move this to utils class
    @VisibleForTesting
    public static boolean isJobScheduled(JobScheduler scheduler, int jobId) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            for (JobInfo info : scheduler.getAllPendingJobs()) {
                if (info.getId() == jobId) return true;
            }
            return false;
        }
        return ApiHelperForN.getPendingJob(scheduler, jobId) != null;
    }

    private void recordGetFilesResultAndDuration(@GetFilesResultCode int result, long startTime) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_GET_FILES_RESULT, result, GetFilesResultCode.COUNT);
        RecordHistogram.recordTimesHistogram(
                HISTOGRAM_GET_FILES_DURATION, System.currentTimeMillis() - startTime);
    }
}

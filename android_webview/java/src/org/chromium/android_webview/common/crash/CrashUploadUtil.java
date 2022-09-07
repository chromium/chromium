// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.crash;

import android.app.job.JobInfo;
import android.content.ComponentName;
import android.content.Context;
import android.net.ConnectivityManager;

import androidx.annotation.NonNull;
import androidx.annotation.UiThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.base.Log;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.minidump_uploader.MinidumpUploadJobService;
import org.chromium.components.minidump_uploader.util.NetworkPermissionUtil;

import java.io.File;

/**
 * A util class for request uploading crash reports.
 */
public final class CrashUploadUtil {
    private static final String TAG = "CrashUploadUtil";

    /**
     * Delegate interface to mock network status check and scheduling upload jobs for testing.
     */
    @VisibleForTesting
    public static interface CrashUploadDelegate {
        /**
         * Schedule a MinidumpUploadJobService to attempt uploading all ready crash minidumps.
         */
        void scheduleNewJob(@NonNull Context context);

        /**
         * Check if network is unmetered or not.
         */
        boolean isNetworkUnmetered(@NonNull Context context);
    }

    private static CrashUploadDelegate sDelegate = new CrashUploadDelegate() {
        @Override
        public void scheduleNewJob(@NonNull Context context) {
            JobInfo.Builder builder = new JobInfo.Builder(TaskIds.WEBVIEW_MINIDUMP_UPLOADING_JOB_ID,
                    new ComponentName(context, ServiceNames.AW_MINIDUMP_UPLOAD_JOB_SERVICE));
            MinidumpUploadJobService.scheduleUpload(builder);
        }

        @Override
        public boolean isNetworkUnmetered(@NonNull Context context) {
            ConnectivityManager connectivityManager =
                    (ConnectivityManager) context.getApplicationContext().getSystemService(
                            Context.CONNECTIVITY_SERVICE);
            return NetworkPermissionUtil.isNetworkUnmetered(connectivityManager);
        }
    };

    /**
     * Schedule a MinidumpUploadJobService to attempt uploading all ready crash minidumps.
     */
    public static void scheduleNewJob(@NonNull Context context) {
        sDelegate.scheduleNewJob(context);
    }

    /**
     * Attempts to upload a crash report with the given local ID.
     *
     * Note that this method is asynchronous. All that is guaranteed is that upload attempts will be
     * enqueued. It marks the file as requested to be uploaded and then schedule an upload job that
     * attempts uploading all files available to upload (including the given minidump file).
     *
     * @param context the context where the upload job will be scheduled from.
     * @param localId The local ID of the crash report.
     */
    @UiThread
    public static void tryUploadCrashDumpWithLocalId(
            @NonNull Context context, @NonNull String localId) {
        CrashFileManager fileManager =
                new CrashFileManager(SystemWideCrashDirectories.getWebViewCrashDir());
        File minidumpFile = fileManager.getCrashFileWithLocalId(localId);
        if (minidumpFile == null) {
            Log.e(TAG, "Could not find a crash dump with local ID " + localId);
            return;
        }
        File renamedMinidumpFile = CrashFileManager.trySetForcedUpload(minidumpFile);
        if (renamedMinidumpFile == null) {
            Log.e(TAG, "Could not rename the file " + minidumpFile.getName() + " for re-upload");
            return;
        }

        scheduleNewJob(context);
    }

    public static boolean isNetworkUnmetered(@NonNull Context context) {
        return sDelegate.isNetworkUnmetered(context);
    }

    @VisibleForTesting
    public static void setCrashUploadDelegateForTesting(CrashUploadDelegate delegate) {
        sDelegate = delegate;
    }

    // Do not instantiate this class.
    private CrashUploadUtil() {}
}

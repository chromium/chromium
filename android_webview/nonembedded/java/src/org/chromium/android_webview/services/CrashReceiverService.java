// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.app.Service;
import android.app.job.JobInfo;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;

import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.crash.CrashInfo;
import org.chromium.android_webview.common.crash.SystemWideCrashDirectories;
import org.chromium.android_webview.common.services.ICrashReceiverService;
import org.chromium.base.Log;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.minidump_uploader.MinidumpUploadJobService;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

/**
 * Service that is responsible for receiving crash dumps from an application, for upload.
 */
public class CrashReceiverService extends Service {
    private static final String TAG = "CrashReceiverService";
    private static final String WEBVIEW_CRASH_LOG_SUFFIX = "_log.json";

    private final Object mCopyingLock = new Object();
    private boolean mIsCopying;

    private final ICrashReceiverService.Stub mBinder = new ICrashReceiverService.Stub() {
        @Override
        public void transmitCrashes(ParcelFileDescriptor[] fileDescriptors, List crashInfo) {
            int uid = Binder.getCallingUid();
            if (crashInfo != null) {
                assert crashInfo.size() == fileDescriptors.length;
            }
            performMinidumpCopyingSerially(
                    uid, fileDescriptors, crashInfo, true /* scheduleUploads */);
        }
    };

    /**
     * Copies minidumps in a synchronized way, waiting for any already started copying operations to
     * finish before copying the current dumps.
     * @param scheduleUploads whether to ask JobScheduler to schedule an upload-job (avoid this
     * during testing).
     */
    @VisibleForTesting
    public void performMinidumpCopyingSerially(int uid, ParcelFileDescriptor[] fileDescriptors,
            List<Map<String, String>> crashesInfo, boolean scheduleUploads) {
        if (!waitUntilWeCanCopy()) {
            Log.e(TAG, "something went wrong when waiting to copy minidumps, bailing!");
            return;
        }

        try {
            boolean copySucceeded = copyMinidumps(uid, fileDescriptors, crashesInfo);
            if (copySucceeded && scheduleUploads) {
                // Only schedule a new job if there actually are any files to upload.
                scheduleNewJob();
            }
        } finally {
            synchronized (mCopyingLock) {
                mIsCopying = false;
                mCopyingLock.notifyAll();
            }
        }
    }

    /**
     * Wait until we are allowed to copy minidumps.
     * @return whether we are actually allowed to copy the files - if false we should just bail.
     */
    private boolean waitUntilWeCanCopy() {
        synchronized (mCopyingLock) {
            while (mIsCopying) {
                try {
                    mCopyingLock.wait();
                } catch (InterruptedException e) {
                    Log.e(TAG, "Was interrupted when waiting to copy minidumps", e);
                    return false;
                }
            }
            mIsCopying = true;
            return true;
        }
    }

    private void scheduleNewJob() {
        JobInfo.Builder builder = new JobInfo.Builder(TaskIds.WEBVIEW_MINIDUMP_UPLOADING_JOB_ID,
                new ComponentName(this, AwMinidumpUploadJobService.class));
        MinidumpUploadJobService.scheduleUpload(builder);
    }

    /**
     * Copy minidumps from the {@param fileDescriptors} to the directory where WebView stores its
     * minidump files. Also writes a new log file for each mindump, the log file contains a JSON
     * object with info from {@param crashesInfo}. The log file name is: <copied-file-name> +
     * {@code "_log.json"} suffix.
     *
     * @return whether any minidump was copied.
     */
    @VisibleForTesting
    public static boolean copyMinidumps(int uid, ParcelFileDescriptor[] fileDescriptors,
            List<Map<String, String>> crashesInfo) {
        CrashFileManager crashFileManager =
                new CrashFileManager(SystemWideCrashDirectories.getOrCreateWebViewCrashDir());
        boolean copiedAnything = false;
        if (fileDescriptors != null) {
            for (int i = 0; i < fileDescriptors.length; i++) {
                ParcelFileDescriptor fd = fileDescriptors[i];
                if (fd == null) continue;
                try {
                    File copiedFile = crashFileManager.copyMinidumpFromFD(fd.getFileDescriptor(),
                            SystemWideCrashDirectories.getWebViewTmpCrashDir(), uid);
                    if (copiedFile == null) {
                        Log.w(TAG, "failed to copy minidump from " + fd.toString());
                        // TODO(gsennton): add UMA metric to ensure we aren't losing too many
                        // minidumps here.
                    } else {
                        copiedAnything = true;
                        if (crashesInfo != null) {
                            Map<String, String> crashInfo = crashesInfo.get(i);
                            File logFile = new File(
                                    SystemWideCrashDirectories.getOrCreateWebViewCrashLogDir(),
                                    copiedFile.getName() + WEBVIEW_CRASH_LOG_SUFFIX);
                            writeCrashInfoToLogFile(logFile, copiedFile, crashInfo);
                        }
                    }
                } catch (IOException e) {
                    Log.w(TAG, "failed to copy minidump from " + fd.toString() + ": "
                            + e.getMessage());
                } finally {
                    deleteFilesInWebViewTmpDirIfExists();
                }
            }
        }
        return copiedAnything;
    }

    /**
     * Writes info about crash in a separate log file for each crash as a JSON Object.
     */
    @VisibleForTesting
    public static boolean writeCrashInfoToLogFile(
            File logFile, File crashFile, Map<String, String> crashInfoMap) {
        try {
            String localId = CrashFileManager.getCrashLocalIdFromFileName(crashFile.getName());
            if (localId == null) return false;
            CrashInfo crashInfo = new CrashInfo(localId);
            crashInfo.captureTime = crashFile.lastModified();

            if (crashInfoMap == null) return false;
            crashInfo.packageName = crashInfoMap.get("app-package-name");

            if (crashInfoMap.containsKey("variations")) {
                crashInfo.variations = Arrays.asList(crashInfoMap.get("variations").split(","));
            }

            FileWriter writer = new FileWriter(logFile);
            try {
                writer.write(crashInfo.serializeToJson());
            } finally {
                writer.close();
            }
            return true;
        } catch (IOException e) {
            Log.w(TAG, "failed to write JSON log entry for crash", e);
        }
        return false;
    }

    /**
     * Delete all files in the directory where temporary files from this Service are stored.
     */
    @VisibleForTesting
    public static void deleteFilesInWebViewTmpDirIfExists() {
        deleteFilesInDirIfExists(SystemWideCrashDirectories.getWebViewTmpCrashDir());
    }

    private static void deleteFilesInDirIfExists(File directory) {
        if (directory.isDirectory()) {
            File[] files = directory.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (!file.delete()) {
                        Log.w(TAG, "Couldn't delete file " + file.getAbsolutePath());
                    }
                }
            }
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
}

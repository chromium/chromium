// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;

import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.services.ICrashReceiverService;
import org.chromium.android_webview.nonembedded.crash.CrashUploadUtil;
import org.chromium.android_webview.nonembedded.crash.SystemWideCrashDirectories;
import org.chromium.base.Log;
import org.chromium.components.minidump_uploader.CrashFileManager;

import java.io.File;
import java.io.IOException;
import java.util.List;
import java.util.Map;

import javax.annotation.concurrent.GuardedBy;

/** Service that is responsible for receiving crash dumps from an application, for upload. */
public class CrashReceiverService extends Service {
    private static final String TAG = "CrashReceiverService";

    private final Object mCopyingLock = new Object();

    @GuardedBy("mCopyingLock")
    private boolean mIsCopying;

    private final ICrashReceiverService.Stub mBinder =
            new ICrashReceiverService.Stub() {
                @Override
                public void transmitCrashes(
                        ParcelFileDescriptor[] fileDescriptors, List crashInfo) {
                    int uid = Binder.getCallingUid();
                    if (crashInfo != null) {
                        assert crashInfo.size() == fileDescriptors.length;
                    }
                    performMinidumpCopyingSerially(
                            uid, fileDescriptors, crashInfo, /* scheduleUploads= */ true);
                }
            };

    /**
     * Copies minidumps in a synchronized way, waiting for any already started copying operations to
     * finish before copying the current dumps.
     * @param scheduleUploads whether to ask JobScheduler to schedule an upload-job (avoid this
     * during testing).
     */
    @VisibleForTesting
    public void performMinidumpCopyingSerially(
            int uid,
            ParcelFileDescriptor[] fileDescriptors,
            List<Map<String, String>> crashesInfo,
            boolean scheduleUploads) {
        if (!waitUntilWeCanCopy()) {
            Log.e(TAG, "something went wrong when waiting to copy minidumps, bailing!");
            return;
        }

        try {
            boolean copySucceeded = copyMinidumps(uid, fileDescriptors, crashesInfo);
            if (copySucceeded && scheduleUploads) {
                // Only schedule a new job if there actually are any files to upload.
                CrashUploadUtil.scheduleNewJob(this);
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

    /**
     * Copy minidumps from the {@param fileDescriptors} to the directory where WebView stores its
     * minidump files. Also writes a new log file for each mindump, the log file contains a JSON
     * object with info from {@param crashesInfo}. The log file name is: <copied-file-name> +
     * {@code "_log.json"} suffix.
     *
     * @return whether any minidump was copied.
     */
    @VisibleForTesting
    public static boolean copyMinidumps(
            int uid,
            ParcelFileDescriptor[] fileDescriptors,
            List<Map<String, String>> crashesInfo) {
        CrashFileManager crashFileManager =
                new CrashFileManager(SystemWideCrashDirectories.getOrCreateWebViewCrashDir());
        boolean copiedAnything = false;
        for (int i = 0; i < fileDescriptors.length; i++) {
            ParcelFileDescriptor fd = fileDescriptors[i];
            Map<String, String> crashInfo = crashesInfo.get(i);
            if (fd == null) continue;
            try {
                File copiedFile =
                        crashFileManager.copyMinidumpFromFD(
                                fd.getFileDescriptor(),
                                SystemWideCrashDirectories.getWebViewTmpCrashDir(),
                                uid);
                if (copiedFile == null) {
                    Log.w(TAG, "failed to copy minidump from " + fd);
                    // TODO(gsennton): add UMA metric to ensure we aren't losing too many
                    // minidumps here.
                } else {
                    copiedAnything = true;
                    File logFile =
                            SystemWideCrashDirectories.createCrashJsonLogFile(copiedFile.getName());
                    CrashLoggingUtils.writeCrashInfoToLogFile(logFile, copiedFile, crashInfo);
                }
            } catch (IOException e) {
                Log.w(TAG, "failed to copy minidump from " + fd, e);
            } finally {
                deleteFilesInWebViewTmpDirIfExists();
            }
        }
        return copiedAnything;
    }

    /** Delete all files in the directory where temporary files from this Service are stored. */
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

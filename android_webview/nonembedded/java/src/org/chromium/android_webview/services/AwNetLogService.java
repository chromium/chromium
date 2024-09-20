// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.app.Service;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Binder;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;

import org.chromium.android_webview.common.services.INetLogService;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.Arrays;
import java.util.Comparator;

/** Service responsible for preparing a ParcelFileDescriptor for uploading NetLogs. */
public class AwNetLogService extends Service {
    private static final String TAG = "AwNetLogs";
    private static final String JSON_EXTENSION = ".json";
    private static final long MAX_TOTAL_CAPACITY = 1000L * 1024 * 1024; // 1 GB
    private static final File NET_LOG_DIR =
            new File(ContextUtils.getApplicationContext().getFilesDir() + "/aw_net_logs");

    private final INetLogService.Stub mBinder =
            new INetLogService.Stub() {
                @Override
                public ParcelFileDescriptor streamLog(long creationTime, String packageName) {
                    NET_LOG_DIR.mkdir();
                    // TODO(crbug.com/338049232): Add job scheduling.
                    cleanUpNetLogDirectory();
                    ParcelFileDescriptor fileDescriptor = null;
                    if (isCorrectPackage(packageName)) {
                        String entireFileName =
                                Binder.getCallingPid()
                                        + "_"
                                        + creationTime
                                        + "_"
                                        + packageName
                                        + JSON_EXTENSION;
                        File newLogFile = new File(NET_LOG_DIR, entireFileName);
                        try {
                            fileDescriptor =
                                    ParcelFileDescriptor.open(
                                            newLogFile,
                                            ParcelFileDescriptor.MODE_WRITE_ONLY
                                                    | ParcelFileDescriptor.MODE_CREATE
                                                    | ParcelFileDescriptor.MODE_TRUNCATE);
                        } catch (FileNotFoundException e) {
                            Log.e(TAG, "Failed to open log file " + newLogFile);
                        }

                        // The boolean value doesn't matter, we only care about the total count.
                        RecordHistogram.recordBooleanHistogram(
                                "Android.WebView.DevUi.NetLogsCalled", true);
                    }

                    return fileDescriptor;
                }
            };

    @Override
    public IBinder onBind(Intent intent) {
        // TODO(crbug.com/340272811): Check if NET_LOG flag is enabled before returning IBinder.
        return mBinder;
    }

    public static File getNetLogFileDirectory() {
        NET_LOG_DIR.mkdir();
        return NET_LOG_DIR;
    }

    private boolean isCorrectPackage(String packageName) {
        int binderUid = Binder.getCallingUid();
        try {
            int applicationUid =
                    ContextUtils.getApplicationContext()
                            .getPackageManager()
                            .getPackageUid(packageName, PackageManager.MATCH_ALL);
            if (applicationUid == binderUid) {
                return true;
            }
        } catch (PackageManager.NameNotFoundException e) {
            Log.w(TAG, "Unable to resolve package name's UID.", e);
        }
        return false;
    }

    public static void cleanUpNetLogDirectory() {
        // Date thirty days ago
        long expirationDate = System.currentTimeMillis() - (1000L * 60 * 60 * 24 * 30);
        File[] files = getNetLogFileDirectory().listFiles();

        long totalBytes = 0L;
        for (File file : files) {
            long creationTime = getCreationTimeFromFileName(file.getName());
            if (creationTime < expirationDate) {
                boolean deleted = file.delete();
                if (!deleted) {
                    Log.w(TAG, "Failed to delete file: " + file.getAbsolutePath());
                }
            } else {
                totalBytes += file.length();
            }
        }

        if (totalBytes > MAX_TOTAL_CAPACITY) {
            reclaimStorageSpace(files, totalBytes);
        }
    }

    // Delete the oldest files untul total disk usage is less than 1GB
    private static void reclaimStorageSpace(File[] files, long totalBytes) {
        // Sort files by creation time, oldest first
        Arrays.sort(
                files,
                new Comparator<File>() {
                    @Override
                    public int compare(File fileOne, File fileTwo) {
                        long firstFileTime = getCreationTimeFromFileName(fileOne.getName());
                        long secondFileTime = getCreationTimeFromFileName(fileTwo.getName());
                        long diff = firstFileTime - secondFileTime;
                        return (int) diff;
                    }
                });
        int index = 0;
        while (totalBytes > MAX_TOTAL_CAPACITY) {
            long capacity = files[index].length();
            boolean deleted = files[index].delete();
            if (deleted) {
                totalBytes -= capacity;
            } else {
                Log.w(TAG, "Failed to delete file: " + files[index].getAbsolutePath());
            }
            index += 1;
        }
    }

    public static Long getCreationTimeFromFileName(String fileName) {
        String[] file = fileName.split("_", 3);
        return Long.parseLong(file[1]);
    }
}

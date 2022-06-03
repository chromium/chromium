// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.metrics;

import android.annotation.TargetApi;
import android.app.usage.StorageStats;
import android.app.usage.StorageStatsManager;
import android.content.Context;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.os.Environment;
import android.os.Process;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.browser_ui.util.ConversionUtils;

import java.io.IOException;
import java.util.UUID;

/**
 * Records UMA about the size of data, cache, and code size on disk for Android.
 */
public class PackageMetrics {
    private static final String TAG = "PackageMetrics";

    private static class PackageMetricsData {
        public long codeSize;
        public long dataSize;
        public long cacheSize;
    }

    @TargetApi(26)
    private static PackageMetricsData getPackageStatsForAndroidO() {
        Context context = ContextUtils.getApplicationContext();
        StorageManager storageManager = context.getSystemService(StorageManager.class);
        StorageStatsManager storageStatsManager =
                context.getSystemService(StorageStatsManager.class);
        if (storageManager == null || storageStatsManager == null) {
            Log.e(TAG, "StorageManager or StorageStatsManager is not found");
            return null;
        }

        String packageName = context.getPackageName();
        PackageMetricsData pmd = new PackageMetricsData();
        for (StorageVolume storageVolume : storageManager.getStorageVolumes()) {
            if (Environment.MEDIA_MOUNTED.equals(storageVolume.getState())) {
                String uuidStr = storageVolume.getUuid();
                UUID uuid = null;
                try {
                    uuid = uuidStr == null ? StorageManager.UUID_DEFAULT : UUID.fromString(uuidStr);
                } catch (IllegalArgumentException ex) {
                    // For SD card, on some phone the IIUD is not in the right format and we need
                    // to ignore it, otherwise the whole call fails.
                    Log.e(TAG, "Could not parse UUID " + uuidStr, ex);
                }
                if (uuid == null) continue;

                try {
                    StorageStats storageStats = storageStatsManager.queryStatsForPackage(
                            uuid, packageName, Process.myUserHandle());
                    pmd.codeSize += storageStats.getAppBytes();
                    pmd.dataSize += (storageStats.getDataBytes() - storageStats.getCacheBytes());
                    pmd.cacheSize += storageStats.getCacheBytes();
                } catch (IOException | NameNotFoundException | SecurityException ex) {
                    Log.e(TAG, "Error calling into queryStatsForPackage", ex);
                }
            }
        }
        return pmd;
    }

    /**
     * Records UMA about the size of data, cache, and code size on disk for Android.
     * Should be called on background thread since some of the API calls can be slow.
     */
    public static void recordPackageStats() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;

        PackageMetricsData data = getPackageStatsForAndroidO();
        if (data != null) {
            RecordHistogram.recordCustomCountHistogram("Android.PackageStats.DataSize",
                    (int) ConversionUtils.bytesToMegabytes(data.dataSize), 1, 10000, 50);
            RecordHistogram.recordCustomCountHistogram("Android.PackageStats.CacheSize",
                    (int) ConversionUtils.bytesToMegabytes(data.cacheSize), 1, 10000, 50);
            RecordHistogram.recordSparseHistogram("Android.PackageStats.CodeSize",
                    (int) ConversionUtils.bytesToMegabytes(data.codeSize));
        }
    }
}

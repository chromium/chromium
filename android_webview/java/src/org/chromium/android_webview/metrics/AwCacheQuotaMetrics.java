// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.metrics;

import android.content.Context;
import android.os.SystemClock;
import android.os.storage.StorageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.util.UUID;

/** Records app cache quota and cache usage histograms for the embedding application. */
@NullMarked
public final class AwCacheQuotaMetrics {
    private AwCacheQuotaMetrics() {}

    /** Queries {@link StorageManager} and records cache quota and usage histograms. */
    public static void record() {
        StorageManager storageManager =
                (StorageManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.STORAGE_SERVICE);
        UUID storageUuid = ContextUtils.getApplicationContext().getApplicationInfo().storageUuid;
        long startTimeGetCacheQuotaMs = SystemClock.uptimeMillis();
        long cacheQuotaKiloBytes = -1;
        try {
            // This can throw `SecurityException` if the app doesn't have sufficient privileges.
            // See crbug.com/422174715.
            cacheQuotaKiloBytes = storageManager.getCacheQuotaBytes(storageUuid) / 1024;
            RecordHistogram.recordCount1MHistogram(
                    "Android.WebView.CacheQuotaSize", (int) cacheQuotaKiloBytes);
        } catch (Exception e) {
        } finally {
            RecordHistogram.recordTimesHistogram(
                    "Android.WebView.GetCacheQuotaSizeTime",
                    SystemClock.uptimeMillis() - startTimeGetCacheQuotaMs);
        }

        long startTimeGetCacheSizeMs = SystemClock.uptimeMillis();
        long cacheSizeKiloBytes = -1;
        try {
            // This can throw `SecurityException` if the app doesn't have sufficient privileges.
            // See crbug.com/422174715.
            cacheSizeKiloBytes = storageManager.getCacheSizeBytes(storageUuid) / 1024;
            RecordHistogram.recordCount1MHistogram(
                    "Android.WebView.CacheSize", (int) cacheSizeKiloBytes);
        } catch (Exception e) {
        } finally {
            RecordHistogram.recordTimesHistogram(
                    "Android.WebView.GetCacheSizeTime",
                    SystemClock.uptimeMillis() - startTimeGetCacheSizeMs);
        }

        if (cacheQuotaKiloBytes != -1 && cacheSizeKiloBytes != -1) {
            long quotaRemainingKiloBytes = cacheQuotaKiloBytes - cacheSizeKiloBytes;
            if (quotaRemainingKiloBytes >= 0) {
                RecordHistogram.recordCount1MHistogram(
                        "Android.WebView.CacheSizeWithinQuota", (int) quotaRemainingKiloBytes);
            } else {
                RecordHistogram.recordCount1MHistogram(
                        "Android.WebView.CacheSizeExceedsQuota",
                        -1 * (int) quotaRemainingKiloBytes);
            }
        }
    }
}

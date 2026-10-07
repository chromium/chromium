// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;

import androidx.annotation.VisibleForTesting;
import androidx.annotation.WorkerThread;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.cached_flags.CachedFlagsSharedPreferences;

import java.io.File;
import java.util.Map;

/**
 * Records metrics about the app-wide (default) SharedPreferences file.
 *
 * <p>All entries are held in memory for the lifetime of the process and the whole file is rewritten
 * on every apply()/commit(), so these act as proxies for the RAM footprint and the disk write cost
 * of SharedPreferences. CachedFlags and CachedFeatureParams are broken down separately to track
 * their reduction as they are migrated away and their keys cleaned up.
 */
@NullMarked
public class SharedPreferencesMetrics {
    // Bytes taken by an entry in the SharedPreferences XML file beyond its key and value, per
    // value type. Each includes 4 spaces of indentation and a trailing newline.
    // `<boolean name="" value="" />`
    private static final int XML_OVERHEAD_BOOLEAN = 33;
    // `<int name="" value="" />`
    private static final int XML_OVERHEAD_INT = 29;
    // `<long name="" value="" />`
    private static final int XML_OVERHEAD_LONG = 30;
    // `<float name="" value="" />`
    private static final int XML_OVERHEAD_FLOAT = 31;
    // `<string name=""></string>`, also used as the fallback for other types.
    private static final int XML_OVERHEAD_STRING = 30;

    // Exponential buckets of ~10% width, enough resolution to see incremental key cleanups.
    private static final int HISTOGRAM_MIN = 1;
    private static final int HISTOGRAM_MAX = 10000;
    private static final int HISTOGRAM_BUCKETS = 100;

    private SharedPreferencesMetrics() {}

    /** Records the metrics. Reads the file size from disk, so must be called off the UI thread. */
    // Needs to enumerate all keys in the file, which SharedPreferencesManager doesn't allow.
    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
    @WorkerThread
    public static void recordAppSharedPreferencesMetrics() {
        // getAll() returns a copy of the in-memory map; acceptable once per process.
        Map<String, ?> allEntries = ContextUtils.getAppSharedPreferences().getAll();
        int cachedFlagsCount = 0;
        long cachedFlagsBytes = 0;
        for (Map.Entry<String, ?> entry : allEntries.entrySet()) {
            String key = entry.getKey();
            if (CachedFlagsSharedPreferences.FLAGS_CACHED.hasGenerated(key)
                    || CachedFlagsSharedPreferences.FLAGS_FEATURE_PARAM_CACHED.hasGenerated(key)) {
                cachedFlagsCount++;
                cachedFlagsBytes += estimateEntrySizeBytes(key, entry.getValue());
            }
        }
        recordCount("Android.AppSharedPreferences.KeyCount", allEntries.size());
        recordCount("Android.AppSharedPreferences.CachedFlags.KeyCount", cachedFlagsCount);
        recordCount(
                "Android.AppSharedPreferences.CachedFlags.EstimatedSizeKB",
                bytesToKbRoundedUp(cachedFlagsBytes));

        Context context = ContextUtils.getApplicationContext();
        // Matches the file used by getDefaultSharedPreferences().
        // Context#getSharedPreferencesPath()
        // would be cleaner, but is a hidden API.
        File prefsFile =
                new File(
                        new File(context.getDataDir(), "shared_prefs"),
                        context.getPackageName() + "_preferences.xml");
        long sizeBytes = prefsFile.length();
        if (sizeBytes > 0) {
            recordCount("Android.AppSharedPreferences.FileSizeKB", bytesToKbRoundedUp(sizeBytes));
        }
    }

    /**
     * Estimates the size of an entry in the SharedPreferences XML file, e.g. `<boolean
     * name="Chrome.Flags.CachedFlag.Foo" value="true" />`. Assumes ASCII keys and values that need
     * no XML escaping, so that characters equal bytes.
     */
    @VisibleForTesting
    static int estimateEntrySizeBytes(String key, @Nullable Object value) {
        int overhead;
        if (value instanceof Boolean) {
            overhead = XML_OVERHEAD_BOOLEAN;
        } else if (value instanceof Integer) {
            overhead = XML_OVERHEAD_INT;
        } else if (value instanceof Long) {
            overhead = XML_OVERHEAD_LONG;
        } else if (value instanceof Float) {
            overhead = XML_OVERHEAD_FLOAT;
        } else {
            overhead = XML_OVERHEAD_STRING;
        }
        return key.length() + String.valueOf(value).length() + overhead;
    }

    private static long bytesToKbRoundedUp(long bytes) {
        return (bytes + 1023) / 1024;
    }

    private static void recordCount(String name, long sample) {
        RecordHistogram.recordCustomCountHistogram(
                name,
                (int) Math.min(sample, Integer.MAX_VALUE),
                HISTOGRAM_MIN,
                HISTOGRAM_MAX,
                HISTOGRAM_BUCKETS);
    }
}

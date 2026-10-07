// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import static org.junit.Assert.assertEquals;

import android.content.SharedPreferences;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.cached_flags.CachedFlagsSharedPreferences;

/** Unit tests for {@link SharedPreferencesMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SharedPreferencesMetricsTest {
    private static final String KEY_COUNT = "Android.AppSharedPreferences.KeyCount";
    private static final String CACHED_FLAGS_KEY_COUNT =
            "Android.AppSharedPreferences.CachedFlags.KeyCount";
    private static final String CACHED_FLAGS_SIZE =
            "Android.AppSharedPreferences.CachedFlags.EstimatedSizeKB";
    private static final String FILE_SIZE = "Android.AppSharedPreferences.FileSizeKB";

    private SharedPreferences mPrefs;

    @Before
    public void setUp() {
        mPrefs = ContextUtils.getAppSharedPreferences();
        mPrefs.edit().clear().commit();
    }

    @Test
    public void testRecord_countsOnlyCachedFlagsAndParams() {
        SharedPreferences.Editor editor = mPrefs.edit();
        // 16 CachedFlags, 70 bytes each = 1120 bytes:
        // `    <boolean name="Chrome.Flags.CachedFlag.Feature00" value="true" />\n`
        for (int i = 0; i < 16; i++) {
            editor.putBoolean(
                    CachedFlagsSharedPreferences.FLAGS_CACHED.createKey(
                            String.format("Feature%02d", i)),
                    true);
        }
        // 1 CachedFeatureParam, 82 bytes:
        // `    <string name="Chrome.Flags.FeatureParamCached.Feature00:param">value</string>\n`
        editor.putString(
                CachedFlagsSharedPreferences.FLAGS_FEATURE_PARAM_CACHED.createKey(
                        "Feature00:param"),
                "value");
        // Not CachedFlags values, even though one shares the "Chrome.Flags." prefix.
        editor.putInt(CachedFlagsSharedPreferences.FLAGS_CRASH_STREAK_BEFORE_CACHE, 1);
        editor.putString("Chrome.Unrelated.Key", "value");
        editor.commit();

        try (HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(KEY_COUNT, 19)
                        .expectIntRecord(CACHED_FLAGS_KEY_COUNT, 17)
                        // 1202 bytes, rounded up to 2 KB.
                        .expectIntRecord(CACHED_FLAGS_SIZE, 2)
                        .expectAnyRecord(FILE_SIZE)
                        .build()) {
            SharedPreferencesMetrics.recordAppSharedPreferencesMetrics();
        }
    }

    @Test
    public void testRecord_noCachedFlags() {
        mPrefs.edit().putString("Chrome.Unrelated.Key", "value").commit();

        try (HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(KEY_COUNT, 1)
                        .expectIntRecord(CACHED_FLAGS_KEY_COUNT, 0)
                        .expectIntRecord(CACHED_FLAGS_SIZE, 0)
                        .build()) {
            SharedPreferencesMetrics.recordAppSharedPreferencesMetrics();
        }
    }

    @Test
    public void testEstimateEntrySizeBytes() {
        // `    <boolean name="k" value="true" />\n`
        assertEquals(38, SharedPreferencesMetrics.estimateEntrySizeBytes("k", true));
        // `    <int name="k" value="5" />\n`
        assertEquals(31, SharedPreferencesMetrics.estimateEntrySizeBytes("k", 5));
        // `    <long name="k" value="5" />\n`
        assertEquals(32, SharedPreferencesMetrics.estimateEntrySizeBytes("k", 5L));
        // `    <float name="k" value="1.5" />\n`
        assertEquals(35, SharedPreferencesMetrics.estimateEntrySizeBytes("k", 1.5f));
        // `    <string name="k">abc</string>\n`
        assertEquals(34, SharedPreferencesMetrics.estimateEntrySizeBytes("k", "abc"));
    }
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.metrics;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.WorkerThread;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;

import java.text.DateFormat;
import java.util.Collections;
import java.util.Date;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

/** Stores visited origins and logs the count of distinct origins for a day. */
@Lifetime.Singleton
public final class AwOriginVisitLogger {
    private static final String PREFS_FILE = "AwOriginVisitLoggerPrefs";
    private static final String KEY_ORIGINS_VISITED_DATE = "origins_visited_date";
    private static final String KEY_ORIGINS_VISITED_SET = "origins_visited_set";

    private AwOriginVisitLogger() {}

    /**
     * Stores the origin and logs the count of distinct origins if there are any for past visits.
     * This should not be called on the UI thread because it uses SharedPreferences.
     */
    @WorkerThread
    public static void logOriginVisit(long originHash) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            SharedPreferences prefs =
                    ContextUtils.getApplicationContext()
                            .getSharedPreferences(PREFS_FILE, Context.MODE_PRIVATE);

            // We use TimeUtils to make testing easier.
            Date now = new Date(TimeUtils.currentTimeMillis());

            // The date will be something like "1/31/22".
            String todayDate = DateFormat.getDateInstance(DateFormat.SHORT, Locale.US).format(now);
            String storedDate = prefs.getString(KEY_ORIGINS_VISITED_DATE, null);

            // Wrap it in a new HashSet as the one returned by getStringSet must not be modified.
            Set<String> origins =
                    new HashSet<>(
                            prefs.getStringSet(KEY_ORIGINS_VISITED_SET, Collections.emptySet()));

            // If there are stored origin hashes that are not for today, then their count must be
            // logged exactly once and the set cleared before we start storing hashes for today.
            if (!origins.isEmpty() && storedDate != null && !storedDate.equals(todayDate)) {
                RecordHistogram.recordLinearCountHistogram(
                        "Android.WebView.OriginsVisited", origins.size(), 1, 99, 100);
                origins.clear();
            }

            // Store the date and origin to be logged on a later day.
            origins.add(Long.toString(originHash));
            prefs.edit()
                    .putString(KEY_ORIGINS_VISITED_DATE, todayDate)
                    .putStringSet(KEY_ORIGINS_VISITED_SET, origins)
                    .apply();
        }
    }
}

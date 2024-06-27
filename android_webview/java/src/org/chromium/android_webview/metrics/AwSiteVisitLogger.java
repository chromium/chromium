// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.metrics;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.WorkerThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/** Stores visited sites and logs the count of distinct visits over a week. */
@Lifetime.Singleton
@JNINamespace("android_webview")
public final class AwSiteVisitLogger {
    // This uses the same file name as {@link AwOriginVisitLogger} so that
    // only one shared preference XML file needs to be opened on navigation.
    private static final String PREFS_FILE = "AwOriginVisitLoggerPrefs";

    private static final String KEY_VISITED_WEEKLY_TIME = "sites_visited_weekly_time";
    private static final String KEY_VISITED_WEEKLY_SET = "sites_visited_weekly_set";
    private static final String KEY_RELATED_VISITED_WEEKLY_SET = "related_sites_visited_weekly_set";

    private static final long MILLIS_PER_WEEK = (TimeUtils.SECONDS_PER_DAY * 7) * 1000;

    private AwSiteVisitLogger() {}

    /**
     * Stores the sites and logs the count of distinct sites if there are any past visits older than
     * a week. This should not be called on the UI thread because it uses SharedPreferences.
     */
    @CalledByNative
    @WorkerThread
    public static void logVisit(long siteHash, boolean isSiteRelated) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            SharedPreferences prefs =
                    ContextUtils.getApplicationContext()
                            .getSharedPreferences(PREFS_FILE, Context.MODE_PRIVATE);

            // TimeUtils is used to make testing easier.
            long now = TimeUtils.currentTimeMillis();
            long storedTime = prefs.getLong(KEY_VISITED_WEEKLY_TIME, now);
            long expiryTime = storedTime + MILLIS_PER_WEEK;

            Set<String> sitesVisited =
                    new HashSet<>(
                            prefs.getStringSet(KEY_VISITED_WEEKLY_SET, Collections.emptySet()));

            Set<String> relatedSitesVisited =
                    new HashSet<>(
                            prefs.getStringSet(
                                    KEY_RELATED_VISITED_WEEKLY_SET, Collections.emptySet()));

            // If there are any stored site hashes from the previous week, then their count must be
            // logged exactly once and the set cleared before we start storing hashes for this week.
            if (now > expiryTime) {
                if (!sitesVisited.isEmpty()) {
                    RecordHistogram.recordLinearCountHistogram(
                            "Android.WebView.SitesVisitedWeekly", sitesVisited.size(), 1, 99, 100);
                    sitesVisited.clear();
                    storedTime = now;
                }
                if (!relatedSitesVisited.isEmpty()) {
                    RecordHistogram.recordLinearCountHistogram(
                            "Android.WebView.RelatedSitesVisitedWeekly",
                            relatedSitesVisited.size(),
                            1,
                            99,
                            100);
                    relatedSitesVisited.clear();
                    storedTime = now;
                }
            }

            // Store the time and site to be logged after a week has passed.
            sitesVisited.add(Long.toString(siteHash));
            if (isSiteRelated) {
                relatedSitesVisited.add(Long.toString(siteHash));
            }

            prefs.edit()
                    .putLong(KEY_VISITED_WEEKLY_TIME, storedTime)
                    .putStringSet(KEY_VISITED_WEEKLY_SET, sitesVisited)
                    .putStringSet(KEY_RELATED_VISITED_WEEKLY_SET, relatedSitesVisited)
                    .apply();
        }
    }
}

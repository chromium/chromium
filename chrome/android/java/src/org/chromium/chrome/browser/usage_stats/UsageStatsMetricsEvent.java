// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Enumeration of usage stats-related metrics events. */
@IntDef({
    UsageStatsMetricsEvent.OPT_IN,
    UsageStatsMetricsEvent.OPT_OUT,
    UsageStatsMetricsEvent.START_TRACKING_TOKEN,
    UsageStatsMetricsEvent.STOP_TRACKING_TOKEN,
    UsageStatsMetricsEvent.SUSPEND_SITES,
    UsageStatsMetricsEvent.UNSUSPEND_SITES,
    UsageStatsMetricsEvent.QUERY_EVENTS,
    UsageStatsMetricsEvent.CLEAR_ALL_HISTORY,
    UsageStatsMetricsEvent.CLEAR_HISTORY_RANGE,
    UsageStatsMetricsEvent.CLEAR_HISTORY_DOMAIN,
    UsageStatsMetricsEvent.NUM_ENTRIES,
})
@Retention(RetentionPolicy.SOURCE)
public @interface UsageStatsMetricsEvent {
    int OPT_IN = 0;
    int OPT_OUT = 1;
    int START_TRACKING_TOKEN = 2;
    int STOP_TRACKING_TOKEN = 3;
    int SUSPEND_SITES = 4;
    int UNSUSPEND_SITES = 5;
    int QUERY_EVENTS = 6;
    int CLEAR_ALL_HISTORY = 7;
    int CLEAR_HISTORY_RANGE = 8;
    int CLEAR_HISTORY_DOMAIN = 9;
    int NUM_ENTRIES = 10;
}

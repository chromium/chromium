// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import org.chromium.base.metrics.RecordHistogram;

/**
 * Recorder for usage-stats related metrics events.
 */
public class UsageStatsMetricsReporter {
    public static void reportMetricsEvent(@UsageStatsMetricsEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "UsageStats.Events", event, UsageStatsMetricsEvent.NUM_ENTRIES);
    }
}
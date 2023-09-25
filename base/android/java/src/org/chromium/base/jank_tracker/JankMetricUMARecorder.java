// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Sends Android jank metrics to native to be recorded using UMA.
 */
@JNINamespace("base::android")
public class JankMetricUMARecorder {
    public static void recordJankMetricsToUMA(JankMetrics metric, long reportingIntervalStartTime,
            long reportingIntervalDuration, @JankScenario int scenario) {
        if (metric == null) {
            return;
        }
        JankMetricUMARecorderJni.get().recordJankMetrics(metric.durationsNs, metric.isJanky,
                reportingIntervalStartTime, reportingIntervalDuration, scenario);
    }

    @NativeMethods
    public interface Natives {
        void recordJankMetrics(long[] durationsNs, boolean[] jankStatus,
                long reportingIntervalStartTime, long reportingIntervalDuration, int scenario);
    }
}

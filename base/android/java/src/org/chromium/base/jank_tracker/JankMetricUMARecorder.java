// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.jank_tracker.JankMetricMeasurement.JankMetric;

/**
 * Sends Android jank metrics to native to be recorded using UMA.
 */
@JNINamespace("base::android")
public class JankMetricUMARecorder {
    public static void recordJankMetricsToUMA(JankMetric metric) {
        if (metric == null) {
            return;
        }

        JankMetricUMARecorderJni.get().recordJankMetrics(
                metric.getDurations(), metric.getJankBursts(), metric.getSkippedFrames());
    }

    @NativeMethods
    interface Natives {
        void recordJankMetrics(long[] durationsNs, long[] jankBurstsNs, int missedFrames);
    }
}

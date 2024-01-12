// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** Sends Android jank metrics to native to be recorded using UMA. */
@JNINamespace("base::android")
public class JankMetricUMARecorder {
    public static void recordJankMetricsToUMA(
            JankMetrics metric,
            long reportingIntervalStartTime,
            long reportingIntervalDuration,
            @JankScenario.Type int scenario) {
        if (metric == null) {
            return;
        }
        JankMetricUMARecorderJni.get()
                .recordJankMetrics(
                        metric.durationsNs,
                        metric.missedVsyncs,
                        reportingIntervalStartTime,
                        reportingIntervalDuration,
                        scenario);
    }

    @NativeMethods
    public interface Natives {
        void recordJankMetrics(
                long[] durationsNs,
                int[] missedVsyncs,
                long reportingIntervalStartTime,
                long reportingIntervalDuration,
                int scenario);
    }
}

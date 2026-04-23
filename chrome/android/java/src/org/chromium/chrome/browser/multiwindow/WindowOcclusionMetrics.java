// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/** Handles metrics collection for WindowOcclusionTracker. */
@NullMarked
class WindowOcclusionMetrics {

    private static final String METRIC_NAMESPACE = "Android.MultiWindow.Occlusion.";

    private static final long PERIODIC_METRIC_DELAY_MS = TimeUnit.MINUTES.toMillis(5);
    private static int sOcclusionCalculations;
    private static boolean sPeriodicMetricsRunning;

    private static final Runnable PERIODIC_METRICS_TASK =
            new Runnable() {
                @Override
                public void run() {
                    RecordHistogram.recordCount100000Histogram(
                            getMetricName("OcclusionCalculationsPer5Minutes"),
                            sOcclusionCalculations);
                    sOcclusionCalculations = 0;
                    ThreadUtils.postOnUiThreadDelayed(this, PERIODIC_METRIC_DELAY_MS);
                }
            };

    @IntDef({
        CalculateResult.SUCCESS,
        CalculateResult.VIEW_NOT_FOUND,
        CalculateResult.VISIBLE_RECT_EMPTY,
        CalculateResult.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CalculateResult {
        int SUCCESS = 0;
        int VIEW_NOT_FOUND = 1;
        int VISIBLE_RECT_EMPTY = 2;
        int NUM_ENTRIES = 3;
    }

    /* package */ static void recordTrackResult(boolean success) {
        RecordHistogram.recordBooleanHistogram(getMetricName("TrackResult"), success);
    }

    /* package */ static void recordUntrackResult(boolean success) {
        RecordHistogram.recordBooleanHistogram(getMetricName("UntrackResult"), success);
    }

    /* package */ static void recordCalculateResult(@CalculateResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                getMetricName("CalculateResult"), result, CalculateResult.NUM_ENTRIES);
    }

    /* package */ static TimingMetric getCalculateDurationTimer() {
        return TimingMetric.shortThreadTime(getMetricName("CalculateDuration"));
    }

    /* package */ static void onCalculateOcclusion() {
        ThreadUtils.assertOnUiThread();

        sOcclusionCalculations++;
        if (!sPeriodicMetricsRunning) {
            sPeriodicMetricsRunning = true;
            postPeriodicMetricRunner();
        }
    }

    @VisibleForTesting
    static void postPeriodicMetricRunner() {
        ThreadUtils.postOnUiThreadDelayed(PERIODIC_METRICS_TASK, PERIODIC_METRIC_DELAY_MS);
    }

    private static String getMetricName(String metricName) {
        return METRIC_NAMESPACE + metricName;
    }

    static void resetForTesting() {
        sOcclusionCalculations = 0;
        sPeriodicMetricsRunning = false;
        ThreadUtils.getUiThreadHandler().removeCallbacks(PERIODIC_METRICS_TASK);
    }

    // TODO(488905916): Add more metrics.
}

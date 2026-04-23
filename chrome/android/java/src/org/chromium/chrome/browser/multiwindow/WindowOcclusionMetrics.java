// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Handles metrics collection for WindowOcclusionTracker. */
@NullMarked
class WindowOcclusionMetrics {

    private static final String METRIC_NAMESPACE = "Android.MultiWindow.Occlusion.";

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

    private static String getMetricName(String metricName) {
        return METRIC_NAMESPACE + metricName;
    }

    // TODO(488905916): Add more metrics.
}

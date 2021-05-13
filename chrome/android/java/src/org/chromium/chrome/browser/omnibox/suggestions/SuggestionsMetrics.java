// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.os.Debug;

import org.chromium.base.metrics.RecordHistogram;

import java.util.concurrent.TimeUnit;

/**
 * This class collects a variety of different Suggestions related metrics.
 */
class SuggestionsMetrics {
    // The expected range for measure and layout durations.
    // If the combination of (N*Create)+Measure+Layout, where N is a maximum number of suggestions
    // exceeds:
    // - 16'000us (16ms), the suggestion list will take more than a single frame on modern devices,
    // - 33'000us (33ms), the suggestion list will take more than a single frame on all devices.
    // We expect the reported values to be within 2milliseconds range.
    // The upper range here is deliberately smaller than the limits mentioned above; any stage
    // taking more time than the limit chosen here is an indication of a problem.
    private static final long MIN_HISTOGRAM_DURATION_US = 100;
    private static final long MAX_HISTOGRAM_DURATION_US = TimeUnit.MILLISECONDS.toMicros(10);
    private static final int NUM_DURATION_BUCKETS = 100;

    /** Class for reporting timing metrics using try-with-resources block. */
    public static class TimingMetric implements AutoCloseable {
        private final String mMetricName;
        private final long mStartTime;

        /**
         * Creates an instance and starts the timer.
         * @param metricName The name of the metric to be reported on close.
         */
        public TimingMetric(String metricName) {
            mMetricName = metricName;
            mStartTime = Debug.threadCpuTimeNanos();
        }

        @Override
        public void close() {
            final long endTime = Debug.threadCpuTimeNanos();
            final long duration = getDurationInMicroseconds(mStartTime, endTime);
            if (duration < 0) return;
            RecordHistogram.recordCustomTimesHistogram(mMetricName, duration,
                    MIN_HISTOGRAM_DURATION_US, MAX_HISTOGRAM_DURATION_US, NUM_DURATION_BUCKETS);
        }
    }

    /**
     * Measure duration between two timestamps in microseconds.
     *
     * @param startTimeNs Operation start timestamp (nanoseconds).
     * @param endTimeNs Operation end time (nanoseconds).
     * @return Duration in hundreds of microseconds or -1, if timestamps were invalid.
     */
    private static final long getDurationInMicroseconds(long startTimeNs, long endTimeNs) {
        if (startTimeNs == -1 || endTimeNs == -1) return -1;
        return TimeUnit.NANOSECONDS.toMicros(endTimeNs - startTimeNs);
    }

    /**
     * Record how long the Suggestion List needed to layout its content and children.
     */
    static final SuggestionsMetrics.TimingMetric recordSuggestionListLayoutTime() {
        return new TimingMetric("Android.Omnibox.SuggestionList.LayoutTime");
    }

    /**
     * Record how long the Suggestion List needed to measure its content and children.
     */
    static final SuggestionsMetrics.TimingMetric recordSuggestionListMeasureTime() {
        return new TimingMetric("Android.Omnibox.SuggestionList.MeasureTime");
    }

    /**
     * Record the amount of time needed to create a new suggestion view.
     * The type of view is intentionally ignored for this call.
     */
    static final SuggestionsMetrics.TimingMetric recordSuggestionViewCreateTime() {
        return new TimingMetric("Android.Omnibox.SuggestionView.CreateTime");
    }

    /**
     * Record whether suggestion view was successfully reused.
     *
     * @param reused Whether suggestion view was reused.
     */
    static final void recordSuggestionViewReused(boolean reused) {
        RecordHistogram.recordBooleanHistogram("Android.Omnibox.SuggestionView.Reused", reused);
    }

    /**
     * Record whether the interaction with the Omnibox resulted with a navigation (true) or user
     * leaving the omnibox and suggestions list.
     *
     * @param focusResultedInNavigation Whether the user completed interaction with navigation.
     */
    static final void recordOmniboxFocusResultedInNavigation(boolean focusResultedInNavigation) {
        RecordHistogram.recordBooleanHistogram(
                "Omnibox.FocusResultedInNavigation", focusResultedInNavigation);
    }

    /**
     * Record the length of time between when omnibox gets focused and when a omnibox match is open.
     */
    static final void recordFocusToOpenTime(long focusToOpenTimeInMillis) {
        RecordHistogram.recordMediumTimesHistogram(
                "Omnibox.FocusToOpenTimeAnyPopupState3", focusToOpenTimeInMillis);
    }

    /**
     * Record whether the used suggestion originates from Cache or Autocomplete subsystem.
     *
     * @param isFromCache Whether the suggestion selected by the User comes from suggestion cache.
     */
    static final void recordUsedSuggestionFromCache(boolean isFromCache) {
        RecordHistogram.recordBooleanHistogram(
                "Android.Omnibox.UsedSuggestionFromCache", isFromCache);
    }
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import static org.junit.Assert.fail;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;

import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Objects;

/**
 * Watches a number of histograms in tests to assert later that the expected values were recorded.
 *
 * Uses the delta of records between startWatching() and assertExpectedHistogramsRecorded(), so
 * that records logged in previous tests (in batched tests) don't interfere with the counting.
 *
 * Usage:
 *
 * HistogramWatcher histogramWatcher = HistogramWatcher.newBuilder()
 *     .expectIntRecord("Histogram1", 555)
 *     .expectIntRecord("Histogram1", 666)
 *     .expectBooleanRecord("Histogram2", true)
 *     .expectAnyRecord("Histogram3")
 *     .build();
 *
 * // Act
 * [code under test that is expected to record the histograms above]
 *
 * // Assert
 * histogramWatcher.assertExpectedHistogramsRecorded();
 */
public class HistogramWatcher {
    /**
     * Create a new {@link HistogramWatcher.Builder} to instantiate {@link HistogramWatcher}.
     */
    public static HistogramWatcher.Builder newBuilder() {
        return new HistogramWatcher.Builder();
    }

    /**
     * Convenience method to create a new {@link HistogramWatcher} that expects a single boolean
     * record with {@code value} for {@code histogram} and no more records to the same histogram.
     */
    public static HistogramWatcher newSingleRecordWatcher(String histogram, boolean value) {
        return newBuilder().expectBooleanRecord(histogram, value).build();
    }

    /**
     * Convenience method to create a new {@link HistogramWatcher} that expects a single integer
     * record with {@code value} for {@code histogram} and no more records to the same histogram.
     */
    public static HistogramWatcher newSingleRecordWatcher(String histogram, int value) {
        return newBuilder().expectIntRecord(histogram, value).build();
    }

    /**
     * Builder for {@link HistogramWatcher}. Use to list the expectations of records.
     */
    public static class Builder {
        private final Map<HistogramAndValue, Integer> mRecordsExpected = new HashMap<>();
        private final Map<String, Integer> mTotalRecordsExpected = new HashMap<>();

        /**
         * Use {@link HistogramWatcher#newBuilder()} to instantiate.
         */
        private Builder() {}

        /**
         * Build the {@link HistogramWatcher} and snapshot current number of records of the expected
         * histograms to calculate the delta later.
         */
        public HistogramWatcher build() {
            return new HistogramWatcher(mRecordsExpected, mTotalRecordsExpected);
        }

        /**
         * Add an expectation that {@code histogram} will be recorded once with a boolean {@code
         * value}.
         */
        public Builder expectBooleanRecord(String histogram, boolean value) {
            return expectBooleanRecords(histogram, value, 1);
        }

        /**
         * Add an expectation that {@code histogram} will be recorded a number of {@code times} with
         * a boolean {@code value}.
         */
        public Builder expectBooleanRecords(String histogram, boolean value, int times) {
            return expectIntRecords(histogram, value ? 1 : 0, times);
        }

        /**
         * Add an expectation that {@code histogram} will be recorded once with an int {@code
         * value}.
         */
        public Builder expectIntRecord(String histogram, int value) {
            return expectIntRecords(histogram, value, 1);
        }

        /**
         * Add an expectation that {@code histogram} will be recorded a number of {@code times} with
         * an int {@code value}.
         */
        public Builder expectIntRecords(String histogram, int value, int times) {
            HistogramAndValue histogramAndValue = new HistogramAndValue(histogram, value);
            incrementRecordsExpected(histogramAndValue, times);
            incrementTotalRecordsExpected(histogram, times);
            return this;
        }

        /**
         * Add an expectation that {@code histogram} will be recorded once with any value.
         */
        public Builder expectAnyRecord(String histogram) {
            return expectAnyRecords(histogram, 1);
        }

        /**
         * Add an expectation that {@code histogram} will be recorded a number of {@code times} with
         * any values.
         */
        public Builder expectAnyRecords(String histogram, int times) {
            incrementTotalRecordsExpected(histogram, times);
            return this;
        }

        /**
         * Add an expectation that {@code histogram} will not be recorded with any values.
         */
        public Builder expectNoRecords(String histogram) {
            if (mTotalRecordsExpected.getOrDefault(histogram, 0) != 0) {
                throw new IllegalStateException(
                        "Cannot expect no records but also expect records in previous calls.");
            }

            mTotalRecordsExpected.put(histogram, 0);
            return this;
        }

        private void incrementRecordsExpected(HistogramAndValue histogramAndValue, int increase) {
            int previousCountExpected = mRecordsExpected.getOrDefault(histogramAndValue, 0);
            mRecordsExpected.put(histogramAndValue, previousCountExpected + increase);
        }

        private void incrementTotalRecordsExpected(String histogram, int increase) {
            int previousCountExpected = mTotalRecordsExpected.getOrDefault(histogram, 0);
            mTotalRecordsExpected.put(histogram, previousCountExpected + increase);
        }
    }

    private final Map<HistogramAndValue, Integer> mRecordsExpected;
    private final Map<String, Integer> mTotalRecordsExpected;

    private final Map<HistogramAndValue, Integer> mStartingCounts = new HashMap<>();
    private final Map<String, Integer> mStartingTotalCounts = new HashMap<>();

    private HistogramWatcher(Map<HistogramAndValue, Integer> recordsExpected,
            Map<String, Integer> totalRecordsExpected) {
        mRecordsExpected = recordsExpected;
        mTotalRecordsExpected = totalRecordsExpected;

        takeSnapshot();
    }

    private void takeSnapshot() {
        for (HistogramAndValue histogramAndValue : mRecordsExpected.keySet()) {
            int currentCount = histogramAndValue.getHistogramValueCountForTesting();
            mStartingCounts.put(histogramAndValue, currentCount);
        }
        for (String histogram : mTotalRecordsExpected.keySet()) {
            int currentCount = RecordHistogram.getHistogramTotalCountForTesting(histogram);
            mStartingTotalCounts.put(histogram, currentCount);
        }
    }

    /**
     * Assert that the watched histograms were recorded as expected.
     */
    public void assertExpected() {
        for (Entry<HistogramAndValue, Integer> kv : mRecordsExpected.entrySet()) {
            HistogramAndValue histogramAndValue = kv.getKey();
            int expectedDelta = kv.getValue();

            int actualFinalCount = histogramAndValue.getHistogramValueCountForTesting();
            int actualDelta = actualFinalCount - mStartingCounts.get(histogramAndValue);

            if (expectedDelta != actualDelta) {
                fail(String.format(
                        "Expected delta of <%d record(s)> of histogram \"%s\" with value [%d], "
                                + "but saw a delta of <%d record(s)> in that value's bucket.",
                        expectedDelta, histogramAndValue.mHistogram, histogramAndValue.mValue,
                        actualDelta));
            }
        }

        for (Entry<String, Integer> kv : mTotalRecordsExpected.entrySet()) {
            String histogram = kv.getKey();
            int expectedDelta = kv.getValue();

            int actualFinalCount = RecordHistogram.getHistogramTotalCountForTesting(histogram);
            int actualDelta = actualFinalCount - mStartingTotalCounts.get(histogram);

            if (expectedDelta != actualDelta) {
                fail(String.format(
                        "Expected delta of <%d total record(s)> of histogram \"%s\", but saw a "
                                + "delta of <%d total record(s)>.",
                        expectedDelta, histogram, actualDelta));
            }
        }
    }

    private static class HistogramAndValue {
        private final String mHistogram;
        private final int mValue;

        private HistogramAndValue(String histogram, int value) {
            mHistogram = histogram;
            mValue = value;
        }

        @Override
        public int hashCode() {
            return Objects.hash(mHistogram, mValue);
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            return Objects.equals(this, obj);
        }

        private int getHistogramValueCountForTesting() {
            return RecordHistogram.getHistogramValueCountForTesting(mHistogram, mValue);
        }
    }
}

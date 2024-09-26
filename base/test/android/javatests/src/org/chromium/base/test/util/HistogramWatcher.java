// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import static org.junit.Assert.fail;

import androidx.annotation.Nullable;

import com.google.common.collect.Iterators;
import com.google.common.collect.PeekingIterator;

import org.chromium.base.metrics.HistogramBucket;
import org.chromium.base.metrics.RecordHistogram;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Objects;
import java.util.Set;
import java.util.TreeMap;

/**
 * Watches a number of histograms in tests to assert later that the expected values were recorded.
 *
 * Uses the delta of records between build() and assertExpected(), so that records logged in
 * previous tests (in batched tests) don't interfere with the counting as would happen with direct
 * calls to {@link RecordHistogram}.
 *
 * Usage:
 *
 * // Arrange
 * var histogramWatcher = HistogramWatcher.newBuilder()
 *     .expectIntRecord("Histogram1", 555)
 *     .expectIntRecord("Histogram1", 666)
 *     .expectBooleanRecord("Histogram2", true)
 *     .expectAnyRecord("Histogram3")
 *     .build();
 * or:
 * var histogramWatcher = HistogramWatcher.newSingleRecordWatcher("Histogram1", 555);
 *
 * // Act
 * [code under test that is expected to record the histograms above]
 *
 * // Assert
 * histogramWatcher.assertExpected();
 *
 * Alternatively, Java's try-with-resources can be used to wrap the act block to make the assert
 * implicit. This can be especially helpful when a test case needs to create multiple watchers,
 * as the watcher variables are scoped separately and cannot be accidentally swapped.
 *
 * try (HistogramWatcher ignored = HistogramWatcher.newSingleRecordWatcher("Histogram1") {
 *     [code under test that is expected to record the histogram above]
 * }
 */
public class HistogramWatcher implements AutoCloseable {
    /** Create a new {@link HistogramWatcher.Builder} to instantiate {@link HistogramWatcher}. */
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
     * Convenience method to create a new {@link HistogramWatcher} that expects a single record with
     * any value for {@code histogram} and no more records to the same histogram.
     */
    public static HistogramWatcher newSingleRecordWatcher(String histogram) {
        return newBuilder().expectAnyRecord(histogram).build();
    }

    /** Builder for {@link HistogramWatcher}. Use to list the expectations of records. */
    public static class Builder {
        private final Map<HistogramAndValue, Integer> mRecordsExpected = new HashMap<>();
        private final Map<String, Integer> mTotalRecordsExpected = new HashMap<>();
        private final Set<String> mHistogramsAllowedExtraRecords = new HashSet<>();

        /** Use {@link HistogramWatcher#newBuilder()} to instantiate. */
        private Builder() {}

        /**
         * Build the {@link HistogramWatcher} and snapshot current number of records of the expected
         * histograms to calculate the delta later.
         */
        public HistogramWatcher build() {
            return new HistogramWatcher(
                    mRecordsExpected,
                    mTotalRecordsExpected.keySet(),
                    mHistogramsAllowedExtraRecords);
        }

        /**
         * Add an expectation that {@code histogram} will be recorded once with a boolean {@code
         * value}.
         */
        public Builder expectBooleanRecord(String histogram, boolean value) {
            return expectBooleanRecordTimes(histogram, value, 1);
        }

        /**
         * Add an expectation that {@code histogram} will be recorded a number of {@code times} with
         * a boolean {@code value}.
         */
        public Builder expectBooleanRecordTimes(String histogram, boolean value, int times) {
            return expectIntRecordTimes(histogram, value ? 1 : 0, times);
        }

        /**
         * Add an expectation that {@code histogram} will be recorded once with an int {@code
         * value}.
         */
        public Builder expectIntRecord(String histogram, int value) {
            return expectIntRecordTimes(histogram, value, 1);
        }

        /**
         * Add expectations that {@code histogram} will be recorded with each of the int {@code
         * values} provided.
         */
        public Builder expectIntRecords(String histogram, int... values) {
            for (int value : values) {
                expectIntRecord(histogram, value);
            }
            return this;
        }

        /**
         * Add an expectation that {@code histogram} will be recorded a number of {@code times} with
         * an int {@code value}.
         */
        public Builder expectIntRecordTimes(String histogram, int value, int times) {
            if (times < 0) {
                throw new IllegalArgumentException(
                        "Cannot expect records a negative number of times");
            } else if (times == 0) {
                throw new IllegalArgumentException(
                        "Cannot expect records zero times. Use expectNoRecords() if no records are"
                            + " expected for this histogram. If only certain values are expected"
                            + " for this histogram, by default extra records will already raise an"
                            + " assert.");
            }
            HistogramAndValue histogramAndValue = new HistogramAndValue(histogram, value);
            incrementRecordsExpected(histogramAndValue, times);
            incrementTotalRecordsExpected(histogram, times);
            return this;
        }

        /** Add an expectation that {@code histogram} will be recorded once with any value. */
        public Builder expectAnyRecord(String histogram) {
            return expectAnyRecordTimes(histogram, 1);
        }

        /**
         * Add an expectation that {@code histogram} will be recorded a number of {@code times} with
         * any values.
         */
        public Builder expectAnyRecordTimes(String histogram, int times) {
            HistogramAndValue histogramAndValue = new HistogramAndValue(histogram, ANY_VALUE);
            incrementRecordsExpected(histogramAndValue, times);
            incrementTotalRecordsExpected(histogram, times);
            return this;
        }

        /** Add an expectation that {@code histogram} will not be recorded with any values. */
        public Builder expectNoRecords(String histogram) {
            Integer recordsAlreadyExpected = mTotalRecordsExpected.get(histogram);
            if (recordsAlreadyExpected != null && recordsAlreadyExpected != 0) {
                throw new IllegalStateException(
                        "Cannot expect no records but also expect records in previous calls.");
            }

            mTotalRecordsExpected.put(histogram, 0);
            return this;
        }

        /**
         * Make more lenient the assert that records matched the expectations for {@code histogram}
         * by ignoring extra records.
         */
        public Builder allowExtraRecords(String histogram) {
            mHistogramsAllowedExtraRecords.add(histogram);
            return this;
        }

        /**
         * For all histograms with expectations added before, make more lenient the assert that
         * records matched the expectations by ignoring extra records.
         */
        public Builder allowExtraRecordsForHistogramsAbove() {
            for (String histogram : mTotalRecordsExpected.keySet()) {
                allowExtraRecords(histogram);
            }
            return this;
        }

        private void incrementRecordsExpected(HistogramAndValue histogramAndValue, int increase) {
            Integer previousCountExpected = mRecordsExpected.get(histogramAndValue);
            if (previousCountExpected == null) {
                previousCountExpected = 0;
            }
            mRecordsExpected.put(histogramAndValue, previousCountExpected + increase);
        }

        private void incrementTotalRecordsExpected(String histogram, int increase) {
            Integer previousCountExpected = mTotalRecordsExpected.get(histogram);
            if (previousCountExpected == null) {
                previousCountExpected = 0;
            }
            mTotalRecordsExpected.put(histogram, previousCountExpected + increase);
        }
    }

    private static final int ANY_VALUE = -1;

    private final Map<HistogramAndValue, Integer> mRecordsExpected;
    private final Set<String> mHistogramsWatched;
    private final Set<String> mHistogramsAllowedExtraRecords;

    private final Map<String, List<HistogramBucket>> mStartingSamples = new HashMap<>();

    private HistogramWatcher(
            Map<HistogramAndValue, Integer> recordsExpected,
            Set<String> histogramsWatched,
            Set<String> histogramsAllowedExtraRecords) {
        mRecordsExpected = recordsExpected;
        mHistogramsWatched = histogramsWatched;
        mHistogramsAllowedExtraRecords = histogramsAllowedExtraRecords;

        takeSnapshot();
    }

    private void takeSnapshot() {
        for (String histogram : mHistogramsWatched) {
            mStartingSamples.put(
                    histogram, RecordHistogram.getHistogramSamplesForTesting(histogram));
        }
    }

    /**
     * Implements {@link AutoCloseable}. Note while this interface throws an {@link Exception}, we
     * do not have to, and this allows call sites that know they're handling a
     * {@link HistogramWatcher} to not catch or declare an exception either.
     */
    @Override
    public void close() {
        assertExpected();
    }

    /** Assert that the watched histograms were recorded as expected. */
    public void assertExpected() {
        assertExpected(/* customMessage= */ null);
    }

    /**
     * Assert that the watched histograms were recorded as expected, with a custom message if the
     * assertion is not satisfied.
     */
    public void assertExpected(@Nullable String customMessage) {
        for (String histogram : mHistogramsWatched) {
            assertExpected(histogram, customMessage);
        }
    }

    private void assertExpected(String histogram, @Nullable String customMessage) {
        List<HistogramBucket> actualBuckets = computeActualBuckets(histogram);
        TreeMap<Integer, Integer> expectedValuesAndCounts = new TreeMap<>();
        for (Entry<HistogramAndValue, Integer> kv : mRecordsExpected.entrySet()) {
            if (kv.getKey().mHistogram.equals(histogram)) {
                expectedValuesAndCounts.put(kv.getKey().mValue, kv.getValue());
            }
        }

        // Since |expectedValuesAndCounts| is a TreeMap, iterates expected records in ascending
        // order by value.
        Iterator<Entry<Integer, Integer>> expectedValuesAndCountsIt =
                expectedValuesAndCounts.entrySet().iterator();
        Entry<Integer, Integer> expectedValueAndCount =
                expectedValuesAndCountsIt.hasNext() ? expectedValuesAndCountsIt.next() : null;
        if (expectedValueAndCount != null && expectedValueAndCount.getKey() == ANY_VALUE) {
            // Skip the ANY_VALUE records expected - conveniently always the first entry -1 when
            // present to check them differently at the end.
            expectedValueAndCount =
                    expectedValuesAndCountsIt.hasNext() ? expectedValuesAndCountsIt.next() : null;
        }

        // Will match the actual records with the expected and flag |unexpected| when the actual
        // records cannot match the expected, and count how many |actualExtraRecords| are seen to
        // match them with |ANY_VALUE|s at the end.
        boolean unexpected = false;
        int actualExtraRecords = 0;

        for (HistogramBucket actualBucket : actualBuckets) {
            if (expectedValueAndCount == null) {
                // No expected values are left, so all records seen in this bucket are extra.
                actualExtraRecords += actualBucket.mCount;
                continue;
            }

            // Count how many expected records fall inside the bucket.
            int expectedRecordsMatchedToActualBucket = 0;
            do {
                int expectedValue = expectedValueAndCount.getKey();
                int expectedCount = expectedValueAndCount.getValue();
                if (actualBucket.contains(expectedValue)) {
                    expectedRecordsMatchedToActualBucket += expectedCount;
                    expectedValueAndCount =
                            expectedValuesAndCountsIt.hasNext()
                                    ? expectedValuesAndCountsIt.next()
                                    : null;
                } else {
                    break;
                }
            } while (expectedValueAndCount != null);

            if (actualBucket.mCount > expectedRecordsMatchedToActualBucket) {
                // Saw more records than expected for that bucket's range.
                // Consider the difference as extra records.
                actualExtraRecords += actualBucket.mCount - expectedRecordsMatchedToActualBucket;
            } else if (actualBucket.mCount < expectedRecordsMatchedToActualBucket) {
                // Saw fewer records than expected for that bucket's range.
                // Assert since all expected records should be accounted for.
                unexpected = true;
                break;
            }
            // else, actual records match expected, so just move to check the next actual bucket.
        }

        if (expectedValueAndCount != null) {
            // Still had more expected values but not seen in any actual bucket.
            unexpected = true;
        }

        boolean allowAnyNumberOfExtraRecords = mHistogramsAllowedExtraRecords.contains(histogram);
        Integer expectedExtraRecords =
                mRecordsExpected.get(new HistogramAndValue(histogram, ANY_VALUE));
        if (expectedExtraRecords == null) {
            expectedExtraRecords = 0;
        }
        if ((!allowAnyNumberOfExtraRecords && actualExtraRecords > expectedExtraRecords)
                || actualExtraRecords < expectedExtraRecords) {
            // Expected |extraRecordsExpected| records with any value, found |extraActualRecords|.
            unexpected = true;
        }

        if (unexpected) {
            String expectedRecordsString =
                    getExpectedHistogramSamplesAsString(expectedValuesAndCounts);
            String actualRecordsString = bucketsToString(actualBuckets);
            String atLeastString = allowAnyNumberOfExtraRecords ? "At least " : "";
            int expectedTotalDelta = 0;
            for (Integer expectedCount : expectedValuesAndCounts.values()) {
                expectedTotalDelta += expectedCount;
            }
            int actualTotalDelta = 0;
            for (HistogramBucket actualBucket : actualBuckets) {
                actualTotalDelta += actualBucket.mCount;
            }
            String defaultMessage =
                    String.format(
                            "Records for histogram \"%s\" did not match expected.\n"
                                    + "%s%d record(s) expected: [%s]\n"
                                    + "%d record(s) seen: [%s]",
                            histogram,
                            atLeastString,
                            expectedTotalDelta,
                            expectedRecordsString,
                            actualTotalDelta,
                            actualRecordsString);
            failWithDefaultOrCustomMessage(defaultMessage, customMessage);
        }
    }

    private static String getExpectedHistogramSamplesAsString(
            TreeMap<Integer, Integer> expectedValuesAndCounts) {
        List<String> expectedRecordsStrings = new ArrayList<>();
        for (Entry<Integer, Integer> kv : expectedValuesAndCounts.entrySet()) {
            int value = kv.getKey();
            int count = kv.getValue();
            if (value == ANY_VALUE) {
                // Write records matching "Any" at the end.
                continue;
            }
            expectedRecordsStrings.add(bucketToString(value, value + 1, count));
        }

        if (expectedValuesAndCounts.containsKey(ANY_VALUE)) {
            int anyExpectedCount = expectedValuesAndCounts.get(ANY_VALUE);
            expectedRecordsStrings.add(bucketToString(ANY_VALUE, ANY_VALUE + 1, anyExpectedCount));
        }

        return String.join(", ", expectedRecordsStrings);
    }

    private List<HistogramBucket> computeActualBuckets(String histogram) {
        List<HistogramBucket> startingBuckets = mStartingSamples.get(histogram);
        List<HistogramBucket> finalBuckets =
                RecordHistogram.getHistogramSamplesForTesting(histogram);
        List<HistogramBucket> deltaBuckets = new ArrayList<>();

        PeekingIterator<HistogramBucket> startingBucketsIt =
                Iterators.peekingIterator(startingBuckets.iterator());

        for (HistogramBucket finalBucket : finalBuckets) {
            int totalInEquivalentStartingBuckets = 0;
            while (startingBucketsIt.hasNext()
                    && startingBucketsIt.peek().mMax <= finalBucket.mMax) {
                HistogramBucket startBucket = startingBucketsIt.next();
                if (startBucket.mMin >= finalBucket.mMax) {
                    // This should not happen as the only transition in bucket schema is from the
                    // CachingUmaRecord (which is as granular as possible, buckets of [n, n+1) )
                    // to NativeUmaRecorder (which has varying granularity).
                    fail(
                            String.format(
                                    "Histogram bucket bounds before and after the test don't match,"
                                            + " cannot assert histogram counts.\n"
                                            + "Before: [%s]\n"
                                            + "After: [%s]",
                                    bucketsToString(startingBuckets),
                                    bucketsToString(finalBuckets)));
                }
                if (startBucket.mMin >= finalBucket.mMin) {
                    // Since start.max <= final.max, this means the start bucket is contained in the
                    // final bucket.
                    totalInEquivalentStartingBuckets += startBucket.mCount;
                }
            }

            int delta = finalBucket.mCount - totalInEquivalentStartingBuckets;

            if (delta == 0) {
                // Empty buckets don't need to be printed.
                continue;
            } else {
                deltaBuckets.add(new HistogramBucket(finalBucket.mMin, finalBucket.mMax, delta));
            }
        }
        return deltaBuckets;
    }

    private static String bucketsToString(List<HistogramBucket> buckets) {
        List<String> bucketStrings = new ArrayList<>();
        for (HistogramBucket bucket : buckets) {
            bucketStrings.add(bucketToString(bucket));
        }
        return String.join(", ", bucketStrings);
    }

    private static String bucketToString(HistogramBucket bucket) {
        return bucketToString(bucket.mMin, bucket.mMax, bucket.mCount);
    }

    private static String bucketToString(int bucketMin, long bucketMax, int count) {
        String bucketString;
        if (bucketMin == ANY_VALUE) {
            bucketString = "Any";
        } else if (bucketMax == bucketMin + 1) {
            // bucketString is "100" for bucketMin == 100, bucketMax == 101
            bucketString = String.valueOf(bucketMin);
        } else {
            // bucketString is "[100, 120)" for bucketMin == 100, bucketMax == 120
            bucketString = String.format("[%d, %d)", bucketMin, bucketMax);
        }

        if (count == 1) {
            // result is "100" for count == 1
            return bucketString;
        } else {
            // result is "100 (2 times)" for count == 2
            return String.format("%s (%d times)", bucketString, count);
        }
    }

    private static void failWithDefaultOrCustomMessage(
            String defaultMessage, @Nullable String customMessage) {
        if (customMessage != null) {
            fail(String.format("%s\n%s", customMessage, defaultMessage));
        } else {
            fail(defaultMessage);
        }
    }

    /**
     * Polls the instrumentation thread until the expected histograms are recorded.
     *
     * Throws {@link CriteriaNotSatisfiedException} if the polling times out, wrapping the
     * assertion to printed out the state of the histograms at the last check.
     */
    public void pollInstrumentationThreadUntilSatisfied() {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        assertExpected();
                        return true;
                    } catch (AssertionError e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
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
            if (obj instanceof HistogramAndValue) {
                HistogramAndValue that = (HistogramAndValue) obj;
                return this.mHistogram.equals(that.mHistogram) && this.mValue == that.mValue;
            }
            return false;
        }
    }
}

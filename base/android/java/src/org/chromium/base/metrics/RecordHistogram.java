// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import android.text.format.DateUtils;

import java.util.List;

/**
 * Java API for recording UMA histograms.
 * */
public class RecordHistogram {
    /**
     * Records a sample in a boolean UMA histogram of the given name. Boolean histogram has two
     * buckets, corresponding to success (true) and failure (false). This is the Java equivalent of
     * the UMA_HISTOGRAM_BOOLEAN C++ macro.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, either true or false
     */
    public static void recordBooleanHistogram(String name, boolean sample) {
        UmaRecorderHolder.get().recordBooleanHistogram(name, sample);
    }

    /**
     * Records a sample in an enumerated histogram of the given name and boundary. Note that
     * {@code max} identifies the histogram - it should be the same at every invocation. This is the
     * Java equivalent of the UMA_HISTOGRAM_ENUMERATION C++ macro.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 0 and at most {@code max-1}
     * @param max upper bound for legal sample values - all sample values have to be strictly
     *            lower than {@code max}
     */
    public static void recordEnumeratedHistogram(String name, int sample, int max) {
        recordExactLinearHistogram(name, sample, max);
    }

    /**
     * Records a sample in a count histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_COUNTS_1M C++ macro.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 1 and at most 999999
     */
    public static void recordCount1MHistogram(String name, int sample) {
        UmaRecorderHolder.get().recordExponentialHistogram(name, sample, 1, 1_000_000, 50);
    }

    /**
     * Records a sample in a count histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_COUNTS_100 C++ macro.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 1 and at most 99
     */
    public static void recordCount100Histogram(String name, int sample) {
        UmaRecorderHolder.get().recordExponentialHistogram(name, sample, 1, 100, 50);
    }

    /**
     * Records a sample in a count histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_COUNTS_1000 C++ macro.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 1 and at most 999
     */
    public static void recordCount1000Histogram(String name, int sample) {
        UmaRecorderHolder.get().recordExponentialHistogram(name, sample, 1, 1_000, 50);
    }

    /**
     * Records a sample in a count histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_COUNTS_100000 C++ macro.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 1 and at most 99999
     */
    public static void recordCount100000Histogram(String name, int sample) {
        UmaRecorderHolder.get().recordExponentialHistogram(name, sample, 1, 100_000, 50);
    }

    /**
     * Records a sample in a count histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_CUSTOM_COUNTS C++ macro.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, expected to fall in range {@code [min, max)}
     * @param min the smallest expected sample value; at least 1
     * @param max the smallest sample value that will be recorded in overflow bucket
     * @param numBuckets the number of buckets including underflow ({@code [0, min)}) and overflow
     *                   ({@code [max, inf)}) buckets; at most 100
     */
    public static void recordCustomCountHistogram(
            String name, int sample, int min, int max, int numBuckets) {
        UmaRecorderHolder.get().recordExponentialHistogram(name, sample, min, max, numBuckets);
    }

    /**
     * Records a sample in a linear histogram. This is the Java equivalent for using
     * {@code base::LinearHistogram}.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, expected to fall in range {@code [min, max)}
     * @param min the smallest expected sample value; at least 1
     * @param max the smallest sample value that will be recorded in overflow bucket
     * @param numBuckets the number of buckets including underflow ({@code [0, min)}) and overflow
     *                   ({@code [max, inf)}) buckets; at most 100
     */
    public static void recordLinearCountHistogram(
            String name, int sample, int min, int max, int numBuckets) {
        UmaRecorderHolder.get().recordLinearHistogram(name, sample, min, max, numBuckets);
    }

    /**
     * Records a sample in a percentage histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_PERCENTAGE C++ macro.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 0 and at most 100
     */
    public static void recordPercentageHistogram(String name, int sample) {
        recordExactLinearHistogram(name, sample, 101);
    }

    /**
     * Records a sparse histogram. This is the Java equivalent of {@code base::UmaHistogramSparse}.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded: All values of {@code sample} are valid, including
     *               negative values. Keep the number of distinct values across all users
     *               {@code <= 100} ideally, definitely {@code <= 1000}.
     */
    public static void recordSparseHistogram(String name, int sample) {
        UmaRecorderHolder.get().recordSparseHistogram(name, sample);
    }

    /**
     * Records a sample in a histogram of times. Useful for recording short durations. This is the
     * Java equivalent of the UMA_HISTOGRAM_TIMES C++ macro.
     * <p>
     * Note that histogram samples will always be converted to milliseconds when logged.
     *
     * @param name name of the histogram
     * @param durationMs duration to be recorded in milliseconds
     */
    public static void recordTimesHistogram(String name, long durationMs) {
        recordCustomTimesHistogramMilliseconds(
                name, durationMs, 1, DateUtils.SECOND_IN_MILLIS * 10, 50);
    }

    /**
     * Records a sample in a histogram of times. Useful for recording medium durations. This is the
     * Java equivalent of the UMA_HISTOGRAM_MEDIUM_TIMES C++ macro.
     * <p>
     * Note that histogram samples will always be converted to milliseconds when logged.
     *
     * @param name name of the histogram
     * @param durationMs duration to be recorded in milliseconds
     */
    public static void recordMediumTimesHistogram(String name, long durationMs) {
        recordCustomTimesHistogramMilliseconds(
                name, durationMs, 10, DateUtils.MINUTE_IN_MILLIS * 3, 50);
    }

    /**
     * Records a sample in a histogram of times. Useful for recording long durations. This is the
     * Java equivalent of the UMA_HISTOGRAM_LONG_TIMES C++ macro.
     * <p>
     * Note that histogram samples will always be converted to milliseconds when logged.
     *
     * @param name name of the histogram
     * @param durationMs duration to be recorded in milliseconds
     */
    public static void recordLongTimesHistogram(String name, long durationMs) {
        recordCustomTimesHistogramMilliseconds(name, durationMs, 1, DateUtils.HOUR_IN_MILLIS, 50);
    }

    /**
     * Records a sample in a histogram of times. Useful for recording long durations. This is the
     * Java equivalent of the UMA_HISTOGRAM_LONG_TIMES_100 C++ macro.
     * <p>
     * Note that histogram samples will always be converted to milliseconds when logged.
     *
     * @param name name of the histogram
     * @param durationMs duration to be recorded in milliseconds
     */
    public static void recordLongTimesHistogram100(String name, long durationMs) {
        recordCustomTimesHistogramMilliseconds(name, durationMs, 1, DateUtils.HOUR_IN_MILLIS, 100);
    }

    /**
     * Records a sample in a histogram of times with custom buckets. This is the Java equivalent of
     * the UMA_HISTOGRAM_CUSTOM_TIMES C++ macro.
     * <p>
     * Note that histogram samples will always be converted to milliseconds when logged.
     *
     * @param name name of the histogram
     * @param durationMs duration to be recorded in milliseconds; expected to fall in range
     *                   {@code [min, max)}
     * @param min the smallest expected sample value; at least 1
     * @param max the smallest sample value that will be recorded in overflow bucket
     * @param numBuckets the number of buckets including underflow ({@code [0, min)}) and overflow
     *                   ({@code [max, inf)}) buckets; at most 100
     */
    public static void recordCustomTimesHistogram(
            String name, long durationMs, long min, long max, int numBuckets) {
        recordCustomTimesHistogramMilliseconds(name, durationMs, min, max, numBuckets);
    }

    /**
     * Records a sample in a histogram of sizes in KB. This is the Java equivalent of the
     * UMA_HISTOGRAM_MEMORY_KB C++ macro.
     *
     * <p>Good for sizes up to about 500MB.
     *
     * @param name name of the histogram
     * @param sizeInKB Sample to record in KB
     */
    public static void recordMemoryKBHistogram(String name, int sizeInKB) {
        UmaRecorderHolder.get().recordExponentialHistogram(name, sizeInKB, 1000, 500000, 50);
    }

    /**
     * Records a sample in a histogram of sizes in MB. This is the Java equivalent of the
     * UMA_HISTOGRAM_MEMORY_MEDIUM_MB C++ macro.
     * <p>
     * Good for sizes up to about 4000MB.
     *
     * @param name name of the histogram
     * @param sizeInMB Sample to record in MB
     */
    public static void recordMemoryMediumMBHistogram(String name, int sizeInMB) {
        UmaRecorderHolder.get().recordExponentialHistogram(name, sizeInMB, 1, 4000, 100);
    }

    /**
     * Records a sample in a linear histogram where each bucket in range {@code [0, max)} counts
     * exactly a single value.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, expected to fall in range {@code [0, max)}
     * @param max the smallest value counted in the overflow bucket, shouldn't be larger than 100
     */
    public static void recordExactLinearHistogram(String name, int sample, int max) {
        // Range [0, 1) is counted in the underflow bucket. The first "real" bucket starts at 1.
        final int min = 1;
        // One extra is added for the overflow bucket.
        final int numBuckets = max + 1;
        UmaRecorderHolder.get().recordLinearHistogram(name, sample, min, max, numBuckets);
    }

    private static int clampToInt(long value) {
        if (value > Integer.MAX_VALUE) return Integer.MAX_VALUE;
        // Note: Clamping to MIN_VALUE rather than 0, to let base/ histograms code do its own
        // handling of negative values in the future.
        if (value < Integer.MIN_VALUE) return Integer.MIN_VALUE;
        return (int) value;
    }

    private static void recordCustomTimesHistogramMilliseconds(
            String name, long duration, long min, long max, int numBuckets) {
        UmaRecorderHolder.get()
                .recordExponentialHistogram(
                        name, clampToInt(duration), clampToInt(min), clampToInt(max), numBuckets);
    }

    /**
     * Returns the number of samples recorded in the given bucket of the given histogram.
     *
     * @deprecated Raw counts are easy to misuse. Does not reset between batched tests. Use
     * {@link org.chromium.base.test.util.HistogramWatcher} instead.
     *
     * @param name name of the histogram to look up
     * @param sample the bucket containing this sample value will be looked up
     */
    @Deprecated
    public static int getHistogramValueCountForTesting(String name, int sample) {
        return UmaRecorderHolder.get().getHistogramValueCountForTesting(name, sample);
    }

    /**
     * Returns the number of samples recorded for the given histogram.
     *
     * @deprecated Raw counts are easy to misuse. Does not reset between batched tests. Use
     * {@link org.chromium.base.test.util.HistogramWatcher} instead.
     *
     * @param name name of the histogram to look up
     */
    @Deprecated
    public static int getHistogramTotalCountForTesting(String name) {
        return UmaRecorderHolder.get().getHistogramTotalCountForTesting(name);
    }

    /**
     * Returns the buckets of samples recorded for the given histogram.
     *
     * Use {@link org.chromium.base.test.util.HistogramWatcher} instead of using this directly.
     *
     * @param name name of the histogram to look up
     */
    public static List<HistogramBucket> getHistogramSamplesForTesting(String name) {
        return UmaRecorderHolder.get().getHistogramSamplesForTesting(name);
    }
}

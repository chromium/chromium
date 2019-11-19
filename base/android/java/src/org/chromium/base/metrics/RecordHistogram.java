// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import android.text.format.DateUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Java API for recording UMA histograms.
 *
 * Internally, histograms objects are cached on the Java side by their pointer
 * values (converted to long). This is safe to do because C++ Histogram objects
 * are never freed. Caching them on the Java side prevents needing to do costly
 * Java String to C++ string conversions on the C++ side during lookup.
 *
 * Note: the JNI calls are relatively costly - avoid calling these methods in performance-critical
 * code.
 */
@JNINamespace("base::android")
@MainDex
public class RecordHistogram {
    /**
     * Whether recording histograms is currently disabled for testing. Exposed for use in peer
     * classes {e.g. AnimationFrameTimeHistogram}.
     * Use {@link #setDisabledForTests(boolean)} to set this value.
     */
    @VisibleForTesting
    public static Throwable sDisabledBy;
    private static Map<String, Long> sCache =
            Collections.synchronizedMap(new HashMap<String, Long>());

    /**
     * Tests may not have native initialized, so they may need to disable metrics. The value should
     * be reset after the test done, to avoid carrying over state to unrelated tests.
     *
     * In JUnit tests this can be done automatically using
     * {@link org.chromium.chrome.browser.DisableHistogramsRule}
     */
    @VisibleForTesting
    public static void setDisabledForTests(boolean disabled) {
        if (disabled && sDisabledBy != null) {
            throw new IllegalStateException("Histograms are already disabled.", sDisabledBy);
        }
        sDisabledBy = disabled ? new Throwable() : null;
    }

    private static long getCachedHistogramKey(String name) {
        Long key = sCache.get(name);
        // Note: If key is null, we don't have it cached. In that case, pass 0
        // to the native code, which gets converted to a null histogram pointer
        // which will cause the native code to look up the object on the native
        // side.
        return (key == null ? 0 : key);
    }

    /**
     * Records a sample in a boolean UMA histogram of the given name. Boolean histogram has two
     * buckets, corresponding to success (true) and failure (false). This is the Java equivalent of
     * the UMA_HISTOGRAM_BOOLEAN C++ macro.
     * @param name name of the histogram
     * @param sample sample to be recorded, either true or false
     */
    public static void recordBooleanHistogram(String name, boolean sample) {
        if (sDisabledBy != null) return;
        long key = getCachedHistogramKey(name);
        long result = RecordHistogramJni.get().recordBooleanHistogram(name, key, sample);
        if (result != key) sCache.put(name, result);
    }

    /**
     * Records a sample in an enumerated histogram of the given name and boundary. Note that
     * |boundary| identifies the histogram - it should be the same at every invocation. This is the
     * Java equivalent of the UMA_HISTOGRAM_ENUMERATION C++ macro.
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 0 and at most |boundary| - 1
     * @param boundary upper bound for legal sample values - all sample values have to be strictly
     *        lower than |boundary|
     */
    public static void recordEnumeratedHistogram(String name, int sample, int boundary) {
        if (sDisabledBy != null) return;
        long key = getCachedHistogramKey(name);
        long result =
                RecordHistogramJni.get().recordEnumeratedHistogram(name, key, sample, boundary);
        if (result != key) sCache.put(name, result);
    }

    /**
     * Records a sample in a count histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_COUNTS_1M C++ macro.
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 1 and at most 999999
     */
    public static void recordCountHistogram(String name, int sample) {
        recordCustomCountHistogram(name, sample, 1, 1000000, 50);
    }

    /**
     * Records a sample in a count histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_COUNTS_100 C++ macro.
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 1 and at most 99
     */
    public static void recordCount100Histogram(String name, int sample) {
        recordCustomCountHistogram(name, sample, 1, 100, 50);
    }

    /**
     * Records a sample in a count histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_COUNTS_1000 C++ macro.
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 1 and at most 999
     */
    public static void recordCount1000Histogram(String name, int sample) {
        recordCustomCountHistogram(name, sample, 1, 1000, 50);
    }

    /**
     * Records a sample in a count histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_COUNTS_100000 C++ macro.
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 1 and at most 99999
     */
    public static void recordCount100000Histogram(String name, int sample) {
        recordCustomCountHistogram(name, sample, 1, 100000, 50);
    }

    /**
     * Records a sample in a count histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_CUSTOM_COUNTS C++ macro.
     * @param name name of the histogram
     * @param sample sample to be recorded, at least |min| and at most |max| - 1
     * @param min lower bound for expected sample values. It must be >= 1
     * @param max upper bounds for expected sample values
     * @param numBuckets the number of buckets
     */
    public static void recordCustomCountHistogram(
            String name, int sample, int min, int max, int numBuckets) {
        if (sDisabledBy != null) return;
        long key = getCachedHistogramKey(name);
        long result = RecordHistogramJni.get().recordCustomCountHistogram(
                name, key, sample, min, max, numBuckets);
        if (result != key) sCache.put(name, result);
    }

    /**
     * Records a sample in a linear histogram. This is the Java equivalent for using
     * base::LinearHistogram.
     * @param name name of the histogram
     * @param sample sample to be recorded, at least |min| and at most |max| - 1.
     * @param min lower bound for expected sample values, should be at least 1.
     * @param max upper bounds for expected sample values
     * @param numBuckets the number of buckets
     */
    public static void recordLinearCountHistogram(
            String name, int sample, int min, int max, int numBuckets) {
        if (sDisabledBy != null) return;
        long key = getCachedHistogramKey(name);
        long result = RecordHistogramJni.get().recordLinearCountHistogram(
                name, key, sample, min, max, numBuckets);
        if (result != key) sCache.put(name, result);
    }

    /**
     * Records a sample in a percentage histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_PERCENTAGE C++ macro.
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 0 and at most 100.
     */
    public static void recordPercentageHistogram(String name, int sample) {
        if (sDisabledBy != null) return;
        long key = getCachedHistogramKey(name);
        long result = RecordHistogramJni.get().recordEnumeratedHistogram(name, key, sample, 101);
        if (result != key) sCache.put(name, result);
    }

    /**
     * Records a sparse histogram. This is the Java equivalent of UmaHistogramSparse.
     * @param name name of the histogram
     * @param sample sample to be recorded. All values of |sample| are valid, including negative
     *        values.
     */
    public static void recordSparseHistogram(String name, int sample) {
        if (sDisabledBy != null) return;
        long key = getCachedHistogramKey(name);
        long result = RecordHistogramJni.get().recordSparseHistogram(name, key, sample);
        if (result != key) sCache.put(name, result);
    }

    /**
     * Records a sample in a histogram of times. Useful for recording short durations. This is the
     * Java equivalent of the UMA_HISTOGRAM_TIMES C++ macro.
     * Note that histogram samples will always be converted to milliseconds when logged.
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
     * Note that histogram samples will always be converted to milliseconds when logged.
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
     * Note that histogram samples will always be converted to milliseconds when logged.
     * @param name name of the histogram
     * @param durationMs duration to be recorded in milliseconds
     */
    public static void recordLongTimesHistogram(String name, long durationMs) {
        recordCustomTimesHistogramMilliseconds(name, durationMs, 1, DateUtils.HOUR_IN_MILLIS, 50);
    }

    /**
     * Records a sample in a histogram of times. Useful for recording long durations. This is the
     * Java equivalent of the UMA_HISTOGRAM_LONG_TIMES_100 C++ macro.
     * Note that histogram samples will always be converted to milliseconds when logged.
     * @param name name of the histogram
     * @param durationMs duration to be recorded in milliseconds
     */
    public static void recordLongTimesHistogram100(String name, long durationMs) {
        recordCustomTimesHistogramMilliseconds(name, durationMs, 1, DateUtils.HOUR_IN_MILLIS, 100);
    }

    /**
     * Records a sample in a histogram of times with custom buckets. This is the Java equivalent of
     * the UMA_HISTOGRAM_CUSTOM_TIMES C++ macro.
     * Note that histogram samples will always be converted to milliseconds when logged.
     * @param name name of the histogram
     * @param durationMs duration to be recorded in milliseconds
     * @param min the minimum bucket value
     * @param max the maximum bucket value
     * @param numBuckets the number of buckets
     */
    public static void recordCustomTimesHistogram(
            String name, long durationMs, long min, long max, int numBuckets) {
        recordCustomTimesHistogramMilliseconds(name, durationMs, min, max, numBuckets);
    }

    /**
     * Records a sample in a histogram of sizes in KB. This is the Java equivalent of the
     * UMA_HISTOGRAM_MEMORY_KB C++ macro.
     *
     * Good for sizes up to about 500MB.
     *
     * @param name name of the histogram.
     * @param sizeInkB Sample to record in KB.
     */
    public static void recordMemoryKBHistogram(String name, int sizeInKB) {
        recordCustomCountHistogram(name, sizeInKB, 1000, 500000, 50);
    }

    private static int clampToInt(long value) {
        if (value > Integer.MAX_VALUE) return Integer.MAX_VALUE;
        // Note: Clamping to MIN_VALUE rather than 0, to let base/ histograms code
        // do its own handling of negative values in the future.
        if (value < Integer.MIN_VALUE) return Integer.MIN_VALUE;
        return (int) value;
    }

    private static void recordCustomTimesHistogramMilliseconds(
            String name, long duration, long min, long max, int numBuckets) {
        if (sDisabledBy != null) return;
        long key = getCachedHistogramKey(name);
        // Note: Duration, min and max are clamped to int here because that's what's expected by
        // the native histograms API. Callers of these functions still pass longs because that's
        // the types returned by TimeUnit and System.currentTimeMillis() APIs, from which these
        // values come.
        assert max == clampToInt(max);
        long result = RecordHistogramJni.get().recordCustomTimesHistogramMilliseconds(
                name, key, clampToInt(duration), clampToInt(min), clampToInt(max), numBuckets);
        if (result != key) sCache.put(name, result);
    }

    /**
     * Returns the number of samples recorded in the given bucket of the given histogram.
     * @param name name of the histogram to look up
     * @param sample the bucket containing this sample value will be looked up
     */
    @VisibleForTesting
    public static int getHistogramValueCountForTesting(String name, int sample) {
        return RecordHistogramJni.get().getHistogramValueCountForTesting(name, sample);
    }

    /**
     * Returns the number of samples recorded for the given histogram.
     * @param name name of the histogram to look up.
     */
    @VisibleForTesting
    public static int getHistogramTotalCountForTesting(String name) {
        return RecordHistogramJni.get().getHistogramTotalCountForTesting(name);
    }

    /**
     * Natives API to record metrics.
     */
    @NativeMethods
    public interface Natives {
        long recordCustomTimesHistogramMilliseconds(
                String name, long key, int duration, int min, int max, int numBuckets);
        long recordBooleanHistogram(String name, long key, boolean sample);
        long recordEnumeratedHistogram(String name, long key, int sample, int boundary);
        long recordCustomCountHistogram(
                String name, long key, int sample, int min, int max, int numBuckets);
        long recordLinearCountHistogram(
                String name, long key, int sample, int min, int max, int numBuckets);
        long recordSparseHistogram(String name, long key, int sample);
        int getHistogramValueCountForTesting(String name, int sample);
        int getHistogramTotalCountForTesting(String name);
    }
}

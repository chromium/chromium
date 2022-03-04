// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics.test;

import android.util.Pair;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import org.chromium.base.metrics.RecordHistogram;

import java.util.HashMap;
import java.util.Map;

/**
 * Implementation of RecordHistogram which does not rely on native and still enables testing of
 * histogram counts.
 * ShadowRecordHistogram.reset() should be called in the @Before method of test cases using this
 * class.
 */
@Implements(RecordHistogram.class)
public class ShadowRecordHistogram {
    private static final Map<Pair<String, Integer>, Integer> sSamples = new HashMap<>();
    private static final Map<String, Integer> sTotals = new HashMap<>();

    @Resetter
    public static void reset() {
        sSamples.clear();
        sTotals.clear();
    }

    @Implementation
    protected static void recordBooleanHistogram(String name, boolean sample) {
        Pair<String, Integer> key = Pair.create(name, sample ? 1 : 0);
        recordSample(key);
    }

    @Implementation
    protected static void recordEnumeratedHistogram(String name, int sample, int boundary) {
        assert sample < boundary : "Sample " + sample + " is not within boundary " + boundary + "!";
        recordSample(Pair.create(name, sample));
    }

    @Implementation
    protected static void recordExactLinearHistogram(String name, int sample, int max) {
        Pair<String, Integer> key = Pair.create(name, sample);
        recordSample(key);
    }

    @Implementation
    protected static void recordCountHistogram(String name, int sample) {
        Pair<String, Integer> key = Pair.create(name, sample);
        recordSample(key);
    }

    @Implementation
    protected static void recordCount100Histogram(String name, int sample) {
        Pair<String, Integer> key = Pair.create(name, sample);
        recordSample(key);
    }

    @Implementation
    protected static void recordCount1000Histogram(String name, int sample) {
        Pair<String, Integer> key = Pair.create(name, sample);
        recordSample(key);
    }

    @Implementation
    protected static void recordCount100000Histogram(String name, int sample) {
        Pair<String, Integer> key = Pair.create(name, sample);
        recordSample(key);
    }

    @Implementation
    protected static void recordCustomCountHistogram(
            String name, int sample, int min, int max, int numBuckets) {
        Pair<String, Integer> key = Pair.create(name, sample);
        recordSample(key);
    }

    @Implementation
    protected static void recordLinearCountHistogram(
            String name, int sample, int min, int max, int numBuckets) {
        Pair<String, Integer> key = Pair.create(name, sample);
        recordSample(key);
    }

    @Implementation
    protected static void recordPercentageHistogram(String name, int sample) {
        Pair<String, Integer> key = Pair.create(name, sample);
        recordSample(key);
    }

    @Implementation
    protected static void recordSparseHistogram(String name, int sample) {
        Pair<String, Integer> key = Pair.create(name, sample);
        recordSample(key);
    }

    @Implementation
    protected static void recordTimesHistogram(String name, long durationMs) {
        Pair<String, Integer> key = Pair.create(name, (int) durationMs);
        recordSample(key);
    }

    @Implementation
    protected static void recordMediumTimesHistogram(String name, long durationMs) {
        Pair<String, Integer> key = Pair.create(name, (int) durationMs);
        recordSample(key);
    }

    @Implementation
    protected static void recordLongTimesHistogram(String name, long durationMs) {
        Pair<String, Integer> key = Pair.create(name, (int) durationMs);
        recordSample(key);
    }

    @Implementation
    protected static void recordLongTimesHistogram100(String name, long durationMs) {
        Pair<String, Integer> key = Pair.create(name, (int) durationMs);
        recordSample(key);
    }

    @Implementation
    protected static void recordCustomTimesHistogram(
            String name, long durationMs, long min, long max, int numBuckets) {
        Pair<String, Integer> key = Pair.create(name, (int) durationMs);
        recordSample(key);
    }

    @Implementation
    protected static void recordMemoryKBHistogram(String name, int sizeInKB) {
        Pair<String, Integer> key = Pair.create(name, sizeInKB);
        recordSample(key);
    }

    /**
     * Returns #getHistogramValueCountForTesting value in junit tests.
     * While {@link RecordHistogram#getHistogramValueCountForTesting} is deprecated, junit tests
     * should use this function to get the test counts without mocking JNI.
     */
    @Implementation
    public static int getHistogramValueCountForTesting(String name, int sample) {
        Integer i = sSamples.get(Pair.create(name, sample));
        return (i != null) ? i : 0;
    }

    /**
     * Returns #getHistogramTotalCountForTesting value in junit tests.
     * While {@link RecordHistogram#getHistogramTotalCountForTesting} is deprecated, junit tests
     * should use this function to get the correct tests counts without mocking JNI.
     */
    @Implementation
    public static int getHistogramTotalCountForTesting(String name) {
        Integer i = sTotals.get(name);
        return (i != null) ? i : 0;
    }

    private static void recordSample(Pair<String, Integer> key) {
        Integer bucketValue = sSamples.get(key);
        if (bucketValue == null) {
            bucketValue = 0;
        }
        sSamples.put(key, bucketValue + 1);

        Integer totalCount = sTotals.get(key.first);
        if (totalCount == null) {
            totalCount = 0;
        }
        sTotals.put(key.first, totalCount + 1);
    }
}

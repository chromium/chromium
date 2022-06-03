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
import java.util.Set;

/**
 * Implementation of RecordHistogram which does not rely on native and still enables testing of
 * histogram counts.
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

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static void recordBooleanHistogram(String name, boolean sample) {
        Pair<String, Integer> key = Pair.create(name, sample ? 1 : 0);
        recordSample(key);
    }

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static void recordEnumeratedHistogram(String name, int sample, int boundary) {
        assert sample < boundary : "Sample " + sample + " is not within boundary " + boundary + "!";
        recordSample(Pair.create(name, sample));
    }

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static void recordCountHistogram(String name, int sample) {
        Pair<String, Integer> key = Pair.create(name, sample);
        recordSample(key);
    }

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static void recordCount100Histogram(String name, int sample) {
        Pair<String, Integer> key = Pair.create(name, sample);
        recordSample(key);
    }

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static void recordCount1000Histogram(String name, int sample) {
        Pair<String, Integer> key = Pair.create(name, sample);
        recordSample(key);
    }

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static void recordCount100000Histogram(String name, int sample) {
        Pair<String, Integer> key = Pair.create(name, sample);
        recordSample(key);
    }

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static void recordCustomCountHistogram(
            String name, int sample, int min, int max, int numBuckets) {
        Pair<String, Integer> key = Pair.create(name, sample);
        recordSample(key);
    }

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static void recordLinearCountHistogram(
            String name, int sample, int min, int max, int numBuckets) {
        Pair<String, Integer> key = Pair.create(name, sample);
        recordSample(key);
    }

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static void recordPercentageHistogram(String name, int sample) {
        Pair<String, Integer> key = Pair.create(name, sample);
        recordSample(key);
    }

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static void recordSparseHistogram(String name, int sample) {
        Pair<String, Integer> key = Pair.create(name, sample);
        recordSample(key);
    }

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static void recordTimesHistogram(String name, long durationMs) {
        Pair<String, Integer> key = Pair.create(name, (int) durationMs);
        recordSample(key);
    }

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static void recordMediumTimesHistogram(String name, long durationMs) {
        Pair<String, Integer> key = Pair.create(name, (int) durationMs);
        recordSample(key);
    }

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static void recordLongTimesHistogram(String name, long durationMs) {
        Pair<String, Integer> key = Pair.create(name, (int) durationMs);
        recordSample(key);
    }

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static void recordLongTimesHistogram100(String name, long durationMs) {
        Pair<String, Integer> key = Pair.create(name, (int) durationMs);
        recordSample(key);
    }

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static void recordCustomTimesHistogram(
            String name, long durationMs, long min, long max, int numBuckets) {
        Pair<String, Integer> key = Pair.create(name, (int) durationMs);
        recordSample(key);
    }

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static void recordMemoryKBHistogram(String name, int sizeInKB) {
        Pair<String, Integer> key = Pair.create(name, sizeInKB);
        recordSample(key);
    }

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static int getHistogramValueCountForTesting(String name, int sample) {
        Integer i = sSamples.get(Pair.create(name, sample));
        return (i != null) ? i : 0;
    }

    /** @deprecated This method should be protected. Use RecordHistogram directly instead */
    @Implementation
    public static int getHistogramTotalCountForTesting(String name) {
        Integer i = sTotals.get(name);
        return (i != null) ? i : 0;
    }

    @Implementation
    protected static void forgetHistogramForTesting(String name) {
        Set<Pair<String, Integer>> keySet = sSamples.keySet();
        for (Pair<String, Integer> key : keySet) {
            if (name.equals(key.first)) sSamples.put(key, 0);
        }
        sTotals.put(name, 0);
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

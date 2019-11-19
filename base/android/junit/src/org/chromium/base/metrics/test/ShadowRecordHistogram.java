// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics.test;

import android.util.Pair;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import org.chromium.base.metrics.CachedMetrics;
import org.chromium.base.metrics.RecordHistogram;

import java.util.HashMap;

/**
 * Implementation of RecordHistogram which does not rely on native and still enables testing of
 * histogram counts.
 */
@Implements(RecordHistogram.class)
public class ShadowRecordHistogram {
    private static HashMap<Pair<String, Integer>, Integer> sSamples =
            new HashMap<Pair<String, Integer>, Integer>();

    @Resetter
    public static void reset() {
        sSamples.clear();
    }

    @Implementation
    public static void recordBooleanHistogram(String name, boolean sample) {
        Pair<String, Integer> key = Pair.create(name, sample ? 1 : 0);
        incrementSampleCount(key);
    }

    @Implementation
    public static void recordCountHistogram(String name, int sample) {
        Pair<String, Integer> key = Pair.create(name, sample);
        incrementSampleCount(key);
    }

    @Implementation
    public static void recordCount100Histogram(String name, int sample) {
        Pair<String, Integer> key = Pair.create(name, sample);
        incrementSampleCount(key);
    }

    @Implementation
    public static void recordCustomCountHistogram(
            String name, int sample, int min, int max, int numBuckets) {
        Pair<String, Integer> key = Pair.create(name, sample);
        incrementSampleCount(key);
    }

    @Implementation
    public static void recordEnumeratedHistogram(String name, int sample, int boundary) {
        assert sample < boundary : "Sample " + sample + " is not within boundary " + boundary + "!";
        incrementSampleCount(Pair.create(name, sample));
    }

    @Implementation
    public static void recordLongTimesHistogram100(String name, long durationMs) {
        Pair<String, Integer> key = Pair.create(name, (int) durationMs);
        incrementSampleCount(key);
    }

    @Implementation
    public static int getHistogramValueCountForTesting(String name, int sample) {
        CachedMetrics.commitCachedMetrics();
        Integer i = sSamples.get(Pair.create(name, sample));
        return (i != null) ? i : 0;
    }

    private static void incrementSampleCount(Pair<String, Integer> key) {
        Integer i = sSamples.get(key);
        if (i == null) {
            i = 0;
        }
        sSamples.put(key, i + 1);
    }
}

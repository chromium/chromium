// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

/** Common interface for code recording UMA metrics. */
public interface UmaRecorder {
    /** Records a single sample of a boolean histogram. */
    void recordBooleanHistogram(String name, boolean sample);

    /**
     * Records a single sample of a histogram with exponentially scaled buckets. See
     * {@link
     * https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md#count-histograms}
     * <p>
     * This is the default histogram type used by "counts", "times" and "memory" histograms in
     * {@code base/metrics/histogram_functions.h}
     *
     * @param min the smallest value recorded in the first bucket; should be greater than zero.
     * @param max the smallest value recorded in the overflow bucket.
     * @param numBuckets number of histogram buckets: Two buckets are used for underflow and
     *         overflow, and the remaining buckets cover the range {@code [min, max)}; {@code
     *         numBuckets} should be {@code 100} or less.
     */
    void recordExponentialHistogram(String name, int sample, int min, int max, int numBuckets);

    /**
     * Records a single sample of a histogram with evenly spaced buckets. See
     * {@link
     * https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md#percentage-or-ratio-histograms}
     * <p>
     * This histogram type is best suited for recording enums, percentages and ratios.
     *
     * @param min the smallest value recorded in the first bucket; should be equal to one, but will
     *         work with values greater than zero.
     * @param max the smallest value recorded in the overflow bucket.
     * @param numBuckets number of histogram buckets: Two buckets are used for underflow and
     *         overflow, and the remaining buckets evenly cover the range {@code [min, max)}; {@code
     *         numBuckets} should be {@code 100} or less.
     */
    void recordLinearHistogram(String name, int sample, int min, int max, int numBuckets);

    /**
     * Records a single sample of a sparse histogram. See
     * {@link
     * https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md#when-to-use-sparse-histograms}
     */
    void recordSparseHistogram(String name, int sample);

    /**
     * Records a user action. Action names must be documented in {@code actions.xml}. See {@link
     * https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/actions/README.md}
     *
     * @param name Name of the user action.
     * @param elapsedRealtimeMillis Value of {@link android.os.SystemClock.elapsedRealtime()} when
     *         the action was observed.
     */
    void recordUserAction(String name, long elapsedRealtimeMillis);
}

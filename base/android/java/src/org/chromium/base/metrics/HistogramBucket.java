// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

/** Represents one single bucket of a histogram, with the count of records in that bucket. */
public class HistogramBucket {
    public final int mMin;
    public final long mMax;
    public final int mCount;

    public HistogramBucket(int min, long max, int count) {
        mMin = min;
        mMax = max;
        mCount = count;
    }

    public boolean contains(int value) {
        return value >= mMin && value < mMax;
    }
}

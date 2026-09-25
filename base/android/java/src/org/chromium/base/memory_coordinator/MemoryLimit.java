// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.memory_coordinator;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Represents a memory usage limit for a {@link MemoryConsumer}. */
@NullMarked
public final class MemoryLimit {
    public static final MemoryLimit DEFAULT = new MemoryLimit(100);
    public static final MemoryLimit NO_PRESSURE = DEFAULT;
    public static final MemoryLimit MODERATE_PRESSURE = new MemoryLimit(50);
    public static final MemoryLimit CRITICAL_PRESSURE = new MemoryLimit(0);

    private final int mPercent;

    public MemoryLimit(int percent) {
        assert percent >= 0 : "Memory limit cannot be negative";
        mPercent = percent;
    }

    /** Returns the memory limit as an integer percentage (e.g. 100 for 100%). */
    public int getPercent() {
        return mPercent;
    }

    /** Returns the memory limit as a ratio (e.g. 1.0 for 100%, 0.5 for 50%). */
    public double getRatio() {
        return mPercent / 100.0;
    }

    /** Scales the given integer baseline value by this memory limit ratio. */
    public int scale(int baseline) {
        assert baseline >= 0 : "Baseline cannot be negative";
        long scaled = ((long) baseline * mPercent) / 100L;
        return (int) Math.min((long) Integer.MAX_VALUE, scaled);
    }

    /** Scales the given long baseline value by this memory limit ratio. */
    public long scale(long baseline) {
        assert baseline >= 0 : "Baseline cannot be negative";
        if (mPercent == 0 || baseline == 0) return 0;
        double scaled = baseline * getRatio();
        if (scaled >= (double) Long.MAX_VALUE) {
            return Long.MAX_VALUE;
        }
        return (long) scaled;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof MemoryLimit)) return false;
        return mPercent == ((MemoryLimit) obj).mPercent;
    }

    @Override
    public int hashCode() {
        return Integer.hashCode(mPercent);
    }

    @Override
    public String toString() {
        return "MemoryLimit(" + mPercent + "%)";
    }
}

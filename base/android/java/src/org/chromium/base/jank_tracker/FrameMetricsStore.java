// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import java.util.ArrayList;

/**
 * This class stores relevant metrics from FrameMetrics between the calls to UMA reporting methods.
 */
public class FrameMetricsStore {
    // Array of total durations stored in nanoseconds, they represent how long each frame took to
    // draw.
    private final ArrayList<Long> mTotalDurationsNs = new ArrayList<>();

    // Array of boolean values denoting whether a given frame is janky or not. Must always be the
    // same size as mTotalDurationsNs.
    private final ArrayList<Boolean> mIsJanky = new ArrayList<>();

    /**
     * Records the total draw duration and jankiness for a single frame.
     */
    void addFrameMeasurement(long totalDurationNs, boolean isJanky) {
        mTotalDurationsNs.add(totalDurationNs);
        mIsJanky.add(isJanky);
    }

    /**
     * Returns a copy of accumulated metrics and clears the internal storage.
     */
    JankMetrics takeMetrics() {
        Long[] longDurations;
        Boolean[] booleanIsJanky;

        longDurations = mTotalDurationsNs.toArray(new Long[mTotalDurationsNs.size()]);
        mTotalDurationsNs.clear();

        booleanIsJanky = mIsJanky.toArray(new Boolean[mIsJanky.size()]);
        mIsJanky.clear();

        long[] durations = new long[longDurations.length];
        for (int i = 0; i < longDurations.length; i++) {
            durations[i] = longDurations[i].longValue();
        }

        boolean[] isJanky = new boolean[booleanIsJanky.length];
        for (int i = 0; i < booleanIsJanky.length; i++) {
            isJanky[i] = booleanIsJanky[i].booleanValue();
        }

        JankMetrics jankMetrics = new JankMetrics(durations, isJanky);
        return jankMetrics;
    }
}

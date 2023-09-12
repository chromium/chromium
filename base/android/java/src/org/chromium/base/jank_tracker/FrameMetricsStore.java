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
    private final ArrayList<Boolean> mIsScrolling = new ArrayList<>();
    // Tracks whether we are in a web contents scroll or not.
    private long mScrollStartTime = Long.MAX_VALUE;
    private long mScrollEndTime;

    /**
     * Records the total draw duration and jankiness for a single frame.
     */
    void addFrameMeasurement(long totalDurationNs, boolean isJanky, long frameStartVsyncTs) {
        mTotalDurationsNs.add(totalDurationNs);
        mIsJanky.add(isJanky);
        boolean isScrollFrame =
                mScrollStartTime < frameStartVsyncTs && frameStartVsyncTs < mScrollEndTime;
        mIsScrolling.add(isScrollFrame);
    }

    /**
     * Returns a copy of accumulated metrics and clears the internal storage.
     */
    JankMetrics takeMetrics() {
        Long[] longDurations;
        Boolean[] booleanIsJanky;
        Boolean[] booleanIsScrolling;

        longDurations = mTotalDurationsNs.toArray(new Long[mTotalDurationsNs.size()]);
        mTotalDurationsNs.clear();

        booleanIsJanky = mIsJanky.toArray(new Boolean[mIsJanky.size()]);
        mIsJanky.clear();
        booleanIsScrolling = mIsScrolling.toArray(new Boolean[mIsScrolling.size()]);
        mIsScrolling.clear();

        long[] durations = new long[longDurations.length];
        for (int i = 0; i < longDurations.length; i++) {
            durations[i] = longDurations[i].longValue();
        }

        boolean[] isJanky = new boolean[booleanIsJanky.length];
        for (int i = 0; i < booleanIsJanky.length; i++) {
            isJanky[i] = booleanIsJanky[i].booleanValue();
        }

        boolean[] isScrolling = new boolean[booleanIsScrolling.length];
        for (int i = 0; i < booleanIsScrolling.length; i++) {
            isScrolling[i] = booleanIsScrolling[i].booleanValue();
        }

        JankMetrics jankMetrics = new JankMetrics(durations, isJanky, isScrolling);
        return jankMetrics;
    }

    public void onWebContentsScrollStateUpdate(boolean isScrolling) {
        if (isScrolling) {
            mScrollStartTime = System.nanoTime();
            mScrollEndTime = Long.MAX_VALUE;
        } else {
            mScrollEndTime = System.nanoTime();
        }
    }
}

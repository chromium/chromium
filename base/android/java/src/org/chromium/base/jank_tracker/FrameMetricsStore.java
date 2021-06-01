// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.BuildConfig;

import java.util.ArrayList;
import java.util.HashMap;

import javax.annotation.concurrent.GuardedBy;

/**
 * This class stores timestamps and frame durations related to different jank scenarios (e.g.
 * Opening omnibox, Opening tab switcher). Scenarios are tracked by calling {@Link
 * #startTrackingScenario} and {@Link #stopTrackingScenario}. {@Link #stopTrackingScenario} returns
 * a FrameMetrics object with the timestamp and duration of all frames drawn between the start and
 * end of the specified scenario.
 *
 * Multiple scenarios can be tracked simultaneously without duplicating data, and frame data is
 * cleared as needed when scenarios end.
 */
class FrameMetricsStore {
    // Guards access to the fields below.
    private final Object mLock = new Object();

    // Array of timestamps stored in nanoseconds, they represent the moment when each frame
    // finished drawing, must always be the same size as mTotalDurationsNs.
    @GuardedBy("mLock")
    private final ArrayList<Long> mTimestampsNs = new ArrayList<>();

    // Array of total durations stored in nanoseconds, they represent how long each frame took to
    // draw, must always be the same size as mTimestampsNs.
    @GuardedBy("mLock")
    private final ArrayList<Long> mTotalDurationsNs = new ArrayList<>();

    // Number of frames that FrameMetrics was unable to report on, due to excessive activity on its
    // handler thread.
    @GuardedBy("mLock")
    private final ArrayList<Integer> mSkippedFrames = new ArrayList<>();

    // Map of jank scenarios being currently tracked and the timestamp of the frame before tracking
    // began. Each key corresponds to a JankScenario and each value corresponds to a timestamp
    // present in mTimestampsNs, or 0 in case the scenario tracking started before any frames were
    // recorded. If empty then no scenarios are being tracked, so calls to addFrameMeasurement won't
    // store anything.
    @GuardedBy("mLock")
    private final HashMap<Integer, Long> mScenarioPreviousFrameTimestampNs = new HashMap<>();

    /**
     * Records a timestamp and total draw duration for a single frame.
     */
    void addFrameMeasurement(long timestampNs, long totalDurationNs, int skippedFrames) {
        synchronized (mLock) {
            if (mScenarioPreviousFrameTimestampNs.isEmpty()) {
                return;
            }

            mTimestampsNs.add(timestampNs);
            mTotalDurationsNs.add(totalDurationNs);
            mSkippedFrames.add(skippedFrames);
        }
    }

    void startTrackingScenario(@JankScenario int scenario) {
        synchronized (mLock) {
            // Ignore multiple calls to startTrackingScenario without corresponding
            // stopTrackingScenario calls.
            if (mScenarioPreviousFrameTimestampNs.containsKey(scenario)) {
                return;
            }

            // Scenarios are tracked based on the latest stored timestamp to allow fast lookups
            // (find index of [timestamp] vs find first index that's >= [timestamp]). In case there
            // are no stored timestamps then we hardcode the scenario's starting timestamp to 0L,
            // this is handled as a special case in stopTrackingScenario by returning all stored
            // frames.
            Long startingTimestamp = 0L;
            if (!mTimestampsNs.isEmpty()) {
                startingTimestamp = mTimestampsNs.get(mTimestampsNs.size() - 1);
            }

            mScenarioPreviousFrameTimestampNs.put(scenario, startingTimestamp);
        }
    }

    FrameMetrics stopTrackingScenario(@JankScenario int scenario) {
        synchronized (mLock) {
            // Get the timestamp of the latest frame before startTrackingScenario was called. This
            // can be null if tracking never started for scenario, or 0L if tracking started when no
            // frames were stored.
            Long previousFrameTimestamp = mScenarioPreviousFrameTimestampNs.remove(scenario);

            // If stopTrackingScenario is called without a corresponding startTrackingScenario then
            // return an empty FrameMetrics object.
            if (previousFrameTimestamp == null) {
                return new FrameMetrics();
            }

            int startingIndex;
            // Starting timestamp may be 0 if a scenario starts without any frames stored, in this
            // case return all frames.
            if (previousFrameTimestamp == 0) {
                startingIndex = 0;
            } else {
                startingIndex = mTimestampsNs.indexOf(previousFrameTimestamp);
                // The scenario starts with the frame after the tracking timestamp.
                startingIndex++;

                // If startingIndex is out of bounds then we haven't recorded any frames since
                // tracking started, return an empty FrameMetrics object.
                if (startingIndex >= mTimestampsNs.size()) {
                    return new FrameMetrics();
                }
            }

            // Ending index is exclusive, so this is not out of bounds.
            int endingIndex = mTimestampsNs.size();
            int scenarioFrameCount = endingIndex - startingIndex;

            Long[] timestamps = mTimestampsNs.subList(startingIndex, endingIndex)
                                        .toArray(new Long[scenarioFrameCount]);
            Long[] durations = mTotalDurationsNs.subList(startingIndex, endingIndex)
                                       .toArray(new Long[scenarioFrameCount]);
            Integer[] skippedFrames = mSkippedFrames.subList(startingIndex, endingIndex)
                                              .toArray(new Integer[scenarioFrameCount]);

            FrameMetrics frameMetrics = new FrameMetrics(timestamps, durations, skippedFrames);
            removeUnusedFrames();

            return frameMetrics;
        }
    }

    @VisibleForTesting
    FrameMetrics getAllStoredMetricsForTesting() {
        synchronized (mLock) {
            Long[] timestamps = mTimestampsNs.toArray(new Long[mTimestampsNs.size()]);
            Long[] durations = mTotalDurationsNs.toArray(new Long[mTotalDurationsNs.size()]);
            Integer[] skippedFrames = mSkippedFrames.toArray(new Integer[mSkippedFrames.size()]);

            FrameMetrics frameMetrics = new FrameMetrics(timestamps, durations, skippedFrames);

            return frameMetrics;
        }
    }

    @GuardedBy("mLock")
    private void removeUnusedFrames() {
        if (mScenarioPreviousFrameTimestampNs.isEmpty()) {
            mTimestampsNs.clear();
            mTotalDurationsNs.clear();
            mSkippedFrames.clear();
            return;
        }

        long firstUsedTimestamp = findFirstUsedTimestamp();
        // If the earliest timestamp tracked is 0 then that scenario contains every frame
        // stored, so we shouldn't delete anything.
        if (firstUsedTimestamp == 0L) {
            return;
        }

        int firstUsedIndex = mTimestampsNs.indexOf(firstUsedTimestamp);
        if (firstUsedIndex == -1) {
            if (BuildConfig.ENABLE_ASSERTS) {
                throw new IllegalStateException("Timestamp for tracked scenario not found");
            }
            // This shouldn't happen.
            return;
        }

        mTimestampsNs.subList(0, firstUsedIndex).clear();
        mTotalDurationsNs.subList(0, firstUsedIndex).clear();
        mSkippedFrames.subList(0, firstUsedIndex).clear();
    }

    @GuardedBy("mLock")
    private long findFirstUsedTimestamp() {
        long firstTimestamp = Long.MAX_VALUE;
        for (long timestamp : mScenarioPreviousFrameTimestampNs.values()) {
            if (timestamp < firstTimestamp) {
                firstTimestamp = timestamp;
            }
        }

        return firstTimestamp;
    }
}
// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import java.util.ArrayList;

import javax.annotation.concurrent.GuardedBy;

/**
 * This class records frame durations and timestamps. And it calculates a jank burst metric.
 *
 * Jank bursts are periods with janky frames in a quick succession. A non-janky frame may be
 * included in this measurement if it's preceded and succeeded by janky frames, as long as the three
 * frames were drawn in a quick succession (called 'consecutive' in this file). Jank bursts are
 * delimited by either a succession (2 or more) of non-janky frames or a period of inactivity
 * (defined by JANK_BURST_CONSECUTIVE_FRAME_THRESHOLD_MS).
 *
 * Example: X = Janky frame. O = Non-janky frame.
 *
 *  O
 *  O
 *  X <- Jank burst 1 starts here.
 *  X
 *  O <- This frame is not janky, but as the adjacent frames are janky we include it.
 *  X <- Jank burst 1 ends here.
 *  O
 *  O
 *  X <- Jank burst 2 starts here.
 *  X <- Jank burst 2 ends here.
 *  500 ms later...
 *  X <- Jank burst 3 starts here. Because some time passed between the last frame and this one
 *       we assume their jankyness was related to different events/animations.
 *  X <- Jank burst 3 ends here.
 *  O
 *  O
 *
 *  In this example the jank burst metric would report 3 values, which are the sum of the frame
 *  duration of all frames inside a jank burst, including the first and last frames.
 */
class JankMetricMeasurement {
    protected static class JankMetric {
        private final Long[] mTimestampsNs;
        private final Long[] mDurationsNs;
        private final Long[] mJankBurstsNs;
        private final int mSkippedFrames;

        public JankMetric(
                Long[] timestampsNs, Long[] durationsNs, Long[] jankBurstsNs, int skippedFrames) {
            mTimestampsNs = timestampsNs;
            mDurationsNs = durationsNs;
            mJankBurstsNs = jankBurstsNs;
            mSkippedFrames = skippedFrames;
        }

        public Long[] getTimestamps() {
            return mTimestampsNs;
        }

        public Long[] getDurations() {
            return mDurationsNs;
        }

        public Long[] getJankBursts() {
            return mJankBurstsNs;
        }

        public int getSkippedFrames() {
            return mSkippedFrames;
        }
    }

    // Guards access to the fields below.
    private final Object mLock = new Object();

    // Array of timestamps stored in nanoseconds, they represent the moment when each frame
    // finished drawing, must always be the same size as mTotalDurationsNs.
    @GuardedBy("mLock")
    private final ArrayList<Long> mTimestampsNs;

    // Array of total durations stored in nanoseconds, they represent how long each frame took to
    // draw, must always be the same size as mTimestampsNs.
    @GuardedBy("mLock")
    private final ArrayList<Long> mTotalDurationsNs;

    // Number of frames that FrameMetrics was unable to report on, due to excessive activity on its
    // handler thread.
    @GuardedBy("mLock")
    private int mSkippedFrames;

    private static final long NANOSECONDS_PER_MILLISECOND = 1_000_000;

    // Threshold in milliseconds to distinguish janky and non-janky frames. Any frames whose
    // duration is greater than this number are considered janky. Ideally this number would depend
    // on the device's refresh rate, but for now we assume the device runs at 60hz.
    private static final long JANK_THRESHOLD_NS = 16 * NANOSECONDS_PER_MILLISECOND;

    // Maximum duration between frames to consider them consecutive. This is used to split jank
    // bursts when 2 janky frames are separated by an idle period.
    private static final long JANK_BURST_CONSECUTIVE_FRAME_THRESHOLD_NS =
            50 * NANOSECONDS_PER_MILLISECOND;

    JankMetricMeasurement() {
        mTimestampsNs = new ArrayList<>();
        mTotalDurationsNs = new ArrayList<>();
        mSkippedFrames = 0;
    }

    /**
     *  Records a timestamp and total draw duration for a single frame, both values are in
     * milliseconds.
     */
    void addFrameMeasurement(long timestampNs, long totalDurationNs, int skippedFrames) {
        synchronized (mLock) {
            mTimestampsNs.add(timestampNs);
            mTotalDurationsNs.add(totalDurationNs);
            mSkippedFrames += skippedFrames;
        }
    }

    /**
     *  Returns an array of all recorded jank bursts measured in nanoseconds (see class doc for
     * details).
     */
    private static Long[] calculateJankBurstDurationsNs(Long[] timestampsNs, Long[] durationsNs) {
        assert timestampsNs.length == durationsNs.length;
        ArrayList<Long> jankBurstDurationsNs = new ArrayList<>();
        long currentJankBurstDurationNs = 0;
        for (int i = 0; i < timestampsNs.length; i++) {
            // If there's an existing jank burst, but the current frame is not consecutive
            // compared to the last then we end the burst.
            if (i > 0 && currentJankBurstDurationNs > 0
                    && !areFramesConsecutive(i - 1, i, timestampsNs, durationsNs)) {
                jankBurstDurationsNs.add(currentJankBurstDurationNs);
                currentJankBurstDurationNs = 0;
            }

            long currentFrameDurationNs = durationsNs[i];

            // If the frame is janky we start or continue the jank burst.
            if (isFrameJanky(i, durationsNs)) {
                currentJankBurstDurationNs += currentFrameDurationNs;
            } else {
                if (currentJankBurstDurationNs > 0) {
                    // If the frame is not janky, but there's a jank burst and the adjacent
                    // frames are consecutive and janky then we count this frame's duration in
                    // the burst.
                    if (areFramesConsecutive(i - 1, i, timestampsNs, durationsNs)
                            && areFramesConsecutive(i, i + 1, timestampsNs, durationsNs)
                            && isFrameJanky(i + 1, durationsNs)) {
                        currentJankBurstDurationNs += currentFrameDurationNs;
                    } else {
                        // If the frame is not janky and the next frame is not janky or consecutive
                        // then we end the burst.
                        jankBurstDurationsNs.add(currentJankBurstDurationNs);
                        currentJankBurstDurationNs = 0;
                    }
                }
            }
        }

        // TODO(salg): Jank bursts may spread across measurements, consider removing this and
        // continuing the jank burst after we record more frames.
        if (currentJankBurstDurationNs > 0) {
            jankBurstDurationsNs.add(currentJankBurstDurationNs);
        }

        return jankBurstDurationsNs.toArray(new Long[jankBurstDurationsNs.size()]);
    }

    /**
     *  Returns a new object with metrics for all frames recorded since started or clear() was
     * called.
     */
    JankMetric getMetrics() {
        Long[] timestampsNs;
        Long[] totalDurationsNs;
        int skippedFrames;

        synchronized (mLock) {
            timestampsNs = mTimestampsNs.toArray(new Long[mTimestampsNs.size()]);
            totalDurationsNs = mTotalDurationsNs.toArray(new Long[mTotalDurationsNs.size()]);
            skippedFrames = mSkippedFrames;
        }

        Long[] jankBursts = calculateJankBurstDurationsNs(timestampsNs, totalDurationsNs);

        return new JankMetric(timestampsNs, totalDurationsNs, jankBursts, skippedFrames);
    }

    /**
     * Clears this measurement.
     */
    void clear() {
        synchronized (mLock) {
            mTimestampsNs.clear();
            mTotalDurationsNs.clear();
            mSkippedFrames = 0;
        }
    }

    /**
     * Checks if the frame at frameIndex is janky (i.e. Its duration is greater than 16 ms). Returns
     * false if frameIndex is out of bounds.
     */
    private static boolean isFrameJanky(int frameIndex, Long[] durationsNs) {
        if (frameIndex < 0 || frameIndex >= durationsNs.length) return false;

        long frameDurationNs = durationsNs[frameIndex];

        return frameDurationNs > JANK_THRESHOLD_NS;
    }

    /**
     * Checks if the frame at secondFrameIndex was drawn immediately after the frame at
     * firstFrameIndex (with a margin of 50ms defined in JANK_BURST_CONSECUTIVE_FRAME_THRESHOLD_MS).
     * Timestamps are recorded at the end of each frame, so we compare the end of the first frame
     * and the beginning of the second frame. If either index is out of bounds then we return false.
     */
    private static boolean areFramesConsecutive(
            int firstFrameIndex, int secondFrameIndex, Long[] timestampsNs, Long[] durationsNs) {
        assert firstFrameIndex < secondFrameIndex;
        assert timestampsNs.length == durationsNs.length;
        if (firstFrameIndex < 0 || secondFrameIndex < 0) {
            return false;
        }
        if (firstFrameIndex >= timestampsNs.length || secondFrameIndex >= timestampsNs.length) {
            return false;
        }

        long firstFrameEndNs = timestampsNs[firstFrameIndex];

        long secondFrameEndNs = timestampsNs[secondFrameIndex];
        long secondFrameDurationNs = durationsNs[secondFrameIndex];
        long secondFrameStartNs = secondFrameEndNs - secondFrameDurationNs;

        long timeBetweenFramesNs = secondFrameStartNs - firstFrameEndNs;

        return (timeBetweenFramesNs < JANK_BURST_CONSECUTIVE_FRAME_THRESHOLD_NS);
    }
}
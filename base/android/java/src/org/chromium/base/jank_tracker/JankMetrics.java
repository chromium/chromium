// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

/**
 * This class is a container for jank metrics, which are processed FrameMetrics ready to be uploaded
 * to UMA.
 */
class JankMetrics {
    public final long[] timestampsNs;
    public final long[] durationsNs;
    public final int[] missedVsyncs;
    public final boolean[] isJanky;

    public JankMetrics() {
        timestampsNs = new long[0];
        durationsNs = new long[0];
        missedVsyncs = new int[0];
        isJanky = new boolean[0];
    }

    public JankMetrics(long[] timestampsNs, long[] durationsNs, int[] missedVsyncs) {
        this.timestampsNs = timestampsNs;
        this.durationsNs = durationsNs;
        this.missedVsyncs = missedVsyncs;
        isJanky = new boolean[0];
    }
}

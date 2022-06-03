// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

/**
 * Container for individual frame metrics as reported by Android's FrameMetrics API.
 */
class FrameMetrics {
    public final Long[] timestampsNs;
    public final Long[] durationsNs;
    public final Integer[] skippedFrames;

    public FrameMetrics(Long[] timestampsNs, Long[] durationsNs, Integer[] skippedFrames) {
        this.timestampsNs = timestampsNs;
        this.durationsNs = durationsNs;
        this.skippedFrames = skippedFrames;
    }

    public FrameMetrics() {
        this.timestampsNs = new Long[0];
        this.durationsNs = new Long[0];
        this.skippedFrames = new Integer[0];
    }
}

// Copyright 2021 The Chromium Authors. All rights reserved.
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
    public final long[] jankBurstsNs;
    public final int skippedFrames;

    public JankMetrics(
            long[] timestampsNs, long[] durationsNs, long[] jankBurstsNs, int skippedFrames) {
        this.timestampsNs = timestampsNs;
        this.durationsNs = durationsNs;
        this.jankBurstsNs = jankBurstsNs;
        this.skippedFrames = skippedFrames;
    }
}

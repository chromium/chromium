// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

/**
 * This class is a container for jank metrics, which are processed FrameMetrics ready to be uploaded
 * to UMA.
 */
class JankMetrics {
    public final long[] durationsNs;
    public final boolean[] isJanky;

    public JankMetrics(long[] durationsNs, boolean[] isJanky) {
        this.durationsNs = durationsNs;
        this.isJanky = isJanky;
    }
}

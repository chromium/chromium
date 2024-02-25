// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

/**
 * A simple Data structure that holds a uptimeNanos that we wish to have data up until, and a delay
 * to wait for this data from the Android FrameMetrics API.
 */
public final class JankEndScenarioTime {
    public final long endScenarioTimeNs;
    // 100ms should be long enough to receive frame metric timeline if they haven't been dropped.
    public final long timeoutDelayMs = 100;

    public static JankEndScenarioTime endAt(long uptimeNanos) {
        if (uptimeNanos <= 0) {
            return null;
        }
        return new JankEndScenarioTime(uptimeNanos);
    }

    private JankEndScenarioTime(long uptimeNanos) {
        endScenarioTimeNs = uptimeNanos;
    }
}

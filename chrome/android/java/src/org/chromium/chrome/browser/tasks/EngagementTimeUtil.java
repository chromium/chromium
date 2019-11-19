// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import org.chromium.base.TimeUtilsJni;

import java.util.concurrent.TimeUnit;

/**
 * Utility class to provide engagement time helper methods.
 */
public class EngagementTimeUtil {
    // This is the point at which a user has most likely started a clobber or "focused" the tab.
    private static final long TAB_CLOBBER_THRESHOLD_MS = 10000L;
    private static final long INVALID_TIME = -1;

    /**
     * Provide the current time in milliseconds.
     *
     * @return long - the current time in milliseconds.
     */
    public long currentTime() {
        return System.currentTimeMillis();
    }

    /**
     * Given the last engagement timestamp, return the elapsed time in milliseconds since that time.
     *
     * @param lastEngagementMs - time of the last engagement
     * @return time in milliseconds that have elapsed since lastEngagementMs
     */
    public long timeSinceLastEngagement(long lastEngagementMs) {
        return timeSinceLastEngagement(lastEngagementMs, currentTime());
    }

    /**
     * Given the last engagement timestamp and a current engagement time to compare, return the
     * elapsed time in milliseconds between the two.
     *
     * @return time in milliseconds that has elapsed between last engagement and current engagement.
     */
    public long timeSinceLastEngagement(long lastEngagementMs, long currentEngagementMs) {
        long elapsedMs = currentEngagementMs - lastEngagementMs;

        if (elapsedMs < 0) return INVALID_TIME;

        return elapsedMs;
    }

    /**
     * Given the last engagement time, typically set by System.currentTimeMillis(), and the current
     * event timestamp in base::TimeTicks in milliseconds, this computes the elapsed time between
     * the two in milliseconds.
     *
     * @param lastEngagementMs Timestamp of the last engagement in milliseconds.
     * @param currentEngagementTicksMs Timestamp of the current event in TimeTicks (ms)
     * @return
     */
    public long timeSinceLastEngagementFromTimeTicksMs(
            final long lastEngagementMs, final long currentEngagementTicksMs) {
        final long currentTimeMs = currentTime();
        final long currentTimeTicksUs = TimeUtilsJni.get().getTimeTicksNowUs();
        final long currentTimeTicksMs = TimeUnit.MICROSECONDS.toMillis(currentTimeTicksUs);
        final long offsetMs = currentTimeTicksMs - currentEngagementTicksMs;
        final long currentEngagementTimeMs = currentTimeMs - offsetMs;
        final long elapsedMs = currentEngagementTimeMs - lastEngagementMs;

        if (elapsedMs < 0) return INVALID_TIME;

        return elapsedMs;
    }

    /**
     * Return the threshold within which a tab is considered "engaged" and a navigation away is no
     * longer considered a clobber.
     *
     * @return configured tab clobber threshold in milliseconds.
     */
    public long tabClobberThresholdMillis() {
        return TAB_CLOBBER_THRESHOLD_MS;
    }
}

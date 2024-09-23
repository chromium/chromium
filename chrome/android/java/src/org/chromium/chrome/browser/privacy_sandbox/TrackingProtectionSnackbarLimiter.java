// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import java.util.HashMap;
import java.util.Map;

/** Rate limiter for tracking protection snackbar to prevent excessive display. */
public class TrackingProtectionSnackbarLimiter {
    private static final long MINIMUM_TIME_BETWEEN_REQUESTS_MS = 5 * 60 * 1000; // 5 minutes

    private final Map<String, Long> mLastRequestTimes = new HashMap<>();

    /**
     * Determines whether a request to show the snackbar for a given host should be allowed.
     *
     * @param host The host for which the snackbar display is being requested.
     * @return true if the request is allowed (enough time has passed since the last display), false
     *     otherwise.
     */
    public boolean shouldAllowRequest(String host) {
        return shouldAllowRequest(host, System.currentTimeMillis());
    }

    boolean shouldAllowRequest(String host, long currentTime) {
        if (mLastRequestTimes.containsKey(host)) {
            long lastRequestTime = mLastRequestTimes.getOrDefault(host, 0L);
            if (currentTime - lastRequestTime < MINIMUM_TIME_BETWEEN_REQUESTS_MS) {
                return false;
            }
        }

        // Update the last request time and allow the request
        mLastRequestTimes.put(host, currentTime);
        return true;
    }
}

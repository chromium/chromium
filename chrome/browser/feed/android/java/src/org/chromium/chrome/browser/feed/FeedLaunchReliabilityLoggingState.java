// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger;
import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger.SurfaceType;

/**
 * Holds information about feed surface creation until the surface's {@link
 * FeedLaunchReliabilityLogger} is available.
 */
public class FeedLaunchReliabilityLoggingState {
    private final @SurfaceType int mSurfaceType;
    private long mUiStartingTimestamp;

    /**
     * Record the surface type and creation time.
     * @param surfaceType Feed surface type.
     * @param uiStartingTimestamp Time at which the feed surface was
     *         constructed in nanoseconds since system boot (System.nanoTime()).
     */
    public FeedLaunchReliabilityLoggingState(
            @SurfaceType int surfaceType, long uiStartingTimestamp) {
        mSurfaceType = surfaceType;
        mUiStartingTimestamp = uiStartingTimestamp;
    }

    /**
     * Log the UI starting event. Should be called once.
     * @param logger The XSurface feed launch reliability logger.
     */
    public void onLoggerAvailable(FeedLaunchReliabilityLogger logger) {
        logger.logUiStarting(mSurfaceType, mUiStartingTimestamp);
    }
}
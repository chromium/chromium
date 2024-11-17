// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.cached_flags.IntCachedFieldTrialParameter;

/**
 * Because JankTracker lives in //base/android it can't use things like field trial parameters so
 * this class is a convenient spot to hold this information.
 *
 * <p>Once ChromeFeatureList.COLLECT_ANDROID_FRAME_TIMELINE_METRICS is enabled by default this class
 * should be cleaned up as part of that.
 */
public final class JankTrackerExperiment {
    // The parameter to control how quickly the JankTracker should be enabled. Some JankTracker
    // implementations can be quite heavy (including thread creation and the link). To avoid
    // impacting start up we support delaying the construction of the JankTracker.
    public static final String PARAM_JANK_TRACKER_DELAYED_START_MS = "delayed_start_ms";
    // Enough to push us past the median start up time without losing to much of feed interactions.
    public static final int DEFAULT_JANK_TRACKED_DELAYED_START_MS = 3000;
    // This gets the finch feature and caches it into shared preferences for the next run.
    public static final IntCachedFieldTrialParameter JANK_TRACKER_DELAYED_START_MS =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.COLLECT_ANDROID_FRAME_TIMELINE_METRICS,
                    PARAM_JANK_TRACKER_DELAYED_START_MS,
                    DEFAULT_JANK_TRACKED_DELAYED_START_MS);
}

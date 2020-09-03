// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.shared;

import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Helper methods covering more complex Feed related feature checks and states.
 */
public final class FeedFeatures {
    /**
     * @return Whether implicit Feed user actions are being reported based on feature states. Can be
     *         used for both Feed v1 and v2.
     */
    public static boolean isReportingUserActions() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.INTEREST_FEED_V2)
                || (ChromeFeatureList.isEnabled(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
                        && ChromeFeatureList.isEnabled(ChromeFeatureList.REPORT_FEED_USER_ACTIONS));
    }

    /**
     * Identical to {@link isReportingUserActions} but uses {@link CachedFeatureFlags} for checking
     * feature states.
     */
    public static boolean cachedIsReportingUserActions() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.INTEREST_FEED_V2)
                || (CachedFeatureFlags.isEnabled(
                            ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
                        && CachedFeatureFlags.isEnabled(
                                ChromeFeatureList.REPORT_FEED_USER_ACTIONS));
    }

    public static boolean isV2Enabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.INTEREST_FEED_V2);
    }
}

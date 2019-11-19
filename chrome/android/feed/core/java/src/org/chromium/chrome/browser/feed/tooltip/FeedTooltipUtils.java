// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.tooltip;

import androidx.annotation.Nullable;

import com.google.android.libraries.feed.api.host.stream.TooltipApi;
import com.google.android.libraries.feed.api.host.stream.TooltipInfo;
import com.google.android.libraries.feed.api.host.stream.TooltipSupportedApi;

import org.chromium.components.feature_engagement.FeatureConstants;

/**
 * Helper methods for implementation of {@link TooltipApi} and {@link TooltipSupportedApi}.
 */
class FeedTooltipUtils {
    /**
     * @param featureName The feature name from {@link TooltipInfo.FeatureName}.
     * @return A corresponding IPH feature name from {@link FeatureConstants} for the specified
     *         {@code featureName}.
     */
    @Nullable
    @FeatureConstants
    static String getFeatureForIPH(@TooltipInfo.FeatureName String featureName) {
        switch (featureName) {
            case TooltipInfo.FeatureName.CARD_MENU_TOOLTIP:
                return FeatureConstants.FEED_CARD_MENU_FEATURE;
            case TooltipInfo.FeatureName.UNKNOWN:
                return null;
            default:
                assert false
                    : String.format("Unknown mapping to IPH feature for TooltipInfo.FeatureName %s",
                              featureName);
                return null;
        }
    }
}

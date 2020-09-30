// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1.tooltip;

import android.text.TextUtils;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipSupportedApi;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.Tracker;

/**
 * A basic implementation of the {@link TooltipSupportedApi}.
 */
public class BasicTooltipSupportedApi implements TooltipSupportedApi {
    @Override
    public void wouldTriggerHelpUi(String featureName, Consumer<Boolean> consumer) {
        final String featureForIPH = FeedTooltipUtils.getFeatureForIPH(featureName);
        if (TextUtils.isEmpty(featureForIPH)) {
            consumer.accept(false);
            return;
        }

        final Tracker tracker =
                TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
        consumer.accept(tracker.wouldTriggerHelpUI(featureForIPH));
    }
}

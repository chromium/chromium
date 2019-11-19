// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.tooltip;

import android.text.TextUtils;

import com.google.android.libraries.feed.api.host.stream.TooltipSupportedApi;
import com.google.android.libraries.feed.common.functional.Consumer;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
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

        final Tracker tracker = TrackerFactory.getTrackerForProfile(
                Profile.getLastUsedProfile().getOriginalProfile());
        consumer.accept(tracker.wouldTriggerHelpUI(featureForIPH));
    }
}

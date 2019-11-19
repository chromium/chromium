// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.tooltip;

import android.text.TextUtils;
import android.view.View;

import com.google.android.libraries.feed.api.host.stream.TooltipApi;
import com.google.android.libraries.feed.api.host.stream.TooltipCallbackApi;
import com.google.android.libraries.feed.api.host.stream.TooltipInfo;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * A basic implementation of the {@link TooltipApi}.
 */
public class BasicTooltipApi implements TooltipApi {
    private static final int TEXT_BUBBLE_TIMEOUT_MS = 10000;

    @Override
    public boolean maybeShowHelpUi(
            TooltipInfo tooltipInfo, View view, TooltipCallbackApi tooltipCallback) {
        final String featureForIPH =
                FeedTooltipUtils.getFeatureForIPH(tooltipInfo.getFeatureName());
        if (TextUtils.isEmpty(featureForIPH)) return false;

        final Tracker tracker = TrackerFactory.getTrackerForProfile(
                Profile.getLastUsedProfile().getOriginalProfile());
        if (!tracker.shouldTriggerHelpUI(featureForIPH)) return false;

        ViewRectProvider rectProvider = new ViewRectProvider(view);
        rectProvider.setInsetPx(0, tooltipInfo.getTopInset(), 0, tooltipInfo.getBottomInset());

        TextBubble textBubble = new TextBubble(view.getContext(), view, tooltipInfo.getLabel(),
                tooltipInfo.getAccessibilityLabel(), true, rectProvider);
        textBubble.setAutoDismissTimeout(TEXT_BUBBLE_TIMEOUT_MS);
        textBubble.addOnDismissListener(() -> {
            tracker.dismissed(featureForIPH);
            tooltipCallback.onHide(TooltipCallbackApi.TooltipDismissType.TIMEOUT);
        });

        textBubble.show();
        tooltipCallback.onShow();
        return true;
    }
}

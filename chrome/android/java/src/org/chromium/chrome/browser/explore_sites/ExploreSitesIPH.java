// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.widget.highlight.PulseDrawable;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * Controls IPH for the Explore Sites features.
 */
public class ExploreSitesIPH {
    public static void configureIPH(TileView tileView, Profile profile) {
        Context context = tileView.getContext();
        if (context == null) return;
        Activity activity = ContextUtils.activityFromContext(context);
        if (activity == null) return;

        if (tileView.isAttachedToWindow()) {
            maybeShowIPH(tileView, profile, activity);
        } else {
            tileView.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
                @Override
                public void onViewAttachedToWindow(View v) {
                    maybeShowIPH(tileView, profile, activity);
                }

                @Override
                public void onViewDetachedFromWindow(View v) {}
            });
        }
    }

    private static void maybeShowIPH(TileView tileView, Profile profile, Activity activity) {
        if (ActivityUtils.isActivityFinishingOrDestroyed(activity)) return;

        final String contentString =
                tileView.getContext().getString(org.chromium.chrome.R.string.explore_sites_iph);
        assert (contentString.length() > 0);

        final String accessibilityString = tileView.getContext().getString(
                org.chromium.chrome.R.string.explore_sites_iph_accessibility);
        assert (accessibilityString.length() > 0);

        final Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        if (!tracker.shouldTriggerHelpUI(FeatureConstants.EXPLORE_SITES_TILE_FEATURE)) return;

        ViewRectProvider rectProvider = new ViewRectProvider(tileView);

        TextBubble textBubble =
                new TextBubble(tileView.getContext(), tileView, contentString, accessibilityString,
                        true, rectProvider, ChromeAccessibilityUtil.get().isAccessibilityEnabled());
        textBubble.setDismissOnTouchInteraction(true);
        View foregroundView = tileView.findViewById(org.chromium.chrome.R.id.tile_view_highlight);
        if (foregroundView == null) return;

        HighlightParams params = new HighlightParams(HighlightShape.CIRCLE);
        params.setCircleRadius(
                new PulseDrawable.Bounds() {
                    @Override
                    public float getMaxRadiusPx(Rect bounds) {
                        return Math.min(bounds.width(), bounds.height()) / 2.f;
                    }
                    @Override
                    public float getMinRadiusPx(Rect bounds) {
                        // Radius is half of the min of width and height, divided by 1.5.
                        // This simplifies to min of width and height divided by 3.
                        return Math.min(bounds.width(), bounds.height()) / 3.f;
                    }
                });
        ViewHighlighter.turnOnHighlight(foregroundView, params);

        textBubble.addOnDismissListener(() -> {
            ViewHighlighter.turnOffHighlight(foregroundView);

            tracker.dismissed(FeatureConstants.EXPLORE_SITES_TILE_FEATURE);
        });
        textBubble.show();
    }
}

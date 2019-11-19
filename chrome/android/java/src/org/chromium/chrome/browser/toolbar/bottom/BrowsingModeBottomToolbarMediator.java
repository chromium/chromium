// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.view.View;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.widget.FeatureHighlightProvider;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/**
 * This class is responsible for reacting to events from the outside world, interacting with other
 * coordinators, running most of the business logic associated with the browsing mode bottom
 * toolbar, and updating the model accordingly.
 */
class BrowsingModeBottomToolbarMediator implements ThemeColorObserver {
    /** The amount of time to show the Duet help bubble for. */
    private static final int DUET_IPH_BUBBLE_SHOW_DURATION_MS = 10000;

    /** The transparency fraction of the IPH bubble. */
    private static final float DUET_IPH_BUBBLE_ALPHA_FRACTION = 0.9f;

    /** The model for the browsing mode bottom toolbar that holds all of its state. */
    private final BrowsingModeBottomToolbarModel mModel;
    private final OverviewModeObserver mOverviewModeObserver;

    /** The overview mode manager. */
    private OverviewModeBehavior mOverviewModeBehavior;

    /** A provider that notifies components when the theme color changes.*/
    private ThemeColorProvider mThemeColorProvider;

    /**
     * Build a new mediator that handles events from outside the bottom toolbar.
     * @param model The {@link BrowsingModeBottomToolbarModel} that holds all the state for the
     *              browsing mode  bottom toolbar.
     */
    BrowsingModeBottomToolbarMediator(BrowsingModeBottomToolbarModel model) {
        mModel = model;
        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeStartedShowing(boolean showToolbar) {
                mModel.set(BrowsingModeBottomToolbarModel.IS_VISIBLE, false);
            }

            @Override
            public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
                mModel.set(BrowsingModeBottomToolbarModel.IS_VISIBLE, true);
            }
        };
    }

    void setThemeColorProvider(ThemeColorProvider themeColorProvider) {
        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addThemeColorObserver(this);
    }

    void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
        }
        mOverviewModeBehavior = overviewModeBehavior;
        mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
    }

    /**
     * Maybe show the IPH bubble for Chrome Duet.
     * @param activity An activity to attach the IPH to.
     * @param anchor The view to anchor the IPH to.
     * @param tracker A tracker for IPH.
     */
    void showIPH(ChromeActivity activity, View anchor, Tracker tracker,
            Runnable completeRunnable) {
        if (!tracker.shouldTriggerHelpUI(FeatureConstants.CHROME_DUET_FEATURE)) return;
        int baseColor =
                ApiCompatibilityUtils.getColor(anchor.getResources(), R.color.modern_blue_600);

        // Clear out the alpha and use custom transparency.
        int finalColor =
                (baseColor & 0x00FFFFFF) | ((int) (DUET_IPH_BUBBLE_ALPHA_FRACTION * 255) << 24);

        FeatureHighlightProvider.getInstance().buildForView(activity, anchor,
                R.string.iph_duet_title, FeatureHighlightProvider.TextAlignment.CENTER,
                R.style.TextAppearance_WhiteTitle1, R.string.iph_duet_description,
                FeatureHighlightProvider.TextAlignment.CENTER, R.style.TextAppearance_WhiteBody,
                finalColor, DUET_IPH_BUBBLE_SHOW_DURATION_MS, completeRunnable);

        anchor.postDelayed(() -> tracker.dismissed(FeatureConstants.CHROME_DUET_FEATURE),
                DUET_IPH_BUBBLE_SHOW_DURATION_MS);
    }

    /**
     * Clean up anything that needs to be when the bottom toolbar is destroyed.
     */
    void destroy() {
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
            mOverviewModeBehavior = null;
        }
        if (mThemeColorProvider != null) {
            mThemeColorProvider.removeThemeColorObserver(this);
            mThemeColorProvider = null;
        }
    }

    @Override
    public void onThemeColorChanged(int primaryColor, boolean shouldAnimate) {
        mModel.set(BrowsingModeBottomToolbarModel.PRIMARY_COLOR, primaryColor);
    }
}

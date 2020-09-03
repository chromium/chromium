// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.app.Activity;

import androidx.annotation.ColorInt;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.components.browser_ui.widget.FeatureHighlightProvider;

/**
 * This class is responsible for reacting to events from the outside world, interacting with other
 * coordinators, running most of the business logic associated with the browsing mode bottom
 * toolbar, and updating the model accordingly.
 */
class BrowsingModeBottomToolbarMediator implements ThemeColorObserver {
    /** The transparency fraction of the IPH bubble. */
    private static final float DUET_IPH_BUBBLE_ALPHA_FRACTION = 0.9f;

    /** The transparency fraction of the IPH background. */
    private static final float DUET_IPH_BACKGROUND_ALPHA_FRACTION = 0.3f;

    /** The dismissable parameter name of the IPH. */
    static final String DUET_IPH_TAP_TO_DISMISS_PARAM_NAME = "duet_iph_tap_to_dismiss_enabled";

    /** The model for the browsing mode bottom toolbar that holds all of its state. */
    private final BrowsingModeBottomToolbarModel mModel;

    /** The overview mode manager. */
    private OverviewModeBehavior mOverviewModeBehavior;

    /** A provider that notifies components when the theme color changes.*/
    private ThemeColorProvider mThemeColorProvider;

    private FeatureHighlightProvider mFeatureHighlightProvider;

    /**
     * Build a new mediator that handles events from outside the bottom toolbar.
     * @param model The {@link BrowsingModeBottomToolbarModel} that holds all the state for the
     *              browsing mode  bottom toolbar.
     */
    BrowsingModeBottomToolbarMediator(BrowsingModeBottomToolbarModel model) {
        mModel = model;
        mFeatureHighlightProvider = AppHooks.get().createFeatureHighlightProvider();
    }

    void setThemeColorProvider(ThemeColorProvider themeColorProvider) {
        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addThemeColorObserver(this);
    }

    /**
     * Dismiss the IPH bubble for Chrome Duet.
     * @param activity An activity to attach the IPH to.
     */
    void dismissIPH(Activity activity) {
        mFeatureHighlightProvider.dismiss((AppCompatActivity) activity);
    }

    /**
     * Clean up anything that needs to be when the bottom toolbar is destroyed.
     */
    void destroy() {
        if (mThemeColorProvider != null) {
            mThemeColorProvider.removeThemeColorObserver(this);
            mThemeColorProvider = null;
        }
    }

    @Override
    public void onThemeColorChanged(int primaryColor, boolean shouldAnimate) {
        mModel.set(BrowsingModeBottomToolbarModel.PRIMARY_COLOR, primaryColor);
    }

    /**
     * Set the alpha for the color.
     * @param baseColor The color which alpha will apply to.
     * @param alpha The desired alpha for the color. The value should between 0 to 1. 0 means total
     *         transparency, 1 means total non-transparency.
     */
    private @ColorInt int applyCustomAlphaToColor(@ColorInt int baseColor, float alpha) {
        return (baseColor & 0x00FFFFFF) | ((int) (alpha * 255) << 24);
    }
}

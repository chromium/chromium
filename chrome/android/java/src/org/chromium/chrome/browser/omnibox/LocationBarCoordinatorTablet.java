// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.graphics.drawable.Drawable;
import android.view.View;

import java.util.List;

/**
 * A supplement to {@link LocationBarCoordinator} with methods specific to larger devices.
 */
public class LocationBarCoordinatorTablet implements LocationBarCoordinator.SubCoordinator {
    private LocationBarTablet mLocationBarTablet;

    public LocationBarCoordinatorTablet(LocationBarTablet tabletLayout) {
        mLocationBarTablet = tabletLayout;
    }

    @Override
    public void destroy() {
        mLocationBarTablet = null;
    }

    /**
     * @param button The {@link View} of the button to hide.
     * @return An animator to run for the given view when hiding buttons in the unfocused
     *         location bar. This should also be used to create animators for hiding toolbar
     *         buttons.
     */
    public ObjectAnimator createHideButtonAnimator(View button) {
        return mLocationBarTablet.createHideButtonAnimator(button);
    }

    /**
     * @param button The {@link View} of the button to show.
     * @return An animator to run for the given view when showing buttons in the unfocused
     *         location bar. This should also be used to create animators for showing toolbar
     *         buttons.
     */
    public ObjectAnimator createShowButtonAnimator(View button) {
        return mLocationBarTablet.createShowButtonAnimator(button);
    }

    /**
     * Creates animators for hiding buttons in the unfocused location bar. The buttons fade out
     * while width of the location bar gets larger. There are toolbar buttons that also hide at
     * the same time, causing the width of the location bar to change.
     *
     * @param toolbarStartPaddingDifference The difference in the toolbar's start padding
     *         between the beginning and end of the animation.
     * @return A list of animators to run.
     */
    public List<Animator> getHideButtonsWhenUnfocusedAnimators(int toolbarStartPaddingDifference) {
        return mLocationBarTablet.getHideButtonsWhenUnfocusedAnimators(
                toolbarStartPaddingDifference);
    }

    /**
     * Creates animators for showing buttons in the unfocused location bar. The buttons fade in
     * while width of the location bar gets smaller. There are toolbar buttons that also show at
     * the same time, causing the width of the location bar to change.
     *
     * @param toolbarStartPaddingDifference The difference in the toolbar's start padding
     *         between the beginning and end of the animation.
     * @return A list of animators to run.
     */
    public List<Animator> getShowButtonsWhenUnfocusedAnimators(int toolbarStartPaddingDifference) {
        return mLocationBarTablet.getShowButtonsWhenUnfocusedAnimators(
                toolbarStartPaddingDifference);
    }

    /** Sets, whether buttons should be displayed in the URL bar when it's not focused. */
    public void setShouldShowButtonsWhenUnfocused(boolean shouldShowButtons) {
        mLocationBarTablet.setShouldShowButtonsWhenUnfocused(shouldShowButtons);
    }

    /** Updates the visibility of the buttons inside the location bar. */
    public void updateButtonVisibility() {
        mLocationBarTablet.updateButtonVisibility();
    }

    /**
     * Gets the background drawable.
     *
     * <p>TODO(1133482): Hide this View interaction if possible.
     *
     * @see View#getBackground()
     */
    public Drawable getBackground() {
        return mLocationBarTablet.getBackground();
    }
}

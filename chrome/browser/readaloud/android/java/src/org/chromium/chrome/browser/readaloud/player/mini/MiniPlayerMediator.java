// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator class responsible for controlling Read Aloud mini player.
 *
 * <p>The show animation has the following steps:
 *
 * <li>Set player visibility from GONE to INVISIBLE to cause it to layout, getting its height. Make
 *     the compositor scene layer visible.
 * <li>Grow bottom controls to cause web contents to shrink and make room for the player. The scene
 *     layer pretends to be the real player and slides up smoothly with the changing bottom controls
 *     min height.
 * <li>When the resize is done, make the player VISIBLE with transparent contents.
 * <li>Fade in the contents.
 *
 * <p>The hide animation is the reverse of the show animation:
 *
 * <li>Fade out the player contents.
 * <li>Make the scene layer visible and set the player visibility to GONE.
 * <li>Shrink the bottom controls and move the scene layer down along with the changing bottom
 *     controls min height.
 */
public class MiniPlayerMediator {
    private final PropertyModel mModel;
    private final BrowserControlsSizer mBrowserControlsSizer;
    // Height of MiniPlayerLayout's background (without shadow).
    private int mLayoutHeightPx;

    private final BrowserControlsStateProvider.Observer mBrowserControlsStateObserver =
            new BrowserControlsStateProvider.Observer() {
                @Override
                public void onControlsOffsetChanged(
                        int topOffset,
                        int topControlsMinHeightOffset,
                        int bottomOffset,
                        int bottomControlsMinHeightOffset,
                        boolean needsAnimate) {
                    if (getVisibility() == VisibilityState.HIDING
                            && bottomControlsMinHeightOffset == 0) {
                        onBottomControlsShrunk();
                    } else if (getVisibility() == VisibilityState.SHOWING
                            && mLayoutHeightPx == bottomControlsMinHeightOffset) {
                        onBottomControlsGrown();
                    }
                }
            };

    MiniPlayerMediator(BrowserControlsSizer browserControlsSizer) {
        mModel =
                new PropertyModel.Builder(Properties.ALL_KEYS)
                        .with(Properties.VISIBILITY, VisibilityState.GONE)
                        .with(Properties.ANDROID_VIEW_VISIBILITY, View.GONE)
                        .with(Properties.COMPOSITED_VIEW_VISIBLE, false)
                        .with(Properties.MEDIATOR, this)
                        .build();
        mBrowserControlsSizer = browserControlsSizer;
        mBrowserControlsSizer.addObserver(mBrowserControlsStateObserver);
    }

    void destroy() {
        mBrowserControlsSizer.removeObserver(mBrowserControlsStateObserver);
    }

    @VisibilityState
    int getVisibility() {
        return mModel.get(Properties.VISIBILITY);
    }

    PropertyModel getModel() {
        return mModel;
    }

    /// Show

    // (1) Grow bottom controls to accommodate player if height is known, otherwise get height and
    // then grow.
    void show(boolean animate) {
        @VisibilityState int currentVisibility = getVisibility();
        if (currentVisibility == VisibilityState.SHOWING
                || currentVisibility == VisibilityState.VISIBLE) {
            return;
        }

        mModel.set(Properties.VISIBILITY, VisibilityState.SHOWING);
        mModel.set(Properties.ANIMATE_VISIBILITY_CHANGES, animate);
        mModel.set(Properties.COMPOSITED_VIEW_VISIBLE, true);

        if (mLayoutHeightPx != 0) {
            // Grow immediately if height is already known.
            growBottomControls();
        }
        // Set player visibility from GONE to INVISIBLE so that it has a height.
        mModel.set(Properties.ANDROID_VIEW_VISIBILITY, View.INVISIBLE);
    }

    /**
     * Called by MiniPlayerLayout during onLayout() with its height minus shadow.
     *
     * @param heightPx Height of MiniPlayerLayout minus the top shadow height.
     */
    void onHeightKnown(int heightPx) {
        // (1.5) Grow bottom controls once player height has been measured.
        if (getVisibility() == VisibilityState.SHOWING && heightPx > 0 && mLayoutHeightPx == 0) {
            mLayoutHeightPx = heightPx;
            mModel.set(Properties.HEIGHT, heightPx);
            growBottomControls();
        }
    }

    // (2) Finished growing, start fading in.
    private void onBottomControlsGrown() {
        // Step two: fade in if transition is animated, or jump to full opacity otherwise.
        mModel.set(Properties.ANDROID_VIEW_VISIBILITY, View.VISIBLE);
        mModel.set(Properties.CONTENTS_OPAQUE, true);
    }

    // (3) Done.
    void onFullOpacityReached() {
        // show() is finished!
        onTransitionFinished(VisibilityState.VISIBLE);
    }

    /// Dismiss

    // (1) Fade out.
    void dismiss(boolean animate) {
        @VisibilityState int currentVisibility = getVisibility();
        if (currentVisibility == VisibilityState.HIDING
                || currentVisibility == VisibilityState.GONE) {
            return;
        }

        mModel.set(Properties.ANIMATE_VISIBILITY_CHANGES, animate);
        mModel.set(Properties.VISIBILITY, VisibilityState.HIDING);
        // Fade out if transition is animated, or jump to zero opacity otherwise.
        mModel.set(Properties.CONTENTS_OPAQUE, false);
    }

    // (2) Finished fading out, now pull down.
    void onZeroOpacityReached() {
        mModel.set(Properties.ANDROID_VIEW_VISIBILITY, View.GONE);
        shrinkBottomControls();
    }

    // (3) Done.
    private void onBottomControlsShrunk() {
        mModel.set(Properties.COMPOSITED_VIEW_VISIBLE, false);
        onTransitionFinished(VisibilityState.GONE);
    }

    /**
     * Called when the view visibility changes due to animation.
     *
     * @param newState New visibility.
     */
    public void onTransitionFinished(@VisibilityState int newState) {
        mModel.set(Properties.VISIBILITY, newState);
    }

    void onBackgroundColorUpdated(@ColorInt int backgroundColorArgb) {
        mModel.set(Properties.BACKGROUND_COLOR_ARGB, backgroundColorArgb);
    }

    private void growBottomControls() {
        setBottomControlsHeight(
                mBrowserControlsSizer.getBottomControlsHeight() + mLayoutHeightPx, mLayoutHeightPx);
    }

    private void shrinkBottomControls() {
        setBottomControlsHeight(
                Math.max(mBrowserControlsSizer.getBottomControlsHeight() - mLayoutHeightPx, 0), 0);
    }

    private void setBottomControlsHeight(int height, int minHeight) {
        mBrowserControlsSizer.setAnimateBrowserControlsHeightChanges(
                mModel.get(Properties.ANIMATE_VISIBILITY_CHANGES));
        mBrowserControlsSizer.setBottomControlsHeight(height, minHeight);
    }
}

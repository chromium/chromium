// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.browser_controls.BottomControlsLayer;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerVisibility;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator class responsible for controlling Read Aloud mini player.
 *
 * <p>The show animation has the following steps:
 * <li>Set player visibility from GONE to VISIBLE to cause it to layout, getting its height, but
 *     leave it transparent. Make the compositor scene layer visible.
 * <li>Grow bottom controls to cause web contents to shrink and make room for the player. The scene
 *     layer pretends to be the real player and slides up smoothly with the changing bottom controls
 *     min height.
 * <li>When the resize is done, fade in the contents.
 *
 *     <p>The hide animation is the reverse of the show animation:
 * <li>Fade out the player contents.
 * <li>Make the scene layer visible and set the player visibility to GONE.
 * <li>Shrink the bottom controls and move the scene layer down along with the changing bottom
 *     controls min height.
 */
public class MiniPlayerMediator implements BottomControlsLayer {
    private final PropertyModel mModel;
    private final BottomControlsStacker mBottomControlsStacker;
    private MiniPlayerCoordinator mCoordinator;
    // Height of MiniPlayerLayout's background (without shadow).
    private int mLayoutHeightPx;
    // Height of the mini player for the purposes of calculations for the bottom browser controls.
    private int mBottomControlsLayerHeightPx;
    private boolean mIsAnimationStarted;
    private final BrowserControlsStateProvider.Observer mBrowserControlsStateObserver =
            new BrowserControlsStateProvider.Observer() {
                @Override
                public void onControlsOffsetChanged(
                        int topOffset,
                        int topControlsMinHeightOffset,
                        int bottomOffset,
                        int bottomControlsMinHeightOffset,
                        boolean needsAnimate,
                        boolean isVisibilityForced) {
                    // Direct the call to BottomControlsLayer#onBrowserControlsOffsetUpdate.
                    if (BottomControlsStacker.isDispatchingYOffset()) return;

                    MiniPlayerMediator.this.onControlsOffsetChanged(bottomControlsMinHeightOffset);
                }

                @Override
                public void onBottomControlsHeightChanged(
                        int bottomControlContainerHeight, int bottomControlsMinHeight) {
                    MiniPlayerMediator.this.onBottomControlsHeightChanged();
                }
            };

    MiniPlayerMediator(BottomControlsStacker bottomControlsStacker) {
        mModel =
                new PropertyModel.Builder(Properties.ALL_KEYS)
                        .with(Properties.VISIBILITY, VisibilityState.GONE)
                        .with(Properties.ANDROID_VIEW_VISIBILITY, View.GONE)
                        .with(Properties.COMPOSITED_VIEW_VISIBLE, false)
                        .with(Properties.MEDIATOR, this)
                        .build();
        mBottomControlsStacker = bottomControlsStacker;
        mBottomControlsStacker.getBrowserControls().addObserver(mBrowserControlsStateObserver);
        mBottomControlsStacker.addLayer(this);
    }

    void setCoordinator(MiniPlayerCoordinator coordinator) {
        mCoordinator = coordinator;
    }

    /**
     * Set the yOffset that the mini player needs to move up. This is used when there are addition
     * browser controls below the mini player.
     *
     * @param yOffset The Y Offset in pixels. A negative yOffset will move the view upwards.
     */
    void setYOffset(int yOffset) {
        mModel.set(Properties.Y_OFFSET, yOffset);
    }

    void destroy() {
        getBrowserControls().removeObserver(mBrowserControlsStateObserver);
        mBottomControlsStacker.removeLayer(this);
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
        // Set player visibility from GONE to VISIBLE so that it has a height.
        mModel.set(Properties.ANDROID_VIEW_VISIBILITY, View.VISIBLE);
    }

    /**
     * Called by MiniPlayerLayout during onLayout() with its height minus shadow.
     *
     * @param heightPx Height of MiniPlayerLayout minus the top shadow height.
     */
    void onHeightKnown(int heightPx) {
        // (1.5) Grow bottom controls once player height has been measured.
        if (heightPx > 0 && heightPx != mLayoutHeightPx) {
            mLayoutHeightPx = heightPx;
            mBottomControlsLayerHeightPx = heightPx;
            mModel.set(Properties.HEIGHT, heightPx);
            growBottomControls();
        }
    }

    // (2) Finished growing, start fading in.
    private void onBottomControlsGrown() {
        // Step two: fade in if transition is animated, or jump to full opacity otherwise.
        mModel.set(Properties.CONTENTS_OPAQUE, true);
    }

    // (3) Done.
    /**
     * @param containerForNonErrorView not null if full opacity reached for non-error view
     */
    void onFullOpacityReached(@Nullable View containerForNonErrorView) {
        // show() is finished!
        onTransitionFinished(VisibilityState.VISIBLE);
        mCoordinator.onShown(containerForNonErrorView);
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
        mLayoutHeightPx = 0;
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
    private void onTransitionFinished(@VisibilityState int newState) {
        mModel.set(Properties.VISIBILITY, newState);
        mBottomControlsStacker.requestLayerUpdate(false);
    }

    void onBackgroundColorUpdated(@ColorInt int backgroundColorArgb) {
        mModel.set(Properties.BACKGROUND_COLOR_ARGB, backgroundColorArgb);
    }

    private void growBottomControls() {
        mBottomControlsStacker.notifyBackgroundColor(mModel.get(Properties.BACKGROUND_COLOR_ARGB));
        int minHeight = getBrowserControls().getBottomControlsMinHeight();
        setBottomControlsHeight(
                getBrowserControls().getBottomControlsHeight() + mLayoutHeightPx,
                mLayoutHeightPx + minHeight);
    }

    private void shrinkBottomControls() {
        // Hack: Bottom controls animation doesn't work if the new height is 0. Shrink
        // to 1 pixel instead in this case.
        // TODO(b/320750931): fix the underlying issue in browser controls code
        int minHeight = getBrowserControls().getBottomControlsMinHeight();
        setBottomControlsHeight(
                Math.max(getBrowserControls().getBottomControlsHeight() - mLayoutHeightPx, 1),
                Math.max(minHeight - mLayoutHeightPx, 0));
    }

    private void setBottomControlsHeight(int height, int minHeight) {
        mIsAnimationStarted = false;
        boolean animate = mModel.get(Properties.ANIMATE_VISIBILITY_CHANGES);
        mBottomControlsStacker.setBottomControlsHeight(height, minHeight, animate);
    }

    private BrowserControlsStateProvider getBrowserControls() {
        return mBottomControlsStacker.getBrowserControls();
    }

    private void onControlsOffsetChanged(int bottomControlsMinHeightOffset) {
        if (!mIsAnimationStarted) {
            mIsAnimationStarted = true;
        }
        // yOffset is constantly changing during an animation. The easier way to measure transition
        // stop is to check the relationship between bottomControlsMinHeightOffset and the browser
        // control's minHeight.
        boolean controlsFinishAnimating =
                getBrowserControls().getBottomControlsMinHeight() == bottomControlsMinHeightOffset;
        if (!controlsFinishAnimating) return;

        if (getVisibility() == VisibilityState.HIDING) {
            onBottomControlsShrunk();
        } else if (getVisibility() == VisibilityState.SHOWING) {
            onBottomControlsGrown();
        }
    }

    private void onBottomControlsHeightChanged() {
        // Hack: bottom controls don't animate on NTP, tab switcher, and potentially
        // other non tab pages.
        // As a result we show an empty bottom bar with no UI controls. This is trying
        // to prevent that from happening by forcing fading in player controls if bottom
        // controls are visible and no animation is running.
        if (getVisibility() == VisibilityState.SHOWING
                && getBrowserControls().getBottomControlsHeight() > 0) {
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        if (getVisibility() == VisibilityState.SHOWING
                                && !mIsAnimationStarted
                                && getBrowserControls().getBottomControlsHeight() > 0) {
                            onBottomControlsGrown();
                        }
                    },
                    200);
        }
    }

    private boolean isVisible() {
        // Consider layer visible even during its transition.
        return mModel.get(Properties.VISIBILITY) == VisibilityState.VISIBLE
                || mModel.get(Properties.VISIBILITY) == VisibilityState.SHOWING;
    }

    // Implements BottomControlsStacker.BottomControlsLayer

    @Override
    public int getType() {
        return LayerType.READ_ALOUD_PLAYER;
    }

    /** Get the height represent the layer in the bottom controls, without the yOffset. */
    @Override
    public int getHeight() {
        return mBottomControlsLayerHeightPx;
    }

    @Override
    public @LayerScrollBehavior int getScrollBehavior() {
        return LayerScrollBehavior.NEVER_SCROLL_OFF;
    }

    @Override
    public @LayerVisibility int getLayerVisibility() {
        @VisibilityState int currentVisibility = getVisibility();
        if (currentVisibility == VisibilityState.VISIBLE) {
            return LayerVisibility.VISIBLE;
        } else if (currentVisibility == VisibilityState.SHOWING) {
            return LayerVisibility.SHOWING;
        } else if (currentVisibility == VisibilityState.HIDING) {
            return LayerVisibility.HIDING;
        } else {
            return LayerVisibility.HIDDEN;
        }
    }

    @Override
    public void onBrowserControlsOffsetUpdate(int layerYOffset) {
        assert BottomControlsStacker.isDispatchingYOffset();

        // yOffset for the mini player is a negative number if it has to move up. This value *can*
        // be positive when we are going through an animation; in which case the player view should
        // stay invisible.
        setYOffset(Math.min(0, layerYOffset));
        onControlsOffsetChanged(getBrowserControls().getBottomControlsMinHeightOffset());
    }
}

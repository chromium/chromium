// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

import android.animation.Animator;
import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;

/** Base abstract class for animating the Overlay Panel. */
public abstract class OverlayPanelAnimation extends OverlayPanelBase {
    /**
     * The base duration of animations in milliseconds. This value is based on
     * the Kennedy specification for slow animations.
     */
    public static final long BASE_ANIMATION_DURATION_MS = 218;

    /** The maximum animation duration in milliseconds. */
    public static final long MAXIMUM_ANIMATION_DURATION_MS = 350;

    /** The minimum animation duration in milliseconds. */
    private static final long MINIMUM_ANIMATION_DURATION_MS = 7 * 1000 / 60;

    /** Average animation velocity in dps per second. */
    private static final float INITIAL_ANIMATION_VELOCITY_DP_PER_SECOND = 1750f;

    /** The PanelState to which the Panel is being animated. */
    private @Nullable @PanelState Integer mAnimatingState;

    /** The StateChangeReason for which the Panel is being animated. */
    private @StateChangeReason int mAnimatingStateReason;

    /** The animator responsible for moving the sheet up and down. */
    private CompositorAnimator mHeightAnimator;

    /** The {@link LayoutUpdateHost} used to request a new frame to be updated and rendered. */
    private final LayoutUpdateHost mUpdateHost;

    // ============================================================================================
    // Constructor
    // ============================================================================================

    /**
     * @param context The current Android {@link Context}.
     * @param updateHost The {@link LayoutUpdateHost} used to request updates in the Layout.
     * @param toolbarHeightDp The height of the toolbar in dp.
     */
    public OverlayPanelAnimation(
            Context context, LayoutUpdateHost updateHost, float toolbarHeightDp) {
        super(context, toolbarHeightDp);
        mUpdateHost = updateHost;
    }

    // ============================================================================================
    // Animation API
    // ============================================================================================

    /**
     * @return The handler responsible for running compositor animations.
     */
    public CompositorAnimationHandler getAnimationHandler() {
        return mUpdateHost.getAnimationHandler();
    }

    /**
     * Animates the Overlay Panel to its maximized state.
     *
     * @param reason The reason for the change of panel state.
     */
    protected void maximizePanel(@StateChangeReason int reason) {
        animatePanelToState(PanelState.MAXIMIZED, reason);
    }

    /**
     * Animates the Overlay Panel to its intermediary state.
     *
     * @param reason The reason for the change of panel state.
     */
    protected void expandPanel(@StateChangeReason int reason) {
        animatePanelToState(PanelState.EXPANDED, reason);
    }

    /**
     * Animates the Overlay Panel to its peeked state.
     *
     * @param reason The reason for the change of panel state.
     */
    protected void peekPanel(@StateChangeReason int reason) {
        updateBasePageTargetY();

        // TODO(pedrosimonetti): Implement custom animation with the following values.
        // int SEARCH_BAR_ANIMATION_DURATION_MS = 218;
        // float SEARCH_BAR_SLIDE_OFFSET_DP = 40;
        // float mSearchBarHeightDp;
        // setTranslationY(mIsShowingFirstRunFlow
        //      ? mSearchBarHeightDp : SEARCH_BAR_SLIDE_OFFSET_DP);
        // setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
        animatePanelToState(PanelState.PEEKED, reason);
    }

    @Override
    protected void closePanel(@StateChangeReason int reason, boolean animate) {
        if (animate) {
            // Only animates the closing action if not doing that already.
            if (mAnimatingState != PanelState.CLOSED) {
                animatePanelToState(PanelState.CLOSED, reason);
            }
        } else {
            resizePanelToState(PanelState.CLOSED, reason);
        }
    }

    @Override
    protected void handleSizeChanged(float width, float height, float previousWidth) {
        if (!isShowing()) return;

        boolean wasFullWidthSizePanel = doesMatchFullWidthCriteria(previousWidth);
        boolean isFullWidthSizePanel = isFullWidthSizePanel();
        // We support resize from any full width to full width, or from narrow width to narrow width
        // when the width does not change (as when the keyboard is shown/hidden).
        boolean isPanelResizeSupported =
                (isFullWidthSizePanel && wasFullWidthSizePanel)
                        || (!isFullWidthSizePanel
                                && !wasFullWidthSizePanel
                                && width == previousWidth);

        // TODO(pedrosimonetti): See crbug.com/568351.
        // We can't keep the panel opened after a viewport size change when the panel's
        // ContentView needs to be resized to a non-default size. The panel provides
        // different desired MeasureSpecs when full-width vs narrow-width
        // (See {@link OverlayPanel#createNewOverlayPanelContentInternal()}).
        // When the activity is resized, ContentViewClient asks for the MeasureSpecs
        // before the panel is notified of the size change, resulting in the panel's
        // ContentView being laid out incorrectly.
        if (isPanelResizeSupported) {
            if (mAnimatingState == null || mAnimatingState != PanelState.UNDEFINED) {
                // If the size changes when an animation is happening, then we need to restart
                // the animation, because the size of the Panel might have changed as well.
                animatePanelToState(mAnimatingState, mAnimatingStateReason);
            } else {
                updatePanelForSizeChange();
            }
        } else {
            // TODO(pedrosimonetti): Find solution that does not require async handling.
            // NOTE(pedrosimonetti): Should close the Panel asynchronously because
            // we might be in the middle of laying out the CompositorViewHolder
            // View. See {@link CompositorViewHolder#onLayout()}. Closing the Panel
            // has the effect of destroying the Views used by the Panel (which are
            // children of the CompositorViewHolder), and if we do that synchronously
            // it will cause a crash in {@link FrameLayout#layoutChildren()}.
            mContainerView
                    .getHandler()
                    .post(
                            new Runnable() {
                                @Override
                                public void run() {
                                    closePanel(StateChangeReason.UNKNOWN, false);
                                }
                            });
        }
    }

    @Override
    public void hidePanel(@StateChangeReason int reason) {
        if (getPanelState() == PanelState.PEEKED) {
            mPanelHidden = true;
            animatePanelToState(PanelState.CLOSED, reason);
        }
    }

    @Override
    public void showPanel(@StateChangeReason int reason) {
        if (mPanelHidden) {
            animatePanelToState(PanelState.PEEKED, reason);
            mPanelHidden = false;
        }
    }

    /** Updates the Panel so it preserves its state when the size changes. */
    protected void updatePanelForSizeChange() {
        resizePanelToState(getPanelState(), StateChangeReason.UNKNOWN);
    }

    /**
     * Animates the Overlay Panel to a given |state| with a default duration.
     *
     * @param state The state to animate to.
     * @param reason The reason for the change of panel state.
     */
    private void animatePanelToState(
            @Nullable @PanelState Integer state, @StateChangeReason int reason) {
        animatePanelToState(state, reason, BASE_ANIMATION_DURATION_MS);
    }

    /**
     * Animates the Overlay Panel to a given |state| with a custom |duration|.
     *
     * @param state The state to animate to.
     * @param reason The reason for the change of panel state.
     * @param duration The animation duration in milliseconds.
     */
    protected void animatePanelToState(
            @Nullable @PanelState Integer state, @StateChangeReason int reason, long duration) {
        mAnimatingState = state;
        mAnimatingStateReason = reason;

        final float height = getPanelHeightFromState(state);
        animatePanelTo(height, duration);
    }

    /**
     * Resizes the Overlay Panel to a given |state|.
     *
     * @param state The state to resize to.
     * @param reason The reason for the change of panel state.
     */
    protected void resizePanelToState(@PanelState int state, @StateChangeReason int reason) {
        cancelHeightAnimation();

        final float height = getPanelHeightFromState(state);
        setPanelHeight(height);
        setPanelState(state, reason);
        requestUpdate();
    }

    // ============================================================================================
    // Animation Helpers
    // ============================================================================================

    /** Animates the Panel to its nearest state. */
    protected void animateToNearestState() {
        // Calculate the nearest state from the current position, and then calculate the duration
        // of the animation that will start with a desired initial velocity and move the desired
        // amount of dps (displacement).
        final @PanelState int nearestState = findNearestPanelStateFromHeight(getHeight(), 0.0f);
        final float displacement = getPanelHeightFromState(nearestState) - getHeight();
        final long duration =
                calculateAnimationDuration(INITIAL_ANIMATION_VELOCITY_DP_PER_SECOND, displacement);

        animatePanelToState(nearestState, StateChangeReason.SWIPE, duration);
    }

    /**
     * Animates the Panel to its projected state, given a particular vertical |velocity|.
     *
     * @param velocity The velocity of the gesture in dps per second.
     */
    protected void animateToProjectedState(float velocity) {
        @PanelState int projectedState = getProjectedState(velocity);

        final float displacement = getPanelHeightFromState(projectedState) - getHeight();
        final long duration = calculateAnimationDuration(velocity, displacement);

        animatePanelToState(projectedState, StateChangeReason.FLING, duration);
    }

    /**
     * @param velocity The given velocity.
     * @return The projected state the Panel will be if the given velocity is applied.
     */
    protected @PanelState int getProjectedState(float velocity) {
        final float kickY =
                calculateAnimationDisplacement(velocity, (float) BASE_ANIMATION_DURATION_MS);
        final float projectedHeight = getHeight() - kickY;

        // Calculate the projected state the Panel will be at the end of the fling movement and the
        // duration of the animation given the current velocity and the projected displacement.
        @PanelState int projectedState = findNearestPanelStateFromHeight(projectedHeight, velocity);

        return projectedState;
    }

    /**
     * Calculates the animation displacement given the |initialVelocity| and a
     * desired |duration|.
     *
     * @param initialVelocity The initial velocity of the animation in dps per second.
     * @param duration The desired duration of the animation in milliseconds.
     * @return The animation displacement in dps.
     */
    protected float calculateAnimationDisplacement(float initialVelocity, float duration) {
        // NOTE(pedrosimonetti): This formula assumes the deceleration curve is
        // quadratic (t^2), hence the displacement formula should be:
        // displacement = initialVelocity * duration / 2
        //
        // We are also converting the duration from milliseconds to seconds,
        // which explains why we are dividing by 2000 (2 * 1000) instead of 2.
        return initialVelocity * duration / 2000;
    }

    /**
     * Calculates the animation duration given the |initialVelocity| and a
     * desired |displacement|.
     *
     * @param initialVelocity The initial velocity of the animation in dps per second.
     * @param displacement The displacement of the animation in dps.
     * @return The animation duration in milliseconds.
     */
    private long calculateAnimationDuration(float initialVelocity, float displacement) {
        // NOTE(pedrosimonetti): This formula assumes the deceleration curve is
        // quadratic (t^2), hence the duration formula should be:
        // duration = 2 * displacement / initialVelocity
        //
        // We are also converting the duration from seconds to milliseconds,
        // which explains why we are multiplying by 2000 (2 * 1000) instead of 2.
        return MathUtils.clamp(
                Math.round(Math.abs(2000 * displacement / initialVelocity)),
                MINIMUM_ANIMATION_DURATION_MS,
                MAXIMUM_ANIMATION_DURATION_MS);
    }

    /** Cancels any height animation in progress. */
    protected void cancelHeightAnimation() {
        if (mHeightAnimator != null) mHeightAnimator.cancel();
    }

    /**
     * Ends any height animation in progress. This differs from {@link #cancelHeightAnimation()} in
     * that it will snap to it's target state rather than simply stopping the animation.
     */
    protected void endHeightAnimation() {
        if (mHeightAnimator != null) mHeightAnimator.end();
    }

    // ============================================================================================
    // Layout Integration
    // ============================================================================================

    /** Requests a new frame to be updated and rendered. */
    protected void requestUpdate() {
        // NOTE(pedrosimonetti): mUpdateHost will be null in the ContextualSearchEventFilterTest,
        // so we always need to check if it's null before calling requestUpdate.
        if (mUpdateHost != null) {
            mUpdateHost.requestUpdate();
        }
    }

    // ============================================================================================
    // Animation Framework
    // ============================================================================================

    /**
     * Animates the Overlay Panel to a given |height| with a custom |duration|.
     *
     * @param height The height to animate to.
     * @param duration The animation duration in milliseconds.
     */
    protected void animatePanelTo(float height, long duration) {
        if (duration <= 0) return;

        if (mHeightAnimator != null) mHeightAnimator.cancel();

        mHeightAnimator =
                CompositorAnimator.ofFloat(
                        getAnimationHandler(), getHeight(), height, duration, null);
        mHeightAnimator.addUpdateListener(animator -> setPanelHeight(animator.getAnimatedValue()));
        mHeightAnimator.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onEnd(Animator animation) {
                        onHeightAnimationFinished();
                    }
                });
        mHeightAnimator.start();
    }

    /**
     * Called when layout-specific actions are needed after the animation finishes.
     * This method should only be called when the animation ends normally and not when it is
     * canceled.
     */
    protected void onHeightAnimationFinished() {
        // If animating to a particular PanelState, and after completing resizing the Panel to its
        // desired state, then the Panel's state should be updated.
        if (mAnimatingState != null && mAnimatingState != PanelState.UNDEFINED) {
            setPanelState(mAnimatingState, mAnimatingStateReason);
        }

        mAnimatingState = PanelState.UNDEFINED;
        mAnimatingStateReason = StateChangeReason.UNKNOWN;
    }

    @VisibleForTesting
    public boolean isHeightAnimationRunning() {
        return mHeightAnimator != null && mHeightAnimator.isRunning();
    }
}

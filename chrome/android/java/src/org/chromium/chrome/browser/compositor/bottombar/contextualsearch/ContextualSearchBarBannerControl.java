// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimator;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelAnimation;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelInflater;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Controls the Search Bar Banner.
 */
public class ContextualSearchBarBannerControl extends OverlayPanelInflater {
    /**
     * The initial width of the ripple for the appearance animation, in dps.
     */
    private static final float RIPPLE_MINIMUM_WIDTH_DP = 56.f;

    /**
     * Whether the Bar Banner is visible.
     */
    private boolean mIsVisible;

    /**
     * Whether the Bar Banner is in the process of hiding.
     */
    private boolean mIsHiding;

    /**
     * The height of the Bar Banner, in pixels.
     */
    private float mHeightPx;

    /**
     * The width of the Ripple resource in pixels.
     */
    private float mRippleWidthPx;

    /**
     * The opacity of the Ripple resource.
     */
    private float mRippleOpacity;

    /**
     * The opacity of the Text View dynamic resource.
     */
    private float mTextOpacity;

    /**
     * The precomputed padding of the Bar Banner, in pixels.
     */
    private final float mPaddingPx;

    /**
     * The padded height of the Bar Banner in pixels. Set to zero initially, calculated on the first
     * call.
     */
    private float mPaddedHeightPx;

    /**
     * The precomputed minimum width of the Ripple resource in pixels.
     */
    private final float mRippleMinimumWidthPx;

    /**
     * The precomputed maximum width of the Ripple resource in pixels.
     */
    private float mRippleMaximumWidthPx;

    /**
     * @param panel             The panel.
     * @param context           The Android Context used to inflate the View.
     * @param container         The container View used to inflate the View.
     * @param resourceLoader    The resource loader that will handle the snapshot capturing.
     */
    public ContextualSearchBarBannerControl(OverlayPanel panel, Context context,
            ViewGroup container, DynamicResourceLoader resourceLoader) {
        super(panel, R.layout.contextual_search_bar_banner_text_view,
                R.id.contextual_search_bar_banner_text_view, context, container, resourceLoader);

        final float dpToPx = context.getResources().getDisplayMetrics().density;

        mPaddingPx = context.getResources().getDimensionPixelOffset(
                R.dimen.contextual_search_bar_banner_padding);

        mRippleMinimumWidthPx = RIPPLE_MINIMUM_WIDTH_DP * dpToPx;
        mRippleMaximumWidthPx = panel.getMaximumWidthPx();
    }

    /**
     * Shows the Bar Banner. This includes inflating the View and setting it to its initial state.
     * This also means a new cc::Layer will be created and added to the tree.
     */
    void show() {
        if (mIsVisible) return;

        mIsVisible = true;
        mHeightPx = Math.round(getPaddedHeightPx());

        invalidate();
    }

    /**
     * Hides the Bar Banner, returning the Control to its initial uninitialized state. In this
     * state, now View will be created and no Layer added to the tree (or removed if present).
     */
    void hide() {
        if (!mIsVisible) return;

        if (mHeightPx == 0.f) {
            mIsVisible = false;
        } else {
            animateDisappearance();
        }
    }

    /**
     * @return The height of the Bar Banner when the Panel is the peeked state.
     */
    float getHeightPeekingPx() {
        return (!isVisible() || mIsHiding) ? 0.f : getPaddedHeightPx();
    }

    /** Calculates the padded height of the bar banner if it has not been calculated before.
     * @return The padded height of the Bar Banner.
     */
    private float getPaddedHeightPx() {
        if (mPaddedHeightPx == 0.f) {
            // Calculate the padded height based on the measured height of the TextView.
            inflate();
            layout();
            mPaddedHeightPx = getMeasuredHeight() + (2 * mPaddingPx);
        }
        return mPaddedHeightPx;
    }

    // ============================================================================================
    // Custom Behaviors
    // ============================================================================================

    void onResized(ContextualSearchPanel panel) {
        mRippleMaximumWidthPx = panel.getMaximumWidthPx();

        // In the rare condition that size is changed mid-animation, e.g. the device orientation is
        // changed, mRippleWidthPx will be updated by the CompositorAnimator and will grow to the
        // new mRippleMaximumWidthPx by the end of animation.
        mRippleWidthPx = mRippleMaximumWidthPx;
    }

    // ============================================================================================
    // Public API
    // ============================================================================================

    /**
     * @return Whether the Bar Banner is visible.
     */
    public boolean isVisible() {
        return mIsVisible;
    }

    /**
     * @return The Bar Banner height in pixels.
     */
    public float getHeightPx() {
        return mHeightPx;
    }

    /**
     * @return The Bar Banner padding in pixels.
     */
    public float getPaddingPx() {
        return mPaddingPx;
    }

    /**
     * @return The width of the Ripple resource in pixels.
     */
    public float getRippleWidthPx() {
        return mRippleWidthPx;
    }

    /**
     * @return The opacity of the Ripple resource.
     */
    public float getRippleOpacity() {
        return mRippleOpacity;
    }

    /**
     * @return The opacity of the Text View dynamic resource.
     */
    public float getTextOpacity() {
        return mTextOpacity;
    }

    // ============================================================================================
    // Panel Animation
    // ============================================================================================

    /**
     * Interpolates the UI from states Closed to Peeked.
     *
     * @param percentage The completion percentage.
     */
    public void onUpdateFromCloseToPeek(float percentage) {
        if (!isVisible() || mIsHiding) return;

        mHeightPx = Math.round(getPaddedHeightPx());
    }

    /**
     * Interpolates the UI from states Peeked to Expanded.
     *
     * @param percentage The completion percentage.
     */
    public void onUpdateFromPeekToExpand(float percentage) {
        if (!isVisible() || mIsHiding) return;

        mHeightPx = Math.round(MathUtils.interpolate(getPaddedHeightPx(), 0.f, percentage));
        mTextOpacity = MathUtils.interpolate(1.f, 0.f, percentage);
    }

    /**
     * Interpolates the UI from states Expanded to Maximized.
     *
     * @param percentage The completion percentage.
     */
    public void onUpdateFromExpandToMaximize(float percentage) {
        if (!isVisible() || mIsHiding) return;

        mHeightPx = 0.f;
        mTextOpacity = 0.f;
    }

    // ============================================================================================
    // Bar Banner Appearance Animation
    // ============================================================================================

    /**
     * Animates the Bar Banner appearance.
     */
    public void animateAppearance() {
        CompositorAnimator appearance =
                CompositorAnimator.ofFloat(mOverlayPanel.getAnimationHandler(), 0.f, 1.f,
                        OverlayPanelAnimation.BASE_ANIMATION_DURATION_MS, null);
        appearance.addUpdateListener(animator -> {
            float percentage = animator.getAnimatedFraction();
            mRippleWidthPx = Math.round(MathUtils.interpolate(
                    mRippleMinimumWidthPx, mRippleMaximumWidthPx, percentage));

            mRippleOpacity = MathUtils.interpolate(0.f, 1.f, percentage);

            float textOpacityDelay = 0.5f;
            float textOpacityPercentage =
                    Math.max(0.f, percentage - textOpacityDelay) / (1.f - textOpacityDelay);
            mTextOpacity = MathUtils.interpolate(0.f, 1.f, textOpacityPercentage);
        });
        appearance.start();
    }

    /**
     * Animates the Bar Banner disappearance.
     */
    private void animateDisappearance() {
        mIsHiding = true;
        CompositorAnimator disappearance =
                CompositorAnimator.ofFloat(mOverlayPanel.getAnimationHandler(), 1.f, 0.f,
                        OverlayPanelAnimation.BASE_ANIMATION_DURATION_MS, null);
        disappearance.addUpdateListener(animator -> {
            if (isVisible()) {
                float percentage = animator.getAnimatedFraction();
                mHeightPx = MathUtils.interpolate(getPaddedHeightPx(), 0.f, percentage);
            }
        });
        disappearance.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mHeightPx = 0.f;
                mIsHiding = false;
                hide();
            }
        });
        disappearance.start();
    }
}
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.widget.FrameLayout.LayoutParams;
import android.widget.ImageView;

import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

/**
 * An animating ImageView that is drawn on top of the progress bar. This will animate over the
 * current length of the progress bar only if the progress bar is static for some amount of time.
 */
public class ToolbarProgressBarAnimatingView extends ImageView {
    /** The drawable inside this ImageView. */
    private final ColorDrawable mAnimationDrawable;

    /** The fraction of the total time that the slow animation should take. */
    private static final float SLOW_ANIMATION_FRACTION = 0.60f;

    /** The fraction of the total time that the fast animation delay should take. */
    private static final float FAST_ANIMATION_DELAY = 0.02f;

    /** The fraction of the total time that the fast animation should take. */
    private static final float FAST_ANIMATION_FRACTION = 0.38f;

    /** The time between animation sequences. */
    private static final int ANIMATION_DELAY_MS = 1000;

    /** The width of the animating bar relative to the current width of the progress bar. */
    private static final float ANIMATING_BAR_SCALE = 0.3f;

    /**
     * The width of the animating bar relative to the current width of the progress bar for the
     * first half of the slow animation.
     */
    private static final float SMALL_ANIMATING_BAR_SCALE = 0.1f;

    /** The fraction of overall completion that the small animating bar should be expanded at. */
    private static final float SMALL_BAR_EXPANSION_COMPLETE = 0.6f;

    /** The maximum size of the animating view. */
    private static final float ANIMATING_VIEW_MAX_WIDTH_DP = 400;

    /** Interpolator for enter and exit animation. */
    private final BakedBezierInterpolator mBezier = BakedBezierInterpolator.FADE_OUT_CURVE;

    /** The current width of the progress bar. */
    private float mProgressWidth;

    /** The set of individual animators that constitute the whole animation sequence. */
    private final AnimatorSet mAnimatorSet;

    /** The animator controlling the fast animation. */
    private final ValueAnimator mFastAnimation;

    /** The animator controlling the slow animation. */
    private final ValueAnimator mSlowAnimation;

    /** Track if the animation has been canceled. */
    private boolean mIsCanceled;

    /** If the layout is RTL. */
    private boolean mIsRtl;

    /** The update listener for the animation. */
    private ProgressBarUpdateListener mListener;

    /** The last fraction of the animation that was drawn. */
    private float mLastAnimatedFraction;

    /** The last animation that received an update. */
    private ValueAnimator mLastUpdatedAnimation;

    /** The ratio of px to dp. */
    private float mDpToPx;

    /**
     * An animation update listener that moves an ImageView across the progress bar.
     */
    private class ProgressBarUpdateListener implements AnimatorUpdateListener {
        @Override
        public void onAnimationUpdate(ValueAnimator animation) {
            mLastUpdatedAnimation = animation;
            mLastAnimatedFraction = animation.getAnimatedFraction();
            updateAnimation(mLastUpdatedAnimation, mLastAnimatedFraction);
        }
    }

    /**
     * @param context The Context for this view.
     * @param height The LayoutParams for this view.
     */
    public ToolbarProgressBarAnimatingView(Context context, LayoutParams layoutParams) {
        super(context);
        setLayoutParams(layoutParams);
        mIsCanceled = true;
        mIsRtl = LocalizationUtils.isLayoutRtl();
        mDpToPx = getResources().getDisplayMetrics().density;

        mAnimationDrawable = new ColorDrawable(Color.WHITE);

        setImageDrawable(mAnimationDrawable);

        mListener = new ProgressBarUpdateListener();
        mAnimatorSet = new AnimatorSet();

        mSlowAnimation = new ValueAnimator();
        mSlowAnimation.setFloatValues(0.0f, 1.0f);
        mSlowAnimation.addUpdateListener(mListener);

        mFastAnimation = new ValueAnimator();
        mFastAnimation.setFloatValues(0.0f, 1.0f);
        mFastAnimation.addUpdateListener(mListener);

        updateAnimationDuration();

        mAnimatorSet.playSequentially(mSlowAnimation, mFastAnimation);

        AnimatorListener listener = new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator a) {
                // Replay the animation if it has not been canceled.
                if (mIsCanceled) return;
                // Repeats of the animation should have a start delay.
                mAnimatorSet.setStartDelay(ANIMATION_DELAY_MS);
                updateAnimationDuration();
                // Only restart the animation if the last animation is ending.
                if (a == mFastAnimation) mAnimatorSet.start();
            }
        };

        mSlowAnimation.addListener(listener);
        mFastAnimation.addListener(listener);
    }

    /**
     * Update the duration of the animation based on the width of the progress bar.
     */
    private void updateAnimationDuration() {
        // If progress is <= 0, the duration is also 0.
        if (mProgressWidth <= 0) return;

        // Total duration: logE(progress_dp) * 200 * 1.3
        long totalDuration = (long) (Math.log(mProgressWidth / mDpToPx) / Math.log(Math.E)) * 260;
        if (totalDuration <= 0) return;

        mSlowAnimation.setDuration((long) (totalDuration * SLOW_ANIMATION_FRACTION));
        mFastAnimation.setStartDelay((long) (totalDuration * FAST_ANIMATION_DELAY));
        mFastAnimation.setDuration((long) (totalDuration * FAST_ANIMATION_FRACTION));
    }

    /**
     * Start the animation if it hasn't been already.
     */
    public void startAnimation() {
        mIsCanceled = false;
        if (!mAnimatorSet.isStarted()) {
            updateAnimationDuration();
            // Set the initial start delay to 0ms so it starts immediately.
            mAnimatorSet.setStartDelay(0);

            // Reset position.
            setScaleX(0.0f);
            setTranslationX(0.0f);
            mAnimatorSet.start();

            // Fade in to look nice on sites that trigger many loads that end quickly.
            animate().alpha(1.0f).setDuration(500).setInterpolator(
                    BakedBezierInterpolator.FADE_IN_CURVE);
        }
    }

    /**
     * Update the animating view.
     * @param animator The current running animator.
     * @param animatedFraction The current fraction of completion for the animation.
     */
    private void updateAnimation(ValueAnimator animator, float animatedFraction) {
        if (mIsCanceled) return;
        float bezierProgress = mBezier.getInterpolation(animatedFraction);

        // Left and right bound change based on if the layout is RTL.
        float leftBound = mIsRtl ? -mProgressWidth : 0.0f;
        float rightBound = mIsRtl ? 0.0f : mProgressWidth;

        float barScale = ANIMATING_BAR_SCALE;

        // If the current animation is the slow animation, the bar slowly expands from 20% of the
        // progress bar width to 30%.
        if (animator == mSlowAnimation && animatedFraction <= SMALL_BAR_EXPANSION_COMPLETE) {
            float sizeDiff = ANIMATING_BAR_SCALE - SMALL_ANIMATING_BAR_SCALE;

            // Since the bar will only expand for the first 60% of the animation, multiply the
            // animated fraction by 1/0.6 to get the fraction completed.
            float completeFraction = (animatedFraction / SMALL_BAR_EXPANSION_COMPLETE);

            barScale = SMALL_ANIMATING_BAR_SCALE + sizeDiff * completeFraction;
        }

        // Include the width of the animating bar in this computation so it comes from
        // off-screen.
        float animatingWidth =
                Math.min(ANIMATING_VIEW_MAX_WIDTH_DP * mDpToPx, mProgressWidth * barScale);

        float animatorCenter =
                ((mProgressWidth + animatingWidth) * bezierProgress) - animatingWidth / 2.0f;
        if (mIsRtl) animatorCenter *= -1.0f;

        // The left and right x-coordinate of the animating view.
        float animatorRight = animatorCenter + (animatingWidth / 2.0f);
        float animatorLeft = animatorCenter - (animatingWidth / 2.0f);

        // "Clip" the view so it doesn't go past where the progress bar starts or ends.
        if (animatorRight > rightBound) {
            animatingWidth -= Math.abs(animatorRight - rightBound);
            animatorCenter -= Math.abs(animatorRight - rightBound) / 2.0f;
        } else if (animatorLeft < leftBound) {
            animatingWidth -= Math.abs(animatorLeft - leftBound);
            animatorCenter += Math.abs(animatorLeft - leftBound) / 2.0f;
        }

        setScaleX(animatingWidth);
        setTranslationX(animatorCenter);
    }

    /**
     * @return True if the animation is running.
     */
    public boolean isRunning() {
        return !mIsCanceled;
    }

    /**
     * Cancel the animation.
     */
    public void cancelAnimation() {
        mIsCanceled = true;
        mAnimatorSet.cancel();
        // Reset position and alpha.
        setScaleX(0.0f);
        setTranslationX(0.0f);
        animate().cancel();
        setAlpha(0.0f);
        mLastAnimatedFraction = 0.0f;
        mProgressWidth = 0;
    }

    /**
     * Update info about the progress bar holding this animating block.
     * @param progressWidth The width of the contaiing progress bar.
     */
    public void update(float progressWidth) {
        // Since the progress bar can become visible before current progress is sent, the width
        // needs to be updated even if the progess bar isn't showing. The result of not having
        // this is most noticable if you rotate the device on a slow page.
        mProgressWidth = progressWidth;
        updateAnimation(mLastUpdatedAnimation, mLastAnimatedFraction);
    }

    /**
     * @param color The Android color that the animating bar should be.
     */
    public void setColor(int color) {
        mAnimationDrawable.setColor(color);
    }
}

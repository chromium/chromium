// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.HUB_LAYOUT_FADE_DURATION_MS;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.view.View;

import org.chromium.base.supplier.SyncOneshotSupplier;
import org.chromium.base.supplier.SyncOneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.interpolators.Interpolators;

import java.util.ArrayList;
import java.util.List;
import java.util.function.DoubleConsumer;

/**
 * {@link HubLayoutAnimatorProvider} for fade in/out animation for the tab list items.
 *
 * <p>Maybe GridLayoutAnimationController
 * (https://developer.android.com/reference/android/view/animation/GridLayoutAnimationController)
 * can be used instead of this manual animation implementation.
 */
@NullMarked
public class TabListHubLayoutAnimatorProvider implements HubLayoutAnimatorProvider {
    private static final long VIEW_ANIMATION_DELAY_MS = 33;
    private final @HubLayoutAnimationType int mAnimationType;
    private final HubContainerView mHubContainerView;
    private final SyncOneshotSupplierImpl<HubLayoutAnimator> mAnimatorSupplier;
    private final SyncOneshotSupplier<List<View>> mAnimationDataSupplier;
    private final long mDurationMs;
    private final DoubleConsumer mOnAlphaChange;
    private final float mInitialAlpha;
    private final float mFinalAlpha;
    private final float mInitialScaleRatio;
    private final float mFinalScaleRatio;

    /**
     * Creates a fade in/out tab list items animation.
     *
     * @param animationType The {@link HubLayoutAnimationType} of this animation.
     * @param hubContainerView The {@link HubContainerView} to animate.
     * @param animationDataSupplier The supplier of List<View> to use for the animation.
     * @param durationMs The duration in milliseconds of the animation.
     * @param onAlphaChange Observer to notify when alpha changes during animations.
     */
    public TabListHubLayoutAnimatorProvider(
            @HubLayoutAnimationType int animationType,
            HubContainerView hubContainerView,
            SyncOneshotSupplier<List<View>> animationDataSupplier,
            long durationMs,
            DoubleConsumer onAlphaChange) {
        assert animationType == HubLayoutAnimationType.FADE_IN
                        || animationType == HubLayoutAnimationType.FADE_OUT
                : "Invalid TabList fade in/out HubLayoutAnimationType: " + animationType;
        mAnimationType = animationType;
        mInitialAlpha = mAnimationType == HubLayoutAnimationType.FADE_IN ? 0f : 1f;
        mFinalAlpha = mAnimationType == HubLayoutAnimationType.FADE_IN ? 1f : 0f;
        mInitialScaleRatio = mAnimationType == HubLayoutAnimationType.FADE_IN ? 0.95f : 1f;
        mFinalScaleRatio = mAnimationType == HubLayoutAnimationType.FADE_IN ? 1f : 0.95f;
        mHubContainerView = hubContainerView;
        mAnimationDataSupplier = animationDataSupplier;
        mAnimatorSupplier = new SyncOneshotSupplierImpl<>();
        mDurationMs = durationMs;
        mOnAlphaChange = onAlphaChange;
        mAnimationDataSupplier.onAvailable(this::onAnimationDataAvailable);
    }

    @Override
    public @HubLayoutAnimationType int getPlannedAnimationType() {
        return mAnimationType;
    }

    @Override
    public SyncOneshotSupplier<HubLayoutAnimator> getAnimatorSupplier() {
        return mAnimatorSupplier;
    }

    @Override
    public void supplyAnimatorNow() {
        if (mAnimatorSupplier.get() != null) return;
        supplyFallbackAnimator();
    }

    private void maybeSupplyAnimation() {
        var animator = mAnimatorSupplier.get();
        List<View> views = mAnimationDataSupplier.get();
        if (animator != null || views == null) return;

        AnimatorSet animatorSet = buildAnimatorSet(views);
        HubLayoutAnimationListener listener =
                new HubLayoutAnimationListener() {
                    @Override
                    public void beforeStart() {
                        resetState(true);
                    }

                    @Override
                    public void onEnd(boolean wasForcedToFinish) {
                        resetState(false);
                    }
                };

        mAnimatorSupplier.set(new HubLayoutAnimator(mAnimationType, animatorSet, listener));
    }

    private void supplyFallbackAnimator() {
        if (mAnimationType == HubLayoutAnimationType.FADE_IN) {
            mAnimatorSupplier.set(
                    FadeHubLayoutAnimationFactory.createFadeInAnimator(
                            mHubContainerView, HUB_LAYOUT_FADE_DURATION_MS, mOnAlphaChange));
        } else if (mAnimationType == HubLayoutAnimationType.FADE_OUT) {
            mAnimatorSupplier.set(
                    FadeHubLayoutAnimationFactory.createFadeOutAnimator(
                            mHubContainerView, HUB_LAYOUT_FADE_DURATION_MS, mOnAlphaChange));
        } else {
            assert false : "Not reached.";
            // If in production we somehow get here just skip animating entirely.
            mAnimatorSupplier.set(
                    new HubLayoutAnimator(HubLayoutAnimationType.NONE, new AnimatorSet(), null));
        }
    }

    private void onAnimationDataAvailable(List<View> animationData) {
        maybeSupplyAnimation();
    }

    /** Set views' initial or final state. */
    private void resetState(boolean intialState) {
        float alpha = intialState ? mInitialAlpha : mFinalAlpha;
        float scale = intialState ? mInitialScaleRatio : mFinalScaleRatio;

        View toolbarView = mHubContainerView.findViewById(R.id.hub_toolbar);
        if (toolbarView != null) {
            toolbarView.setAlpha(alpha);
        }
        mOnAlphaChange.accept(alpha);
        mHubContainerView.setVisibility(View.VISIBLE); // mHubContainerView is always visible.

        List<View> views = mAnimationDataSupplier.get();
        if (views == null || views.isEmpty()) return;

        for (View view : views) {
            view.setAlpha(alpha);
            view.setScaleX(scale);
            view.setScaleY(scale);
        }
    }

    private AnimatorSet buildAnimatorSet(List<View> views) {
        AnimatorSet animatorSet = new AnimatorSet();
        List<Animator> animators = new ArrayList<>();

        for (int index = 0; index < views.size(); index++) {
            Animator fadeAnimator = getViewFadeAnimator(views.get(index), index);
            animators.add(fadeAnimator);

            Animator scaleXAnimator = getViewScaleAnimator(views.get(index), index, true);
            animators.add(scaleXAnimator);

            Animator scaleYAnimator = getViewScaleAnimator(views.get(index), index, false);
            animators.add(scaleYAnimator);
        }

        // Offset of the last view animation + view animation duration.
        long totalAnimationDuration = (views.size() - 1) * VIEW_ANIMATION_DELAY_MS + mDurationMs;
        Animator toolbarAnimator = getToolbarFadeAnimator(totalAnimationDuration);
        if (toolbarAnimator != null) {
            animators.add(toolbarAnimator);
        }

        animatorSet.playTogether(animators);
        return animatorSet;
    }

    private Animator getViewFadeAnimator(View view, int index) {
        ObjectAnimator alphaAnimator =
                ObjectAnimator.ofFloat(view, View.ALPHA, mInitialAlpha, mFinalAlpha);
        alphaAnimator.setInterpolator(Interpolators.STANDARD_INTERPOLATOR);
        alphaAnimator.setDuration(mDurationMs);
        alphaAnimator.setStartDelay(getAnimationDelayForIndex(index));
        return alphaAnimator;
    }

    private Animator getViewScaleAnimator(View view, int index, boolean scaleX) {
        ObjectAnimator scaleAnimator =
                ObjectAnimator.ofFloat(
                        view, scaleX ? View.SCALE_X : View.SCALE_Y, mFinalScaleRatio);
        scaleAnimator.setInterpolator(Interpolators.STANDARD_INTERPOLATOR);
        scaleAnimator.setDuration(mDurationMs);
        scaleAnimator.setStartDelay(getAnimationDelayForIndex(index));
        return scaleAnimator;
    }

    private @Nullable Animator getToolbarFadeAnimator(long duration) {
        View toolbarView = mHubContainerView.findViewById(R.id.hub_toolbar);
        if (toolbarView == null) return null;

        ObjectAnimator fadeAnimator =
                ObjectAnimator.ofFloat(toolbarView, View.ALPHA, mInitialAlpha, mFinalAlpha);
        fadeAnimator.setInterpolator(Interpolators.STANDARD_INTERPOLATOR);
        fadeAnimator.setDuration(duration);
        fadeAnimator.addUpdateListener(
                animation -> {
                    if (animation.getAnimatedValue() instanceof Float animationAlpha) {
                        mOnAlphaChange.accept(animationAlpha);
                    }
                });
        return fadeAnimator;
    }

    private long getAnimationDelayForIndex(int index) {
        return index * VIEW_ANIMATION_DELAY_MS;
    }
}

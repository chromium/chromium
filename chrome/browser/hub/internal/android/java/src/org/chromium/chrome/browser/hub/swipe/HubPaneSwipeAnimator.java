// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub.swipe;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_SETTLE_MIN_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_SLIDE_ANIMATION_DURATION_MS;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.interpolators.Interpolators;

import java.util.Objects;

/** Handles slide and settle animations for Hub pane transitions. */
@NullMarked
public class HubPaneSwipeAnimator {
    /** Callback for reporting swipe drag animation progress. */
    public interface SwipeAnimationProgressCallback {
        /** Called with the normalized progress [0..1] and gesture direction. */
        void onProgress(float progress, boolean isSwipeLeft);
    }

    private final AnimationHandler mAnimationHandler;

    /** Creates an animator instance. */
    public HubPaneSwipeAnimator() {
        mAnimationHandler = new AnimationHandler();
    }

    /**
     * Animates the interactive settle transition between the current and adjacent pane views.
     *
     * @param currentView The current pane view.
     * @param adjacentView The adjacent pane view.
     * @param isSwitch Whether the gesture resulted in switching to the adjacent pane.
     * @param isSwipeLeft Whether the swipe direction was towards the left.
     * @param containerWidth The width of the container.
     * @param progressCallback Callback for progress updates during animation.
     * @param onAnimationEnd Callback invoked when the settle animation finishes.
     */
    public void animateSettle(
            View currentView,
            View adjacentView,
            boolean isSwitch,
            boolean isSwipeLeft,
            int containerWidth,
            @Nullable SwipeAnimationProgressCallback progressCallback,
            Runnable onAnimationEnd) {
        mAnimationHandler.forceFinishAnimation();
        if (containerWidth <= 0) {
            onAnimationEnd.run();
            return;
        }

        float offset = isSwipeLeft ? -containerWidth : containerWidth;
        float currentTargetX = isSwitch ? offset : 0f;
        float adjacentTargetX = isSwitch ? 0f : -offset;

        ValueAnimator currentAnim =
                ObjectAnimator.ofFloat(currentView, View.TRANSLATION_X, currentTargetX);
        Animator adjacentAnim =
                ObjectAnimator.ofFloat(adjacentView, View.TRANSLATION_X, adjacentTargetX);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(currentAnim, adjacentAnim);
        animatorSet.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);

        float remainingFraction =
                Math.abs(currentTargetX - currentView.getTranslationX()) / containerWidth;
        long settleDuration =
                Math.max(
                        PANE_SETTLE_MIN_DURATION_MS,
                        Math.round(PANE_SLIDE_ANIMATION_DURATION_MS * remainingFraction));
        animatorSet.setDuration(settleDuration);

        if (progressCallback != null) {
            currentAnim.addUpdateListener(
                    animation -> {
                        float dx = currentView.getTranslationX();
                        float progress = Math.abs(dx) / (float) containerWidth;
                        progressCallback.onProgress(progress, isSwipeLeft);
                    });
        }

        animatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        onAnimationEnd.run();
                    }
                });
        mAnimationHandler.startAnimation(animatorSet);
    }

    /**
     * Animates programmatic slide transitions between two views (e.g. from tab switcher toolbar
     * taps).
     *
     * @param container The container holding the views.
     * @param oldRootView The view sliding out.
     * @param newRootView The view sliding in.
     * @param isLeftToRight Whether sliding left-to-right or right-to-left.
     * @param onAnimationEnd Callback invoked when the slide animation completes.
     */
    public void animateSlideTransition(
            ViewGroup container,
            View oldRootView,
            View newRootView,
            boolean isLeftToRight,
            Runnable onAnimationEnd) {
        mAnimationHandler.forceFinishAnimation();
        int containerWidth = container.getWidth();
        if (containerWidth <= 0) {
            onAnimationEnd.run();
            return;
        }

        float oldViewEndTranslation = isLeftToRight ? containerWidth : -containerWidth;
        float newViewStartTranslation = isLeftToRight ? -containerWidth : containerWidth;

        oldRootView.setTranslationX(0);
        newRootView.setTranslationX(newViewStartTranslation);

        tryAddViewToContainer(container, newRootView);

        Animator slideOut =
                ObjectAnimator.ofFloat(oldRootView, View.TRANSLATION_X, 0, oldViewEndTranslation);
        slideOut.setDuration(PANE_SLIDE_ANIMATION_DURATION_MS);

        Animator slideIn =
                ObjectAnimator.ofFloat(newRootView, View.TRANSLATION_X, newViewStartTranslation, 0);
        slideIn.setDuration(PANE_SLIDE_ANIMATION_DURATION_MS);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(slideOut, slideIn);
        animatorSet.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        animatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        container.removeView(oldRootView);
                        oldRootView.setTranslationX(0);
                        newRootView.setTranslationX(0);
                        onAnimationEnd.run();
                    }
                });
        mAnimationHandler.startAnimation(animatorSet);
    }

    /** Cancels or finishes any currently running animation immediately. */
    public void forceFinishAnimation() {
        mAnimationHandler.forceFinishAnimation();
    }

    private void tryAddViewToContainer(ViewGroup container, View rootView) {
        ViewParent parent = rootView.getParent();
        if (!Objects.equals(parent, container)) {
            if (parent instanceof ViewGroup viewGroup) {
                viewGroup.removeView(rootView);
            }
            container.addView(rootView);
        }
    }
}

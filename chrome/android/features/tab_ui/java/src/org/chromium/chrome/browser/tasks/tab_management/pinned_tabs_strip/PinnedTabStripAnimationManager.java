// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.chromium.ui.interpolators.Interpolators.STANDARD_INTERPOLATOR;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.RectEvaluator;
import android.animation.ValueAnimator;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.ui.animation.AnimationHandler;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Animation manager for the pinned tab bar and its items. */
@NullMarked
class PinnedTabStripAnimationManager {
    // For pinned tab bar animations.
    private static final int STRIP_VISIBILITY_ANIMATION_DURATION_MS = 400;
    private final AnimationHandler mPinnedTabBarVisibilityAnimationHandler;
    private final TabListRecyclerView mRecyclerView;

    // For item animations.
    private static final int ITEM_WIDTH_ANIMATION_DURATION_MS = 400;
    private static final int ITEM_ZOOM_ANIMATION_DURATION_MS = 200;
    private static final float SELECTED_ITEM_SCALE = 0.8f;
    private static final float UNSELECTED_ITEM_SCALE = 1.0f;
    private static final float SELECTED_ITEM_ALPHA = 0.8f;
    private static final float UNSELECTED_ITEM_ALPHA = 1.0f;

    @IntDef({ItemState.SELECTED, ItemState.UNSELECTED})
    @Retention(RetentionPolicy.SOURCE)
    @interface ItemState {
        int SELECTED = 0;
        int UNSELECTED = 1;
    }

    private boolean mIsHiding;

    /**
     * Constructor for PinnedTabStripAnimationManager.
     *
     * @param recyclerView The {@link TabListRecyclerView} to animate.
     * @param animationHandler The {@link AnimationHandler} for managing animations.
     */
    PinnedTabStripAnimationManager(
            TabListRecyclerView recyclerView, AnimationHandler animationHandler) {
        mRecyclerView = recyclerView;
        mPinnedTabBarVisibilityAnimationHandler = animationHandler;
    }

    /**
     * Constructor for PinnedTabStripAnimationManager with a default AnimationHandler.
     *
     * @param recyclerView The {@link TabListRecyclerView} to animate.
     */
    PinnedTabStripAnimationManager(TabListRecyclerView recyclerView) {
        this(recyclerView, new AnimationHandler());
    }

    void animatePinnedTabBarVisibility(
            boolean shouldBeVisible,
            ObservableSupplierImpl<Boolean> isVisibilityAnimationRunningSupplier) {
        // Ensure the recyclerView is at least invisible so that it has a valid size.
        if (shouldBeVisible && mRecyclerView.getVisibility() == View.GONE) {
            mRecyclerView.setVisibility(View.INVISIBLE);
        }
        mRecyclerView.post(
                () -> updateVisibility(shouldBeVisible, isVisibilityAnimationRunningSupplier));
    }

    private void updateVisibility(
            boolean shouldBeVisible,
            ObservableSupplierImpl<Boolean> isVisibilityAnimationRunningSupplier) {
        TabListRecyclerView recyclerView = mRecyclerView;
        boolean currentlyVisible = recyclerView.getVisibility() == View.VISIBLE;

        boolean visibleOrShowing = currentlyVisible && !mIsHiding;
        boolean hiddenOrHiding = !currentlyVisible || mIsHiding;
        // We are already in the right state (or transitioning to it) so no-op.
        if ((shouldBeVisible && visibleOrShowing) || (!shouldBeVisible && hiddenOrHiding)) {
            return;
        }

        mPinnedTabBarVisibilityAnimationHandler.forceFinishAnimation();

        Rect startRect;
        Rect endRect;
        int width = recyclerView.getMeasuredWidth();
        int height = recyclerView.getMeasuredHeight();
        if (shouldBeVisible) {
            startRect = new Rect(0, 0, width, 1);
            endRect = new Rect(0, 0, width, height);
            mIsHiding = false;
        } else {
            startRect = new Rect(0, 0, width, height);
            endRect = new Rect(0, 0, width, 1);
            mIsHiding = true;
        }

        recyclerView.setAlpha(1.0f);
        recyclerView.setClipBounds(startRect);
        recyclerView.setVisibility(View.VISIBLE);
        ValueAnimator animator = ValueAnimator.ofFloat(0f, 1f);
        RectEvaluator rectEvaluator = new RectEvaluator();
        animator.addUpdateListener(
                animation -> {
                    float t = animation.getAnimatedFraction();
                    Rect interpolatedRect = rectEvaluator.evaluate(t, startRect, endRect);
                    recyclerView.setClipBounds(interpolatedRect);
                });
        animator.setDuration(STRIP_VISIBILITY_ANIMATION_DURATION_MS);
        animator.setInterpolator(STANDARD_INTERPOLATOR);
        animator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        isVisibilityAnimationRunningSupplier.set(true);
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        // Always remove the clip bounds so the next time the animation is updated
                        // or if the configuration changes (screen size, orientation, etc.) we are
                        // in a good state.
                        recyclerView.setClipBounds(null);
                        // Skip setting to GONE if we are invisible due to another animation.
                        if (!shouldBeVisible && recyclerView.getVisibility() != View.INVISIBLE) {
                            recyclerView.setVisibility(View.GONE);
                        }
                        mIsHiding = false;
                        isVisibilityAnimationRunningSupplier.set(false);
                    }
                });

        mPinnedTabBarVisibilityAnimationHandler.startAnimation(animator);
    }

    /**
     * Cancels any running pinned tab bar animations and sets the pinned tab bar to a visible state.
     *
     * @param animationRunningSupplier Supplier to notify about animation status.
     */
    void cancelPinnedTabBarAnimations(ObservableSupplierImpl<Boolean> animationRunningSupplier) {
        mPinnedTabBarVisibilityAnimationHandler.forceFinishAnimation();
        mRecyclerView.setVisibility(View.VISIBLE);
        mRecyclerView.setAlpha(1.0f);
        mRecyclerView.setClipBounds(null);
        setStripVisibilityAnimationRunning(animationRunningSupplier, false);
    }

    /**
     * Animates the width of a given view.
     *
     * @param view The view to animate.
     * @param targetWidth The target width of the view.
     * @param animationHandler The animation handler to manage the animation.
     */
    static void animateItemWidth(View view, int targetWidth, AnimationHandler animationHandler) {
        int startWidth = view.getWidth();

        // If the view is not laid out yet or width is the same, just set the width.
        if (startWidth == 0 || startWidth == targetWidth) {
            updateViewWidth(view, targetWidth);
            return;
        }

        // Finish existing animations, before starting new.
        animationHandler.forceFinishAnimation();

        ValueAnimator animator = ValueAnimator.ofInt(startWidth, targetWidth);
        animator.setDuration(ITEM_WIDTH_ANIMATION_DURATION_MS);
        animator.setInterpolator(STANDARD_INTERPOLATOR);
        animator.addUpdateListener(
                animation -> updateViewWidth(view, (int) animation.getAnimatedValue()));
        animationHandler.startAnimation(animator);
    }

    /**
     * Animates the scale of a view to a target scale.
     *
     * @param view The view to animate.
     * @param itemState The target state for the view, either {@link ItemState.SELECTED} or {@link
     *     ItemState.UNSELECTED}.
     * @param animationHandler The animation handler to manage the animation.
     */
    static void animateItemZoom(
            View view, @ItemState int itemState, AnimationHandler animationHandler) {
        animationHandler.forceFinishAnimation();

        float targetScale =
                itemState == ItemState.SELECTED ? SELECTED_ITEM_SCALE : UNSELECTED_ITEM_SCALE;
        float targetAlpha =
                itemState == ItemState.SELECTED ? SELECTED_ITEM_ALPHA : UNSELECTED_ITEM_ALPHA;

        ValueAnimator animator = ValueAnimator.ofFloat(0f, 1f);
        animator.setDuration(ITEM_ZOOM_ANIMATION_DURATION_MS);
        animator.setInterpolator(STANDARD_INTERPOLATOR);
        final float startScale = view.getScaleX();
        final float startAlpha = view.getAlpha();

        animator.addUpdateListener(
                animation -> {
                    float fraction = animation.getAnimatedFraction();

                    // Animate x and y scale.
                    float scaleDelta = targetScale - startScale;
                    float scale = startScale + scaleDelta * fraction;
                    view.setScaleX(scale);
                    view.setScaleY(scale);

                    // Animate alpha values.
                    float alphaDelta = targetAlpha - startAlpha;
                    view.setAlpha(startAlpha + alphaDelta * fraction);
                });
        animationHandler.startAnimation(animator);
    }

    private void setStripVisibilityAnimationRunning(
            ObservableSupplierImpl<Boolean> animationRunningSupplier, boolean isAnimating) {
        if (animationRunningSupplier != null && animationRunningSupplier.get() != isAnimating) {
            animationRunningSupplier.set(isAnimating);
        }
    }

    private static void updateViewWidth(View view, int width) {
        if (width <= 0) return;
        ViewGroup.LayoutParams layoutParams = view.getLayoutParams();
        if (layoutParams.width == width) return;
        layoutParams.width = width;
        view.setLayoutParams(layoutParams);
    }
}

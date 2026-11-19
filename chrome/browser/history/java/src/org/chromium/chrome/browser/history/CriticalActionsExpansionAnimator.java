// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.interpolators.Interpolators;

/**
 * Expands and collapses the critical actions panel of a history row, optionally with an animation.
 * When animated, the panel's container grows or shrinks while the panel itself keeps its full size,
 * so that the rows below slide down to reveal it, or back up to cover it.
 */
@NullMarked
class CriticalActionsExpansionAnimator {
    // Matches the duration of the UX prototype's transition of the panel.
    private static final long ANIMATION_DURATION_MS = 220;

    private final ImageView mExpandIcon;
    private final ViewGroup mPanelContainer;
    // Finishes expanding or collapsing right away when the row is detached, as it may be reused.
    private final View.OnAttachStateChangeListener mOnAttachStateChangeListener =
            new View.OnAttachStateChangeListener() {
                @Override
                public void onViewAttachedToWindow(View view) {}

                @Override
                public void onViewDetachedFromWindow(View view) {
                    endAnimation();
                }
            };

    // How far the panel is expanded, from 0 (collapsed) to 1 (expanded). In between while it's
    // being expanded or collapsed by mAnimator.
    private float mExpansion;
    // The full height of the panel's container, measured when an animation starts.
    private int mFullHeight;
    private @Nullable ValueAnimator mAnimator;

    /**
     * @param expandIcon The chevron, which is rotated as the panel is expanded.
     * @param panelContainer The container of the panel below the row.
     */
    CriticalActionsExpansionAnimator(ImageView expandIcon, ViewGroup panelContainer) {
        mExpandIcon = expandIcon;
        mPanelContainer = panelContainer;
        mPanelContainer.addOnAttachStateChangeListener(mOnAttachStateChangeListener);
    }

    /**
     * Expands or collapses the panel. When animated, it continues from the current progress if it's
     * already being expanded or collapsed.
     */
    void setExpanded(boolean expanded, boolean animate) {
        cancelAnimation();
        float target = expanded ? 1f : 0f;
        // Nothing to animate if it's already there, e.g. when tapped twice before the first frame.
        if (!animate || mExpansion == target) {
            setExpandedNow(expanded);
            return;
        }

        startAnimatingHeight();
        ValueAnimator animator = ValueAnimator.ofFloat(mExpansion, target);
        animator.setDuration(Math.round(ANIMATION_DURATION_MS * Math.abs(target - mExpansion)));
        animator.setInterpolator(Interpolators.STANDARD_INTERPOLATOR);
        animator.addUpdateListener(animation -> setExpansion((float) animation.getAnimatedValue()));
        animator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        // Ignore animations that were cancelled to be replaced.
                        if (mAnimator != animation) return;
                        mAnimator = null;
                        setExpandedNow(expanded);
                    }
                });
        mAnimator = animator;
        animator.start();
    }

    /** Finishes any running animation right away. */
    void endAnimation() {
        if (mAnimator != null) mAnimator.end();
    }

    /** Stops any running animation and stops listening to the panel's container. */
    void destroy() {
        cancelAnimation();
        mPanelContainer.removeOnAttachStateChangeListener(mOnAttachStateChangeListener);
    }

    @Nullable ValueAnimator getAnimatorForTesting() {
        return mAnimator;
    }

    /**
     * Prepares the panel's container for animating its height: measures its full height, pins the
     * panel to it so that it isn't squeezed while the container is smaller, and shows the container
     * at its current height.
     */
    private void startAnimatingHeight() {
        // The container is as wide as the row it's in.
        int width = mPanelContainer.getParent() instanceof View row ? row.getWidth() : 0;
        mPanelContainer.measure(
                width > 0
                        ? MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY)
                        : MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        mFullHeight = mPanelContainer.getMeasuredHeight();
        for (int i = 0; i < mPanelContainer.getChildCount(); i++) {
            View panel = mPanelContainer.getChildAt(i);
            setHeight(panel, panel.getMeasuredHeight());
        }
        setHeight(mPanelContainer, Math.round(mFullHeight * mExpansion));
        mPanelContainer.setVisibility(View.VISIBLE);
    }

    private void setExpandedNow(boolean expanded) {
        setExpansion(expanded ? 1f : 0f);
        // Let the panel and its container wrap their content again.
        for (int i = 0; i < mPanelContainer.getChildCount(); i++) {
            setHeight(mPanelContainer.getChildAt(i), LayoutParams.WRAP_CONTENT);
        }
        setHeight(mPanelContainer, LayoutParams.WRAP_CONTENT);
        mPanelContainer.setVisibility(expanded ? View.VISIBLE : View.GONE);
    }

    private void cancelAnimation() {
        ValueAnimator animator = mAnimator;
        if (animator == null) return;
        // Cleared first, so that the end listener ignores it.
        mAnimator = null;
        animator.cancel();
    }

    private void setExpansion(float expansion) {
        mExpansion = expansion;
        // The icon points down when collapsed, and is flipped to point up when expanded.
        mExpandIcon.setRotation(180f * expansion);
        if (mAnimator != null) {
            setHeight(mPanelContainer, Math.round(mFullHeight * expansion));
        }
    }

    private static void setHeight(View view, int height) {
        LayoutParams params = view.getLayoutParams();
        if (params == null) {
            params = new LayoutParams(LayoutParams.MATCH_PARENT, height);
        } else if (params.height == height) {
            return;
        } else {
            params.height = height;
        }
        view.setLayoutParams(params);
    }
}

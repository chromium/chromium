// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.animation.ValueAnimator;
import android.view.animation.DecelerateInterpolator;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.GestureStateListenerWithScroll;

/**
 * A Gesture listener that implements scroll-to-hide for the assistant bottomsheet when in FULL
 * state.
 */
public class ScrollToHideGestureListener implements GestureStateListenerWithScroll {
    /** Base duration of the animation of the sheet. 218 ms is a spec for material design. */
    private static final int BASE_ANIMATION_DURATION_MS = 218;

    private final BottomSheetController mBottomSheetController;
    private final AssistantBottomSheetContent mContent;
    @Nullable
    private final BottomSheetObserver mStateChangeTracker = new StateChangeTracker();

    private boolean mScrolling;

    /** Remembers the last value of scroll offset, to compute the delta for the next move. */
    private int mLastScrollOffsetY;

    /**
     * A capture of {@code mBottomSheetController.getCurrentOffset()}. At the end of a scroll, it is
     * compared with the current value to figure out whether the sheet was overall scrolled up or
     * down.
     */
    private float mOffsetMarkPx;

    /** This animator moves the sheet to its final position after scrolling ended. */
    private ValueAnimator mAnimator;

    /**
     * The offset the animator is moving towards. Only relevant when {@code mAnimator} is active.
     */
    private int mAnimatorGoalOffsetPx;

    public ScrollToHideGestureListener(
            BottomSheetController bottomSheetController, AssistantBottomSheetContent content) {
        mBottomSheetController = bottomSheetController;
        mContent = content;
    }

    /** True while scrolling. */
    public boolean isScrolling() {
        return mScrolling;
    }

    /** True if the sheet was hidden. */
    public boolean isSheetHidden() {
        return mBottomSheetController.getSheetState() == SheetState.FULL
                && mBottomSheetController.getCurrentOffset() == 0;
    }

    /** True if the sheet is currently hiding or expanding after a scroll. */
    public boolean isSheetSettling() {
        return mBottomSheetController.getSheetState() == SheetState.FULL && mAnimator != null
                && mAnimator.isStarted();
    }

    @Override
    public void onScrollStarted(int scrollOffsetY, int scrollExtentY) {
        Callback<Integer> offsetController = mContent.getOffsetController();
        if (offsetController == null) return;

        // Scroll to hide only applies if the sheet is fully opened, and state is FULL or is being
        // opened, and target state is FULL.
        if (mBottomSheetController.getTargetSheetState() == SheetState.FULL) {
            // This stops animation and freezes the sheet in place.
            offsetController.onResult(mBottomSheetController.getCurrentOffset());
        }
        if (mBottomSheetController.getSheetState() != SheetState.FULL) return;

        resetScrollingState(); // also cancels any running animations
        mScrolling = true;
        mLastScrollOffsetY = scrollOffsetY;
        mOffsetMarkPx = mBottomSheetController.getCurrentOffset();
        mBottomSheetController.addObserver(mStateChangeTracker);
    }

    @Override
    public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
        onScrollOffsetOrExtentChanged(scrollOffsetY, scrollExtentY);

        if (!mScrolling) return;

        resetScrollingState();

        int maxOffsetPx = getMaxOffsetPx();
        int currentOffsetPx = mBottomSheetController.getCurrentOffset();
        if (currentOffsetPx == 0 || currentOffsetPx == maxOffsetPx) {
            return;
        }

        if (currentOffsetPx >= mOffsetMarkPx || scrollOffsetY == 0) {
            animateTowards(maxOffsetPx);
        } else {
            animateTowards(0);
        }
    }

    @Override
    public void onScrollOffsetOrExtentChanged(int scrollOffsetY, int scrollExtentY) {
        if (!mScrolling) {
            // It's possible for the scroll offset to reset to 0 outside of a scroll, if the page or
            // viewport size change. Scrolling up is not possible so if the sheet is hidden or about
            // to be hidden, show it.
            if (scrollOffsetY == 0
                    && (isSheetHidden() || (isSheetSettling() && mAnimatorGoalOffsetPx == 0))) {
                animateTowards(getMaxOffsetPx());
            }
            return;
        }

        Callback<Integer> offsetController = mContent.getOffsetController();
        if (offsetController == null) {
            resetScrollingState();
            return;
        }

        // deltaPx is the value to add to the current sheet offset (height). It is negative when
        // scrolling down, that is, when scrollOffsetY increases.
        int deltaPx = mLastScrollOffsetY - scrollOffsetY;
        mLastScrollOffsetY = scrollOffsetY;

        int maxOffsetPx = getMaxOffsetPx();
        int offsetPx = MathUtils.clamp(
                mBottomSheetController.getCurrentOffset() + deltaPx, 0, maxOffsetPx);
        offsetController.onResult(offsetPx);

        // If either extremes were reached, update the mark. The decision to fully show or hide will
        // be relative to that point.
        if (offsetPx == 0) {
            mOffsetMarkPx = 0;
        } else if (offsetPx >= maxOffsetPx) {
            mOffsetMarkPx = maxOffsetPx;
        }
    }

    @Override
    public void onFlingStartGesture(int scrollOffsetY, int scrollExtentY) {
        // Flinging and scrolling are handled the same, the sheet follows the movement of the
        // browser page.
        onScrollStarted(scrollOffsetY, scrollExtentY);
    }

    @Override
    public void onFlingEndGesture(int scrollOffsetY, int scrollExtentY) {
        onScrollEnded(scrollOffsetY, scrollExtentY);
    }

    @Override
    public void onDestroyed() {
        resetScrollingState();
    }

    private int getMaxOffsetPx() {
        return mContent.getContentView().getHeight();
    }

    private void resetScrollingState() {
        mScrolling = false;
        mLastScrollOffsetY = 0;
        cancelAnimation();
        mBottomSheetController.removeObserver(mStateChangeTracker);
    }

    private void cancelAnimation() {
        if (mAnimator == null) return;

        mAnimator.cancel();
        mAnimator = null;
    }

    /** Animate the sheet towards {@code goalOffsetPx} without changing its state. */
    private void animateTowards(int goalOffsetPx) {
        Callback<Integer> offsetController = mContent.getOffsetController();
        if (offsetController == null) return;

        ValueAnimator animator =
                ValueAnimator.ofInt(mBottomSheetController.getCurrentOffset(), goalOffsetPx);
        animator.setDuration(BASE_ANIMATION_DURATION_MS);
        animator.setInterpolator(new DecelerateInterpolator(1.0f));
        animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animator) {
                if (mAnimator != animator) return;

                offsetController.onResult((Integer) animator.getAnimatedValue());
            }
        });

        mAnimator = animator;
        mAnimatorGoalOffsetPx = goalOffsetPx;
        mAnimator.start();
    }

    /** Stop scrolling if the sheet leaves the FULL state during scrolling. */
    private class StateChangeTracker extends EmptyBottomSheetObserver {
        @Override
        public void onSheetStateChanged(@SheetState int newState) {
            if (newState != SheetState.FULL) {
                resetScrollingState();
            }
        }
    }
}

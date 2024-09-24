// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.StackScroller;
import org.chromium.ui.base.LocalizationUtils;

/** Delegate to manage the scrolling logic for the tab strip. */
public class ScrollDelegate {
    // Constants.
    private static final int SCROLL_DURATION_MS = 250;
    private static final int SCROLL_DURATION_MS_MEDIUM = 350;
    private static final int SCROLL_DURATION_MS_LONG = 450;
    private static final int SCROLL_DISTANCE_SHORT = 960;
    private static final int SCROLL_DISTANCE_MEDIUM = 1920;
    private static final float EPSILON = 0.001f;

    // External influences.
    private StackScroller mScroller;

    // Internal state.
    /**
     * mScrollOffset represents how far left or right the tab strip is currently scrolled. This is 0
     * when scrolled all the way left and mMinScrollOffset when scrolled all the way right.
     */
    private float mScrollOffset;

    /**
     * mMinScrollOffset is the scroll offset when the tab strip is scrolled all the way to the
     * right. e.g. this is ~(totalViewWidth - stripWidth) if the tab strip can scroll, and 0
     * otherwise.
     */
    private float mMinScrollOffset;

    /**
     * Additional space allocated at the start of the tab strip to allow dragging out of a tab
     * group, if needed.
     */
    private float mReorderStartMargin;

    /**
     * Updates all internal resources and dimensions.
     *
     * @param context The current Android {@link Context}.
     */
    public void onContextChanged(Context context) {
        mScroller = new StackScroller(context);
    }

    /**
     * @return The current scroll offset.
     */
    float getScrollOffset() {
        return mScrollOffset;
    }

    /**
     * Sets the new scroll offset, and clamps to a valid value.
     *
     * @param scrollOffset The new scroll offset.
     * @return The difference between the new and old scroll offsets, accounting for RTL.
     */
    float setScrollOffset(float scrollOffset) {
        float oldScrollOffset = mScrollOffset;
        mScrollOffset = MathUtils.clamp(scrollOffset, mMinScrollOffset, 0);

        return MathUtils.flipSignIf(
                oldScrollOffset - mScrollOffset, LocalizationUtils.isLayoutRtl());
    }

    /**
     * Update any scrolls based on the current time.
     *
     * @param time The current time of the app in ms.
     * @return Whether a scroll is still in progress or not.
     */
    boolean updateScrollInProgress(long time) {
        if (mScroller.computeScrollOffset(time)) {
            setScrollOffset(mScroller.getCurrX());
            return true;
        }
        return false;
    }

    /**
     * Calculate the new minimum scroll offset based on the strip's current properties. Called on
     * every layout update.
     *
     * @param stripViews List of all views on the tab strip.
     * @param width Tab strip width.
     * @param leftMargin Tab strip left margin.
     * @param rightMargin Tab strip right margin.
     * @param cachedTabWidth Ideal tab width in dp.
     * @param tabOverlapWidth Overlap width of tabs in dp.
     * @param groupTitleOverlapWidth Overlap width of group titles in dp.
     */
    void updateScrollOffsetLimits(
            StripLayoutView[] stripViews,
            float width,
            float leftMargin,
            float rightMargin,
            float cachedTabWidth,
            float tabOverlapWidth,
            float groupTitleOverlapWidth) {
        // 1. Compute the width of the available space for all tabs.
        float stripWidth = width - leftMargin - rightMargin;

        // 2. Compute the effective width of every strip view (i.e. tabs, title indicators).
        float totalViewWidth = 0.f;
        for (int i = 0; i < stripViews.length; i++) {
            final StripLayoutView view = stripViews[i];
            if (view instanceof final StripLayoutTab tab) {
                if (tab.isCollapsed()) {
                    // Need to use real width here (which gets animated to effectively 0), so we
                    // don't "jump", but instead smoothly scroll when collapsing near the end of a
                    // full tab strip.
                    totalViewWidth += tab.getWidth() - tabOverlapWidth;
                } else if (!tab.isClosed() && !tab.isDraggedOffStrip()) {
                    totalViewWidth += cachedTabWidth - tabOverlapWidth;
                }
            } else if (view instanceof StripLayoutGroupTitle groupTitle) {
                totalViewWidth += (groupTitle.getWidth() - groupTitleOverlapWidth);
            }
        }

        for (int i = 0; i < stripViews.length; i++) {
            if (stripViews[i] instanceof StripLayoutTab tab) {
                totalViewWidth += tab.getTrailingMargin();
            }
        }

        // 3. Correct fencepost error in totalViewWidth;
        totalViewWidth = totalViewWidth + tabOverlapWidth;

        // 4. Calculate the minimum scroll offset.
        mMinScrollOffset = Math.min(0.f, stripWidth - totalViewWidth);

        // 5. Always include the reorder start margin in the minScrollOffset calculations, so it
        // can be scrolled offscreen, regardless of how full the rest of the strip is. If needed,
        // round > -EPSILON to 0.
        mMinScrollOffset -= mReorderStartMargin;
        if (mMinScrollOffset > -EPSILON) mMinScrollOffset = 0.f;

        // 6. Clamp mScrollOffset to make sure it's in the valid range.
        setScrollOffset(mScrollOffset);
    }

    /**
     * Adjusts the scroll offset based on the change in start margin to make it appear as though the
     * interacting tab does not move. Also adjusts the minScrollOffset accordingly (without
     * calculating from scratch) to ensure the new scroll offset will be valid.
     *
     * @param newStartMargin The new reorder start margin.
     */
    void setReorderStartMargin(float newStartMargin) {
        float delta = newStartMargin - mReorderStartMargin;
        mReorderStartMargin = newStartMargin;

        // Adjusts the minScrollOffset here, since the next update cycle (which accounts for the new
        // reorderStartMargin) will not yet have run.
        mMinScrollOffset -= delta;
        if (mMinScrollOffset > -EPSILON) mMinScrollOffset = 0.f;

        // Auto-scroll to prevent any apparent movement.
        setScrollOffset(mScrollOffset - delta);
    }

    float getReorderStartMargin() {
        return mReorderStartMargin;
    }

    /**
     * Returns whether we are still visually scrolling the tab strip or not. This does not account
     * for the reorder auto-scroll.
     */
    boolean isFinished() {
        return mScroller.isFinished();
    }

    /**
     * Stops the currently running scroll, if any. This keeps the scroll offset at its current
     * position, without causing the scroller to move to its final x position. This does not account
     * for the reorder auto-scroll.
     */
    void stopScroll() {
        mScroller.forceFinished(true);
    }

    /**
     * Scroll a given distance from the current position.
     *
     * @param time The current time of the app in ms.
     * @param delta The signed distance to scroll from the current position.
     * @param animate Whether or not this should be animated.
     */
    void startScroll(long time, float delta, boolean animate) {
        if (animate) {
            mScroller.startScroll(
                    Math.round(mScrollOffset),
                    /* startY= */ 0,
                    (int) delta,
                    /* dy= */ 0,
                    time,
                    getScrollDuration(delta));
        } else {
            setScrollOffset(mScrollOffset + delta);
        }
    }

    /**
     * Scroll in response to a fling.
     *
     * @param time The current time of the app in ms.
     * @param velocity The velocity in the x direction.
     */
    void fling(long time, float velocity) {
        // 1. If we're fast scrolling, figure out the destination of the scroll so we can apply it
        // to the end of this fling.
        int scrollDeltaRemaining = 0;
        if (!mScroller.isFinished()) {
            scrollDeltaRemaining = mScroller.getFinalX() - Math.round(mScrollOffset);
            mScroller.forceFinished(true);
        }

        // 2. Kick off the fling.
        mScroller.fling(
                Math.round(mScrollOffset),
                /* startY= */ 0,
                (int) velocity,
                /* velocityY= */ 0,
                (int) mMinScrollOffset,
                /* maxX= */ 0,
                /* minY= */ 0,
                /* maxY= */ 0,
                /* overX= */ 0,
                /* overY= */ 0,
                time);
        mScroller.setFinalX(mScroller.getFinalX() + scrollDeltaRemaining);
    }

    /**
     * @param isLeft Whether the offset from the left or right side should be returned.
     * @return The delta from the current scroll offset from the min/max scroll offset based on the
     *     requested side.
     */
    float getEdgeOffset(boolean isLeft) {
        // In RTL, scroll position 0 is on the right side of the screen, whereas in LTR scroll
        // position 0 is on the left. Account for that in the offset calculation.
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        boolean useUnadjustedScrollOffset = isRtl != isLeft;
        float scrollOffset = mScrollOffset;

        return -(useUnadjustedScrollOffset ? scrollOffset : (mMinScrollOffset - scrollOffset));
    }

    /**
     * Scales the scroll duration based on the scroll distance.
     *
     * @param scrollDelta The signed delta to scroll from the current position.
     * @return the duration in ms.
     */
    @VisibleForTesting
    int getScrollDuration(float scrollDelta) {
        float scrollDistance = Math.abs(scrollDelta);
        if (scrollDistance <= SCROLL_DISTANCE_SHORT) {
            return SCROLL_DURATION_MS;
        } else if (scrollDistance <= SCROLL_DISTANCE_MEDIUM) {
            return SCROLL_DURATION_MS_MEDIUM;
        } else {
            return SCROLL_DURATION_MS_LONG;
        }
    }

    /**
     * Directly sets the scroll offset. This may temporarily set it to an un-clamped value.
     *
     * @param scrollOffset The new scroll offset.
     */
    void setNonClampedScrollOffsetForTesting(float scrollOffset) {
        mScrollOffset = scrollOffset;
    }

    /**
     * @param minScrollOffset The minimum scroll offset.
     */
    void setMinScrollOffsetForTesting(float minScrollOffset) {
        mMinScrollOffset = minScrollOffset;
    }

    /**
     * @return The minimum scroll offset.
     */
    float getMinScrollOffsetForTesting() {
        return mMinScrollOffset;
    }

    /**
     * @return The scroller.
     */
    StackScroller getScrollerForTesting() {
        return mScroller;
    }
}

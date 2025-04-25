// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.StackScroller;
import org.chromium.ui.base.LocalizationUtils;

/**
 * Delegate to manage the scrolling logic for the tab strip.
 *
 * <p><b>Important</b>: UI values managed by this class, unless documented otherwise, are in a
 * <i>dynamic</i> coordinate system, depending on whether the layout is LTR or RTL. This covers:
 *
 * <ul>
 *   <li>internal UI states, such as "scroll offset" vectors,
 *   <li>parameters passed to public methods, such as a "delta" vector,
 *   <li>return values of public methods.
 * </ul>
 *
 * <p>For LTR layouts, this class uses the following coordinate system:
 *
 * <pre>
 *     (0,0) (top-left corner of the window)
 *     +----------------> X
 *     |
 *     |
 *     |
 *     |
 *    \|/
 *     Y
 * </pre>
 *
 * For RTL layouts, this class uses the following coordinate system:
 *
 * <pre>
 *                        (0, 0) (top-right corner of the window)
 *     X <----------------+
 *                        |
 *                        |
 *                        |
 *                        |
 *                       \|/
 *                        Y
 * </pre>
 *
 * <p>In practice, the following is expected from callers:
 *
 * <ul>
 *   <li>For LTR layouts, no action is needed since the dynamic coordinate system is the same as the
 *       window coordinate system.
 *   <li>For RTL layouts: callers must flip input before passing it to this class if the input is a
 *       1-D vector on the X axis under the window coordinate system. Similarly, callers must also
 *       flip output from this class before applying it to the window coordinate system.
 * </ul>
 *
 * <p>Example:
 *
 * <pre><code>
 *     // Vector under the window coordinate system, meaning "scroll to the right by 10dp".
 *     float newScrollOffset = 10.0;
 *
 *     // Convert the vector to the dynamic coordinate system before passing it to this class.
 *     float newScrollOffsetForScrollDelegate =
 *       MathUtils.flipSignIf(newScrollOffset, LocalizationUtils.isLayoutRtl());
 *     scrollDelegate.setScrollOffset(newScrollOffsetForScrollDelegate);
 *
 *    // Vector under the dynamic coordinate system
 *    float currentScrollOffset = scrollDelegate.getScrollOffset();
 *
 *    // Before using the vector under the window coordinate system, convert the vector.
 *    float scrollOffsetToBeUsed =
 *      MathUtils.flipSignIf(currentScrollOffset, LocalizationUtils.isLayoutRtl());
 * </code></pre>
 *
 * <p>Due to historical reasons, a lot of existing code already uses this class in the way
 * documented above, so it requires time and effort to migrate this class to the <b>static</b>
 * window coordinate system that doesn't change due to layout configurations (LTR/RTL).
 */
public class ScrollDelegate {
    // Constants.
    private static final int SCROLL_DURATION_MS = 250;
    private static final int SCROLL_DURATION_MS_MEDIUM = 350;
    private static final int SCROLL_DURATION_MS_LONG = 450;
    private static final int SCROLL_DISTANCE_SHORT = 960;
    private static final int SCROLL_DISTANCE_MEDIUM = 1920;
    private static final float EPSILON = 0.001f;

    // External influences.
    private final StackScroller mScroller;

    // Internal state.
    /**
     * A 1-D vector on the X axis under the dynamic coordinate system (see class doc). It represents
     * the direction and distance the tab strip is currently scrolled.
     *
     * <p>This is 0 when the tab strip is not scrolled and {@code mScrollOffsetLimit} when scrolled
     * all the way to the limit.
     */
    private float mScrollOffset;

    /**
     * A 1-D vector on the X axis under the dynamic coordinate system (see class doc). It represents
     * the direction and maximum distance the tab strip can be scrolled.
     *
     * <p>This is 0 when the tab strip can't be scrolled (i.e., all tabs are visible), and
     * approximately (tabStripWidth - totalTabWidth) otherwise. Note that when the value is not 0,
     * it's always negative, no matter if the layout is LTR or RTL. This is because this vector is
     * under the dynamic coordinate system (see class doc).
     */
    private float mScrollOffsetLimit;

    /**
     * A scalar value representing the additional space allocated at the start of the tab strip to
     * allow dragging out of a tab group, if needed.
     *
     * <p>This value should always be non-negative.
     */
    private float mReorderStartMargin;

    ScrollDelegate(Context context) {
        mScroller = new StackScroller(context);
    }

    /**
     * @return The current scroll offset under the dynamic coordinate system (see class doc).
     */
    public float getScrollOffset() {
        return mScrollOffset;
    }

    /**
     * Sets the new scroll offset, and clamps to a valid value.
     *
     * @param scrollOffset The new scroll offset under the dynamic coordinate system (see class
     *     doc).
     * @return The 1-D vector along the X axis from the new scroll offset to the old scroll offset,
     *     under the <b>window</b> coordinate system (see class doc).
     */
    public float setScrollOffset(float scrollOffset) {
        float oldScrollOffset = mScrollOffset;
        mScrollOffset = MathUtils.clamp(scrollOffset, mScrollOffsetLimit, 0);

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
     * Calculate the new scroll offset limit based on the strip's current properties. Called on
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
        // TODO(crbug.com/376525967): Pull overlap width from utils constant instead of passing in.
        // 1. Compute the width of the available space for all tabs.
        float stripWidth = width - leftMargin - rightMargin;

        // 2. Compute the effective width of every strip view (i.e. tabs, title indicators).
        float totalViewWidth = 0.f;
        for (int i = 0; i < stripViews.length; i++) {
            final StripLayoutView view = stripViews[i];
            if (view.isDraggedOffStrip()) continue;

            if (view instanceof final StripLayoutTab tab) {
                if (tab.isCollapsed()) {
                    // Need to use real width here (which gets animated to effectively 0), so we
                    // don't "jump", but instead smoothly scroll when collapsing near the end of a
                    // full tab strip.
                    totalViewWidth += tab.getWidth() - tabOverlapWidth;
                } else if (!tab.isClosed()) {
                    totalViewWidth += cachedTabWidth - tabOverlapWidth;
                }
            } else if (view instanceof StripLayoutGroupTitle groupTitle) {
                totalViewWidth += (groupTitle.getWidth() - groupTitleOverlapWidth);
            }
        }

        for (int i = 0; i < stripViews.length; i++) {
            totalViewWidth += stripViews[i].getTrailingMargin();
        }

        // 3. Correct fencepost error in totalViewWidth;
        totalViewWidth = totalViewWidth + tabOverlapWidth;

        // 4. Calculate the scroll offset limit.
        // Note that this is always non-positive under the dynamic coordinate system (see class
        // doc).
        mScrollOffsetLimit = Math.min(0.f, stripWidth - totalViewWidth);

        // 5. Always include the reorder start margin in the scrollOffsetLimit calculations, so it
        // can be scrolled offscreen, regardless of how full the rest of the strip is. If needed,
        // round > -EPSILON to 0.
        mScrollOffsetLimit -= mReorderStartMargin;
        if (mScrollOffsetLimit > -EPSILON) mScrollOffsetLimit = 0.f;

        // 6. Clamp mScrollOffset to make sure it's in the valid range.
        setScrollOffset(mScrollOffset);
    }

    /**
     * Adjusts the scroll offset based on the change in start margin to make it appear as though the
     * interacting tab does not move. Also adjusts {@code mScrollOffsetLimit} accordingly (without
     * calculating from scratch) to ensure the new scroll offset will be valid.
     *
     * @param newStartMargin The new reorder start margin.
     */
    public void setReorderStartMargin(float newStartMargin) {
        float delta = newStartMargin - mReorderStartMargin;
        mReorderStartMargin = newStartMargin;

        // Adjusts the scrollOffSetLimit here, since the next update cycle (which accounts for the
        // new reorderStartMargin) will not yet have run.
        mScrollOffsetLimit -= delta;
        if (mScrollOffsetLimit > -EPSILON) mScrollOffsetLimit = 0.f;

        // Auto-scroll to prevent any apparent movement.
        setScrollOffset(mScrollOffset - delta);
    }

    public float getReorderStartMargin() {
        return mReorderStartMargin;
    }

    /**
     * Returns whether we are still visually scrolling the tab strip or not. This does not account
     * for the reorder auto-scroll.
     */
    public boolean isFinished() {
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
     * @param delta The signed distance to scroll from the current position, under the dynamic
     *     coordinate system (see class doc).
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
     * @param velocity The velocity in the x direction, under the dynamic coordinate system (see
     *     class doc).
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
                (int) mScrollOffsetLimit,
                /* maxX= */ 0,
                /* minY= */ 0,
                /* maxY= */ 0,
                /* overX= */ 0,
                /* overY= */ 0,
                time);
        mScroller.setFinalX(mScroller.getFinalX() + scrollDeltaRemaining);
    }

    /**
     * Calculates a 1-D vector under the window coordinate system that represents the delta from the
     * first/last tab's edge to the tab strip's edge.
     *
     * <pre>
     *     isLeft = true
     *       LTR layout: vector (first tab's left edge --> tab strip's left edge)
     *       RTL layout: vector (last tab's left edge --> tab strip's left edge)
     *     isLeft = false
     *       LTR layout: vector (last tab's right edge --> tab strip's right edge)
     *       RTL layout: vector (first tab's right edge --> tab strip's right edge)
     * </pre>
     *
     * @param isLeft Whether the offset for the left or right side should be returned.
     * @return the 1-D vector as documented above.
     */
    float getEdgeOffset(boolean isLeft) {
        // Under the dynamic coordinate system used by this class (see class doc):
        //
        // In RTL, scroll position 0 is on the right side of the screen, whereas in LTR scroll
        // position 0 is on the left. Account for that in the offset calculation.
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        boolean useUnadjustedScrollOffset = isRtl != isLeft;
        float scrollOffset = mScrollOffset;

        return -(useUnadjustedScrollOffset ? scrollOffset : (mScrollOffsetLimit - scrollOffset));
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
     * @param scrollOffset The new scroll offset under the dynamic coordinate system (see class
     *     doc).
     */
    void setNonClampedScrollOffsetForTesting(float scrollOffset) {
        mScrollOffset = scrollOffset;
    }

    /**
     * @param scrollOffsetLimit The scroll offset limit under the dynamic coordinate system (see
     *     class doc).
     */
    void setScrollOffsetLimitForTesting(float scrollOffsetLimit) {
        mScrollOffsetLimit = scrollOffsetLimit;
    }

    /**
     * @return The scroll offset limit under the dynamic coordinate system (see class doc).
     */
    float getScrollOffsetLimitForTesting() {
        return mScrollOffsetLimit;
    }

    /**
     * @return The scroller.
     */
    StackScroller getScrollerForTesting() {
        return mScroller;
    }

    // Abort scroll animation and set offset.
    void finishScrollForTesting() {
        mScroller.abortAnimation();
        setScrollOffset(mScroller.getFinalX());
    }
}

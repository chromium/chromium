// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.animation.Animator;
import android.util.FloatProperty;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.ui.base.LocalizationUtils;

import java.util.List;

/** Delegate to manage the scrolling logic for the tab strip. */
public class ScrollDelegate {
    /** A property for animations to use for changing the X offset of the tab. */
    public static final FloatProperty<ScrollDelegate> SCROLL_OFFSET =
            new FloatProperty<>("scrollOffset") {
                @Override
                public void setValue(ScrollDelegate object, float value) {
                    object.setScrollOffset(value);
                }

                @Override
                public Float get(ScrollDelegate object) {
                    return object.getScrollOffset();
                }
            };

    private static final int ANIM_TAB_SLIDE_OUT_MS = 250;
    private static final float EPSILON = 0.001f;

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
     * Views may shift when we enter reorder mode. We auto-scroll during this shift to make it
     * appear as though the reordering view has no apparent movement. When the tab strip isn't full,
     * the attempted auto-scrolling may be "cancelled" as it is out-of-bounds of mMinScrollOffset,
     * making it appear as though the reordering view moved away from where the user initiated the
     * reorder. mReorderExtraMinScrollOffset is allocated to allow for auto-scrolling in this case.
     */
    private float mReorderExtraMinScrollOffset;

    /**
     * This is only meant to be used to support the SCROLL_OFFSET animator. Skip clamping, since
     * some animations occur as the width of the views (and thus the minScrollOffset) is changing.
     *
     * @param scrollOffset The new scroll offset.
     */
    @VisibleForTesting
    void setScrollOffset(float scrollOffset) {
        mScrollOffset = scrollOffset;
    }

    float getScrollOffset() {
        return mScrollOffset;
    }

    float getMinScrollOffset() {
        return mMinScrollOffset;
    }

    void setReorderMinScrollOffset(float reorderMinScrollOffset) {
        mReorderExtraMinScrollOffset = reorderMinScrollOffset;
    }

    float getReorderExtraMinScrollOffset() {
        return mReorderExtraMinScrollOffset;
    }

    /**
     * @param scrollOffset The new scroll offset.
     * @return The difference between the new and old scroll offsets, accounting for RTL.
     */
    float setClampedScrollOffset(float scrollOffset) {
        float oldScrollOffset = mScrollOffset;
        mScrollOffset =
                MathUtils.clamp(scrollOffset, mMinScrollOffset - mReorderExtraMinScrollOffset, 0);

        return MathUtils.flipSignIf(
                oldScrollOffset - mScrollOffset, LocalizationUtils.isLayoutRtl());
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
     * @param reorderStartMargin The margin added to allow reordering near the strip's start-side.
     * @param shouldShowTrailingMargins Whether or not reorder trailing margins should be included.
     */
    void updateScrollOffsetLimits(
            StripLayoutView[] stripViews,
            float width,
            float leftMargin,
            float rightMargin,
            float cachedTabWidth,
            float tabOverlapWidth,
            float groupTitleOverlapWidth,
            float reorderStartMargin,
            boolean shouldShowTrailingMargins) {
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

        if (shouldShowTrailingMargins) {
            totalViewWidth += reorderStartMargin;
            for (int i = 0; i < stripViews.length; i++) {
                if (stripViews[i] instanceof StripLayoutTab tab) {
                    totalViewWidth += tab.getTrailingMargin();
                }
            }
        }

        // 3. Correct fencepost error in tabswidth;
        totalViewWidth = totalViewWidth + tabOverlapWidth;

        // 4. Calculate the minimum scroll offset.  Round > -EPSILON to 0.
        mMinScrollOffset = Math.min(0.f, stripWidth - totalViewWidth);
        if (mMinScrollOffset > -EPSILON) mMinScrollOffset = 0.f;

        // 5. Clamp mScrollOffset to make sure it's in the valid range.
        setClampedScrollOffset(mScrollOffset);
    }

    void maybeAnimateScrollOffset(
            CompositorAnimationHandler animationHandler,
            List<Animator> animationList,
            float startValue,
            float endValue) {
        if (animationList != null) {
            animationList.add(
                    CompositorAnimator.ofFloatProperty(
                            animationHandler,
                            this,
                            ScrollDelegate.SCROLL_OFFSET,
                            startValue,
                            endValue,
                            ANIM_TAB_SLIDE_OUT_MS));
        } else {
            setScrollOffset(endValue);
        }
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
}

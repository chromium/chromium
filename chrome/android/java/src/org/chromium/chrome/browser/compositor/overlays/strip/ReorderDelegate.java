// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.ANIM_TAB_MOVE_MS;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.ANIM_TAB_SLIDE_OUT_MS;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.animation.AnimatorListenerAdapter;

import androidx.annotation.Nullable;

import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.base.LocalizationUtils;

import java.util.ArrayList;
import java.util.List;

/** Delegate to manage the reordering logic for the tab strip. */
public class ReorderDelegate {
    // Test State.
    boolean mAnimationsDisabledForTesting;

    // Tab State.
    private TabGroupModelFilter mTabGroupModelFilter;
    private TabModel mModel;

    // Tab Strip State.
    ScrollDelegate mScrollDelegate;

    // Internal State.
    private boolean mInitialized;

    // Reorder State
    boolean mInReorderMode;

    /** Indicates a group title sliding due to a tab being dragged over it during reorder. */
    boolean mGroupTitleSliding;

    StripLayoutTab mInteractingTab;

    boolean getInReorderMode() {
        return mInReorderMode;
    }

    void setInReorderMode(boolean inReorderMode) {
        mInReorderMode = inReorderMode;
    }

    StripLayoutTab getInteractingTab() {
        return mInteractingTab;
    }

    void setInteractingTab(StripLayoutTab interactingTab) {
        mInteractingTab = interactingTab;
    }

    boolean getGroupTitleSliding() {
        // TODO(crbug.com/372546700): Remove once we animate using OFFSET_X instead.
        return mGroupTitleSliding;
    }

    /**
     * Passes the dependencies needed in this delegate. Passed here as they aren't ready on
     * instantiation.
     *
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} linked to this delegate.
     * @param scrollDelegate The {@link ScrollDelegate} linked to this delegate.
     */
    void initialize(TabGroupModelFilter tabGroupModelFilter, ScrollDelegate scrollDelegate) {
        mTabGroupModelFilter = tabGroupModelFilter;
        mModel = mTabGroupModelFilter.getTabModel();
        mScrollDelegate = scrollDelegate;

        mInitialized = true;
    }

    /**
     * Calculates the start and end margins needed to allow for reordering tabs into/out of groups
     * near the edge of the tab strip. 0 if the first/last tabs aren't grouped, respectively.
     *
     * @param firstTab The first {@link StripLayoutTab}.
     * @param lastTab The last {@link StripLayoutTab}.
     * @param tabMarginWidth The desired width for tab group margins.
     * @param reorderingForTabDrop Whether we're processing for a tab dropping into another window.
     */
    void setEdgeMarginsForReorder(
            StripLayoutTab firstTab,
            StripLayoutTab lastTab,
            float tabMarginWidth,
            boolean reorderingForTabDrop) {
        if (!mInitialized) return;
        float marginWidth = tabMarginWidth * REORDER_OVERLAP_SWITCH_PERCENTAGE;

        // 1. Set the start margin - margin is applied by updating scrollOffset.
        boolean firstTabIsInGroup =
                mTabGroupModelFilter.isTabInTabGroup(mModel.getTabById(firstTab.getTabId()));
        mScrollDelegate.setReorderStartMargin(firstTabIsInGroup ? marginWidth : 0.f);

        // 2. Set the trailing margin.
        boolean lastTabIsInGroup =
                mTabGroupModelFilter.isTabInTabGroup(mModel.getTabById(lastTab.getTabId()));
        lastTab.setTrailingMargin((lastTabIsInGroup || reorderingForTabDrop) ? marginWidth : 0.f);
    }

    /**
     * Sets the trailing margin for the current tab. Update bottom indicator width for Tab Group
     * Indicators and animates if necessary.
     *
     * @param animationHandler The {@link CompositorAnimationHandler}.
     * @param tab The tab to update.
     * @param groupTitle The group title associated with the tab. Null if tab is not grouped.
     * @param trailingMargin The given tab's new trailing margin.
     * @param animationList The list to add the animation to, or {@code null} if not animating.
     * @return Whether or not the trailing margin for the given tab actually changed.
     */
    boolean setTrailingMarginForTab(
            CompositorAnimationHandler animationHandler,
            StripLayoutTab tab,
            StripLayoutGroupTitle groupTitle,
            float trailingMargin,
            @Nullable List<Animator> animationList) {
        // Avoid triggering updates if trailing margin isn't actually changing.
        if (tab.getTrailingMargin() == trailingMargin) return false;

        // Update group title bottom indicator width if needed.
        if (groupTitle != null) {
            float delta = trailingMargin - tab.getTrailingMargin();
            float startWidth = groupTitle.getBottomIndicatorWidth();
            float endWidth = startWidth + delta;

            if (animationList != null) {
                animationList.add(
                        CompositorAnimator.ofFloatProperty(
                                animationHandler,
                                groupTitle,
                                StripLayoutGroupTitle.BOTTOM_INDICATOR_WIDTH,
                                startWidth,
                                endWidth,
                                ANIM_TAB_SLIDE_OUT_MS));
            } else {
                groupTitle.setBottomIndicatorWidth(endWidth);
            }
        }

        // Set new trailing margin.
        if (animationList != null) {
            animationList.add(
                    CompositorAnimator.ofFloatProperty(
                            animationHandler,
                            tab,
                            StripLayoutTab.TRAILING_MARGIN,
                            tab.getTrailingMargin(),
                            trailingMargin,
                            ANIM_TAB_SLIDE_OUT_MS));
        } else {
            tab.setTrailingMargin(trailingMargin);
        }

        return true;
    }

    /**
     * This method animates a tab being merged to or removed from a tab group.
     *
     * @param animationHost The {@link AnimationHost} to handle animations.
     * @param stripViews The list of {@link StripLayoutView} that are being reordered.
     * @param stripTabs The list of {@link StripLayoutTab} that are being reordered.
     * @param targetGroupTitle The interacting tab's current group title.
     * @param interactingGroupTitle The title of the tab group the tab is dragging past, which
     *     occurs when a tab is being dragged to merge into or move out of the tab group through
     *     group title.
     * @param curIndex The index of the interacting tab.
     * @param effectiveTabWidth The width of a tab, accounting for overlap.
     * @param isMovingOutOfGroup Whether the action is merging/removing a tab to/from a group.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     */
    void animateGroupIndicatorForTabReorder(
            AnimationHost animationHost,
            StripLayoutView[] stripViews,
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle targetGroupTitle,
            StripLayoutGroupTitle interactingGroupTitle,
            int curIndex,
            float effectiveTabWidth,
            boolean isMovingOutOfGroup,
            boolean towardEnd) {
        List<Animator> animators = new ArrayList();

        // Add the group title swapping animation if the tab is merging into or moving out of tab
        // group through group title.
        boolean throughGroupTitle = interactingGroupTitle != null;
        AnimatorListener groupTitleAnimListener = null;
        if (throughGroupTitle) {
            animators.add(
                    getReorderStripViewAnimator(
                            animationHost,
                            stripViews,
                            stripTabs,
                            curIndex,
                            effectiveTabWidth,
                            towardEnd));
            groupTitleAnimListener = getGroupTitleSlidingAnimatorListener();
        }

        // Add bottom indicator animation.
        // TODO(crbug.com/372546700): Disable animations in tests using CompositorAnimationHandler.
        animators.add(
                StripLayoutUtils.getBottomIndicatorAnimatorForMergeOrMoveOutOfGroup(
                        animationHost.getAnimationHandler(),
                        targetGroupTitle,
                        StripLayoutUtils.getNumOfTabsInGroup(
                                mTabGroupModelFilter, targetGroupTitle),
                        effectiveTabWidth,
                        isMovingOutOfGroup,
                        throughGroupTitle));

        animationHost.startAnimations(animators, groupTitleAnimListener);
    }

    /**
     * This method reorders the StripLayoutView when tab drag is interacting with group title.
     *
     * @param animationHost The {@link AnimationHost} to handle animations.
     * @param stripViews The list of {@link StripLayoutView} that are being reordered.
     * @param stripTabs The list of {@link StripLayoutTab} that are being reordered.
     * @param oldIndex The starting index of the reorder.
     * @param effectiveTabWidth The width of a tab, accounting for overlap.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     * @return The animator for the reorder, if any. Null otherwise.
     */
    private Animator getReorderStripViewAnimator(
            AnimationHost animationHost,
            StripLayoutView[] stripViews,
            StripLayoutTab[] stripTabs,
            int oldIndex,
            float effectiveTabWidth,
            boolean towardEnd) {
        // TODO(crbug.com/372546700): Pass in indices, instead of the view/tab arrays.
        int oldIndexInStripView =
                StripLayoutUtils.findStripViewIndexForStripTab(stripViews, stripTabs, oldIndex);
        assert oldIndexInStripView != TabModel.INVALID_TAB_INDEX;

        boolean isLeftMost = oldIndexInStripView == 0;
        boolean isRightMost = oldIndexInStripView >= stripViews.length - 1;

        if (mInteractingTab == null || (isLeftMost && !towardEnd) || (isRightMost && towardEnd)) {
            return null;
        }

        int direction = towardEnd ? 1 : -1;
        int newIndexInStripView = oldIndexInStripView + direction;

        // 1. If the view is already at the right spot, don't do anything.
        // TODO(crbug.com/372546700): Check if ever different from oldIndexInStripView.
        int index = StripLayoutUtils.findIndexForTab(stripTabs, mInteractingTab.getTabId());
        int curIndexInStripView =
                StripLayoutUtils.findStripViewIndexForStripTab(stripViews, stripTabs, index);
        assert curIndexInStripView != TabModel.INVALID_TAB_INDEX;

        if (curIndexInStripView == newIndexInStripView) return null;

        // 2. Check if it's the view we are dragging, but we have an old source index.  Ignore in
        // this case because we probably just already moved it.
        // TODO:(crbug.com/372546700): Dedup this code; We have very similar checks in #reorderTab.
        if (mInReorderMode && curIndexInStripView != oldIndexInStripView) {
            return null;
        }

        CompositorAnimator animator = null;
        // 3. Animate if necessary.
        if (!mAnimationsDisabledForTesting) {
            // TODO(crbug.com/372546700): Fold into #reorderTab. i.e. compute initial positions for
            //  all views after reordering, then animate using offsets to smoothly transition.
            final float animationLength =
                    MathUtils.flipSignIf(
                            direction * effectiveTabWidth, !LocalizationUtils.isLayoutRtl());

            animationHost.finishAnimationsAndPushTabUpdates();

            StripLayoutView slideView = stripViews[newIndexInStripView];
            animator =
                    CompositorAnimator.ofFloatProperty(
                            animationHost.getAnimationHandler(),
                            slideView,
                            StripLayoutView.DRAW_X,
                            slideView.getDrawX(),
                            slideView.getDrawX() + animationLength,
                            ANIM_TAB_MOVE_MS);
        }

        // 4. Swap the views.
        if (!isRightMost && towardEnd) {
            // TODO(crbug.com/372546700): Fold this into the initial calculation.
            newIndexInStripView += 1;
        }
        StripLayoutUtils.moveElement(stripViews, curIndexInStripView, newIndexInStripView);
        return animator;
    }

    private AnimatorListener getGroupTitleSlidingAnimatorListener() {
        return new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mGroupTitleSliding = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mGroupTitleSliding = false;
            }
        };
    }

    /** Disables animations for testing purposes. */
    void disableAnimationsForTesting() {
        mAnimationsDisabledForTesting = true;
    }
}

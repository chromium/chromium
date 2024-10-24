// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.ANIM_TAB_MOVE_MS;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.ANIM_TAB_SLIDE_OUT_MS;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;

import android.animation.Animator;

import androidx.annotation.Nullable;

import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.ArrayList;
import java.util.List;

/** Delegate to manage the reordering logic for the tab strip. */
public class ReorderDelegate {
    // Test State.
    private boolean mAnimationsDisabledForTesting;

    // Tab State.
    private TabGroupModelFilter mTabGroupModelFilter;
    private TabModel mModel;

    // Tab Strip State.
    private ScrollDelegate mScrollDelegate;

    // Internal State.
    private boolean mInitialized;

    // Reorder State
    private boolean mInReorderMode;
    private boolean mReorderingForTabDrop;
    private StripLayoutTab mInteractingTab;

    // ============================================================================================
    // Getters and setters
    // ============================================================================================

    boolean getInReorderMode() {
        return mInReorderMode;
    }

    void setInReorderMode(boolean inReorderMode) {
        mInReorderMode = inReorderMode;
    }

    boolean getReorderingForTabDrop() {
        return mReorderingForTabDrop;
    }

    void setReorderingForTabDrop(boolean reorderingForTabDrop) {
        mReorderingForTabDrop = reorderingForTabDrop;
    }

    StripLayoutTab getInteractingTab() {
        return mInteractingTab;
    }

    void setInteractingTab(StripLayoutTab interactingTab) {
        mInteractingTab = interactingTab;
    }

    // ============================================================================================
    // Initialization
    // ============================================================================================

    /**
     * Passes the dependencies needed in this delegate. Passed here as they aren't ready on
     * instantiation.
     *
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} linked to this delegate.
     * @param scrollDelegate The {@link ScrollDelegate} linked to this delegate.
     */
    void initialize(TabGroupModelFilter tabGroupModelFilter, ScrollDelegate scrollDelegate) {
        mTabGroupModelFilter = tabGroupModelFilter;
        mScrollDelegate = scrollDelegate;

        mModel = mTabGroupModelFilter.getTabModel();
        mInitialized = true;
    }

    // ============================================================================================
    // Margin helpers
    // ============================================================================================

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
     * @param effectiveTabWidth The width of a tab, accounting for overlap.
     * @param trailingMargin The given tab's new trailing margin.
     * @param animationList The list to add the animation to, or {@code null} if not animating.
     * @return Whether or not the trailing margin for the given tab actually changed.
     */
    boolean setTrailingMarginForTab(
            CompositorAnimationHandler animationHandler,
            StripLayoutTab tab,
            StripLayoutGroupTitle groupTitle,
            float effectiveTabWidth,
            float trailingMargin,
            @Nullable List<Animator> animationList) {
        // Avoid triggering updates if trailing margin isn't actually changing.
        if (tab.getTrailingMargin() == trailingMargin) return false;

        // Update group title bottom indicator width if needed.
        if (groupTitle != null) {
            float startWidth = groupTitle.getBottomIndicatorWidth();
            float endWidth =
                    StripLayoutUtils.calculateBottomIndicatorWidth(
                                    groupTitle,
                                    StripLayoutUtils.getNumOfTabsInGroup(
                                            mTabGroupModelFilter, groupTitle),
                                    effectiveTabWidth)
                            + trailingMargin;

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

    // ============================================================================================
    // Tab reorder helpers
    // ============================================================================================

    /**
     * Wrapper for {@link TabGroupModelFilter#moveTabOutOfGroupInDirection} that also records the
     * tab-strip specific User Action.
     */
    void moveTabOutOfGroupInDirection(int tabId, boolean towardEnd) {
        mTabGroupModelFilter.moveTabOutOfGroupInDirection(tabId, towardEnd);
        RecordUserAction.record("MobileToolbarReorderTab.TabRemovedFromGroup");
    }

    /**
     * @param halfTabWidth Half of the effective width of a tab.
     * @param groupTitle The group title for the desired group. Must not be null.
     * @param towardEnd True if dragging towards the end of the strip.
     * @return The threshold to drag out of a group.
     */
    float getDragOutThreshold(
            float halfTabWidth, StripLayoutGroupTitle groupTitle, boolean towardEnd) {
        float dragOutThreshold = halfTabWidth * REORDER_OVERLAP_SWITCH_PERCENTAGE;
        return dragOutThreshold + (towardEnd ? 0 : groupTitle.getWidth());
    }

    /**
     * @param halfTabWidth Half of the effective width of a tab.
     * @return The threshold to drag into a group.
     */
    float getDragInThreshold(float halfTabWidth) {
        return halfTabWidth * REORDER_OVERLAP_SWITCH_PERCENTAGE;
    }

    // ============================================================================================
    // Animation helpers
    // ============================================================================================

    /**
     * Animates a group indicator after a tab has been dragged out of or into its group and the
     * {@link TabGroupModelFilter} has been updated.
     *
     * @param animationHost The {@link AnimationHost} to handle animations.
     * @param groupTitle the group title that is sliding for tab reorder.
     * @param effectiveTabWidth The width of a tab, accounting for overlap.
     * @param isMovingOutOfGroup Whether the action is merging/removing a tab to/from a group.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     */
    void animateGroupIndicatorForTabReorder(
            AnimationHost animationHost,
            StripLayoutGroupTitle groupTitle,
            float effectiveTabWidth,
            boolean isMovingOutOfGroup,
            boolean towardEnd) {
        // TODO(crbug.com/372546700): Disable animations in tests using CompositorAnimationHandler.
        if (mAnimationsDisabledForTesting) return;

        List<Animator> animators = new ArrayList<>();

        // Add the group title swapping animation if the tab is passing through group title.
        boolean throughGroupTitle = isMovingOutOfGroup ^ towardEnd;
        if (throughGroupTitle) {
            // If not animating, no action needed. The group title will have its new position
            // correctly calculated on the next layout pass.
            animators.add(
                    CompositorAnimator.ofFloatProperty(
                            animationHost.getAnimationHandler(),
                            groupTitle,
                            StripLayoutView.X_OFFSET,
                            /* startValue= */ groupTitle.getDrawX() - groupTitle.getIdealX(),
                            /* endValue= */ 0,
                            ANIM_TAB_MOVE_MS));
        }

        // Update bottom indicator width.
        updateBottomIndicatorWidthForTabReorder(
                animationHost.getAnimationHandler(),
                mTabGroupModelFilter,
                groupTitle,
                effectiveTabWidth,
                isMovingOutOfGroup,
                throughGroupTitle,
                animators);

        animationHost.startAnimations(animators, /* listener= */ null);
    }

    /**
     * Set the new bottom indicator width after a tab has been merged to or moved out of a tab
     * group. Animate iff a list of animators is provided.
     *
     * @param animationHandler The {@link CompositorAnimationHandler}.
     * @param modelFilter The {@link TabGroupModelFilter}.
     * @param groupTitle The {@link StripLayoutGroupTitle} of the interacting group.
     * @param effectiveTabWidth The width of a tab, account for overlap.
     * @param isMovingOutOfGroup Whether the action is merging/removing a tab to/from a group.
     * @param throughGroupTitle True if the tab is passing the {@link StripLayoutGroupTitle}.
     * @param animators The list of animators to add to. If {@code null}, then immediately set the
     *     new width instead of animating to it.
     */
    void updateBottomIndicatorWidthForTabReorder(
            CompositorAnimationHandler animationHandler,
            TabGroupModelFilter modelFilter,
            StripLayoutGroupTitle groupTitle,
            float effectiveTabWidth,
            boolean isMovingOutOfGroup,
            boolean throughGroupTitle,
            List<Animator> animators) {
        float endWidth =
                StripLayoutUtils.calculateBottomIndicatorWidth(
                        groupTitle,
                        StripLayoutUtils.getNumOfTabsInGroup(modelFilter, groupTitle),
                        effectiveTabWidth);
        float startWidth = endWidth + MathUtils.flipSignIf(effectiveTabWidth, !isMovingOutOfGroup);

        if (animators != null) {
            animators.add(
                    CompositorAnimator.ofFloatProperty(
                            animationHandler,
                            groupTitle,
                            StripLayoutGroupTitle.BOTTOM_INDICATOR_WIDTH,
                            startWidth,
                            endWidth,
                            throughGroupTitle ? ANIM_TAB_MOVE_MS : ANIM_TAB_SLIDE_OUT_MS));
        } else {
            groupTitle.setBottomIndicatorWidth(endWidth);
        }
    }

    // ============================================================================================
    // Test support
    // ============================================================================================

    /** Disables animations for testing purposes. */
    void disableAnimationsForTesting() {
        mAnimationsDisabledForTesting = true;
    }
}

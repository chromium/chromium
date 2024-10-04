// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.ANIM_TAB_SLIDE_OUT_MS;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;

import android.animation.Animator;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;

import java.util.List;

/** Delegate to manage the reordering logic for the tab strip. */
public class ReorderDelegate {
    // Tab State.
    private TabGroupModelFilter mTabGroupModelFilter;
    private TabModel mModel;

    // Tab Strip State.
    ScrollDelegate mScrollDelegate;

    // Internal state.
    private boolean mInitialized;

    // Reorder state
    boolean mInReorderMode;
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
}

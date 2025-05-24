// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.ANIM_TAB_MOVE_MS;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.TAB_GROUP_BOTTOM_INDICATOR_WIDTH_OFFSET;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.graphics.PointF;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.overlays.strip.AnimationHost;
import org.chromium.chrome.browser.compositor.overlays.strip.ScrollDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.StripUpdateDelegate;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.ArrayList;
import java.util.List;

/** Tab group reorder - drag collapsed or expanded group within the tab strip. */
public class GroupReorderStrategy extends ReorderStrategyBase {
    private StripLayoutGroupTitle mInteractingGroupTitle;
    private final ArrayList<StripLayoutView> mInteractingViews = new ArrayList<>();
    private StripLayoutTab mSelectedTab;
    private StripLayoutTab mFirstTabInGroup;
    private StripLayoutTab mLastTabInGroup;

    GroupReorderStrategy(
            ReorderDelegate reorderDelegate,
            StripUpdateDelegate stripUpdateDelegate,
            AnimationHost animationHost,
            ScrollDelegate scrollDelegate,
            TabModel model,
            TabGroupModelFilter tabGroupModelFilter,
            View containerView,
            ObservableSupplierImpl<Integer> groupIdToHideSupplier,
            Supplier<Float> tabWidthSupplier,
            Supplier<Long> lastReorderScrollTimeSupplier) {
        super(
                reorderDelegate,
                stripUpdateDelegate,
                animationHost,
                scrollDelegate,
                model,
                tabGroupModelFilter,
                containerView,
                groupIdToHideSupplier,
                tabWidthSupplier,
                lastReorderScrollTimeSupplier);
    }

    @Override
    public void startReorderMode(
            StripLayoutView[] stripViews,
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle[] stripGroupTitles,
            @NonNull StripLayoutView interactingView,
            PointF startPoint) {
        // TODO(crbug.com/394945056): Investigate moving to avoid re-emitting when dragging out,
        //  then back onto the source tab strip.
        RecordUserAction.record("MobileToolbarStartReorderGroup");

        // Store the relevant interacting views. We'll update their offsets as we drag.
        mInteractingGroupTitle = (StripLayoutGroupTitle) interactingView;
        mInteractingViews.add(mInteractingGroupTitle);
        List<StripLayoutTab> groupedTabs =
                StripLayoutUtils.getGroupedTabs(
                        mModel, stripTabs, mInteractingGroupTitle.getRootId());
        mFirstTabInGroup = groupedTabs.get(0);
        mLastTabInGroup = groupedTabs.get(groupedTabs.size() - 1);
        mLastTabInGroup.setForceHideEndDivider(/* forceHide= */ true);
        mInteractingViews.addAll(groupedTabs);

        // Foreground the interacting views as they will be dragged over top other views.
        for (StripLayoutView view : mInteractingViews) {
            view.setIsForegrounded(/* isForegrounded= */ true);
        }

        mAnimationHost.finishAnimationsAndPushTabUpdates();
        StripLayoutUtils.performHapticFeedback(mContainerView);

        // If the selected tab is part of the group, lift its container off the toolbar.
        int index = mModel.index();
        if (mModel.getTabAt(index).getRootId() == mInteractingGroupTitle.getRootId()) {
            assert index >= 0 && index < stripTabs.length : "Not synced with TabModel.";
            mSelectedTab = stripTabs[index];
            ArrayList<Animator> animationList = new ArrayList<>();
            updateTabAttachState(mSelectedTab, /* attached= */ false, animationList);
            mAnimationHost.startAnimations(animationList, /* listener= */ null);
        }
    }

    @Override
    public void updateReorderPosition(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            float endX,
            float deltaX,
            int reorderType) {
        float oldIdealX = mInteractingGroupTitle.getIdealX();
        float oldScrollOffset = mScrollDelegate.getScrollOffset();
        float offset = mInteractingGroupTitle.getOffsetX() + deltaX;

        if (reorderGroupIfThresholdReached(groupTitles, stripTabs, offset)) {
            offset =
                    adjustOffsetAfterReorder(
                            mInteractingGroupTitle,
                            offset,
                            deltaX,
                            oldIdealX,
                            oldScrollOffset,
                            /* oldStartMargin= */ 0.f);
        }

        // Clamp the group to the scrollable region. Re-grab the first/last tab index here,
        // since these may have changed as a result of a reorder above.
        int firstTabIndex =
                StripLayoutUtils.findIndexForTab(stripTabs, mFirstTabInGroup.getTabId());
        int lastTabIndex = StripLayoutUtils.findIndexForTab(stripTabs, mLastTabInGroup.getTabId());
        if (firstTabIndex == 0) offset = Math.max(0, offset);
        if (lastTabIndex == stripTabs.length - 1) offset = Math.min(0, offset);
        for (StripLayoutView view : mInteractingViews) {
            view.setOffsetX(offset);
        }
    }

    @Override
    public void stopReorderMode(StripLayoutView[] stripViews, StripLayoutGroupTitle[] groupTitles) {
        // 1. Animate any offsets back to 0 and reattach the selected tab container if needed.
        mAnimationHost.finishAnimationsAndPushTabUpdates();
        ArrayList<Animator> animationList = new ArrayList<>();
        for (StripLayoutView view : mInteractingViews) {
            animationList.add(
                    CompositorAnimator.ofFloatProperty(
                            mAnimationHost.getAnimationHandler(),
                            view,
                            StripLayoutView.X_OFFSET,
                            view.getOffsetX(),
                            0f,
                            ANIM_TAB_MOVE_MS));
        }
        if (mSelectedTab != null) {
            updateTabAttachState(mSelectedTab, /* attached= */ true, animationList);
        }
        mAnimationHost.startAnimations(
                animationList,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        // Clear after the tabs have slid back to their ideal positions, so the
                        // z-indexing is retained during the animation.
                        for (StripLayoutView view : mInteractingViews) {
                            if (view != null) {
                                view.setIsForegrounded(/* isForegrounded= */ false);
                            }
                        }
                        mInteractingViews.clear();

                        if (mLastTabInGroup != null) {
                            mLastTabInGroup.setForceHideEndDivider(/* forceHide= */ false);
                            mLastTabInGroup = null;
                        }
                    }
                });

        // 2. Clear the interacting views now that the animations have been kicked off.
        mInteractingGroupTitle = null;
        mSelectedTab = null;
    }

    @Override
    public StripLayoutView getInteractingView() {
        return mInteractingGroupTitle;
    }

    /**
     * Handles the two different reordering cases:
     *
     * <pre>
     * A] Dragging past an adjacent group.
     * B] Dragging past an adjacent (ungrouped_ tab.
     * </pre>
     *
     * If the group has been dragged past the threshold for the given case, update the {@link
     * TabModel} and return {@code true}. Else, return {@code false}.
     *
     * @param groupTitles The list of {@link StripLayoutGroupTitle}.
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param offset The distance the group has been dragged from its ideal position.
     * @return {@code True} if the reorder was successful. {@code False} otherwise.
     */
    private boolean reorderGroupIfThresholdReached(
            StripLayoutGroupTitle[] groupTitles, StripLayoutTab[] stripTabs, float offset) {
        boolean towardEnd = isOffsetTowardEnd(offset);
        int firstTabIndex =
                StripLayoutUtils.findIndexForTab(stripTabs, mFirstTabInGroup.getTabId());
        int lastTabIndex = StripLayoutUtils.findIndexForTab(stripTabs, mLastTabInGroup.getTabId());

        // Find the tab we're dragging past. Return if none (i.e. dragging towards strip edge).
        int adjTabIndex = towardEnd ? lastTabIndex + 1 : firstTabIndex - 1;
        if (adjTabIndex >= stripTabs.length || adjTabIndex < 0) return false;
        StripLayoutTab adjStripTab = stripTabs[adjTabIndex];
        Tab adjTab = mModel.getTabById(adjStripTab.getTabId());
        assert adjTab != null : "No matching Tab in the TabModel.";

        if (mTabGroupModelFilter.isTabInTabGroup(mModel.getTabById(adjStripTab.getTabId()))) {
            // Case A: Attempt to drag past adjacent group.
            StripLayoutGroupTitle adjTitle =
                    StripLayoutUtils.findGroupTitle(groupTitles, adjTab.getTabGroupId());
            assert adjTitle != null : "No matching group title on the tab strip.";
            if (Math.abs(offset) <= getGroupSwapThreshold(adjTitle)) return false;

            movePastAdjacentGroup(stripTabs, adjTitle, towardEnd);
        } else {
            // Case B: Attempt to drab past ungrouped tab.
            if (Math.abs(offset) <= getTabSwapThreshold()) return false;

            int destIndex = towardEnd ? adjTabIndex + 1 : adjTabIndex;
            mTabGroupModelFilter.moveRelatedTabs(mInteractingGroupTitle.getRootId(), destIndex);
            animateViewSliding(adjStripTab);
        }

        return true;
    }

    /**
     * Reorders the interacting group past an adjacent group. Animates accordingly.
     *
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param adjTitle The {@link StripLayoutGroupTitle} of the adjacent group.
     * @param towardEnd True if dragging towards the end of the strip.
     */
    private void movePastAdjacentGroup(
            StripLayoutTab[] stripTabs, StripLayoutGroupTitle adjTitle, boolean towardEnd) {
        // Move the interacting group to its new position.
        List<Tab> adjTabs = mTabGroupModelFilter.getRelatedTabList(adjTitle.getRootId());
        int indexTowardStart = TabGroupUtils.getFirstTabModelIndexForList(mModel, adjTabs);
        int indexTowardEnd = TabGroupUtils.getLastTabModelIndexForList(mModel, adjTabs) + 1;
        int destIndex = towardEnd ? indexTowardEnd : indexTowardStart;
        mTabGroupModelFilter.moveRelatedTabs(mInteractingGroupTitle.getRootId(), destIndex);

        // Animate the displaced views sliding to their new positions.
        List<Animator> animators = new ArrayList<>();
        if (!adjTitle.isCollapsed()) {
            // Only need to animate tabs when expanded.
            int start = TabGroupUtils.getFirstTabModelIndexForList(mModel, adjTabs);
            int end = start + adjTabs.size();
            for (int i = start; i < end; i++) {
                animators.add(getViewSlidingAnimator(stripTabs[i]));
            }
        }
        animators.add(getViewSlidingAnimator(adjTitle));
        mAnimationHost.startAnimations(animators, /* listener= */ null);
    }

    /**
     * @param adjTitle The {@link StripLayoutGroupTitle} of the group we're dragging past.
     * @return The threshold needed to trigger a reorder.
     */
    private float getGroupSwapThreshold(StripLayoutGroupTitle adjTitle) {
        if (adjTitle.isCollapsed()) {
            return adjTitle.getWidth() * StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;
        }
        return (adjTitle.getBottomIndicatorWidth() + TAB_GROUP_BOTTOM_INDICATOR_WIDTH_OFFSET)
                * StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;
    }
}

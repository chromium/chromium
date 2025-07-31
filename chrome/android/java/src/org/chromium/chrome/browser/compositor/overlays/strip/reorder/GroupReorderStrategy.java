// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.graphics.PointF;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.MathUtils;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.overlays.strip.AnimationHost;
import org.chromium.chrome.browser.compositor.overlays.strip.ScrollDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTabDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.StripUpdateDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
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
            ObservableSupplierImpl<Token> groupIdToHideSupplier,
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
                        mModel, stripTabs, mInteractingGroupTitle.getTabGroupId());
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
        if (mInteractingGroupTitle.getTabGroupId().equals(mModel.getTabAt(index).getTabGroupId())) {
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
        ArrayList<Animator> animationList = new ArrayList<>();
        Runnable onAnimationEnd =
                () -> {
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

                    // Clear the interacting views now that the animations have been kicked off.
                    mInteractingGroupTitle = null;
                    mSelectedTab = null;
                };
        handleStopReorderMode(
                stripViews,
                groupTitles,
                mInteractingViews,
                mSelectedTab,
                animationList,
                onAnimationEnd);
    }

    @Override
    public StripLayoutView getInteractingView() {
        return mInteractingGroupTitle;
    }

    @Override
    public void reorderViewInDirection(
            StripLayoutTabDelegate tabDelegate,
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            StripLayoutView reorderingView,
            boolean toLeft) {
        // Cast to the correct view type.
        assert reorderingView instanceof StripLayoutGroupTitle
                : "Using incorrect ReorderStrategy for view type.";
        StripLayoutGroupTitle reorderingGroupTitle = (StripLayoutGroupTitle) reorderingView;

        // Grab the relevant StripLayoutTabs for the given group.
        List<StripLayoutTab> groupedTabs =
                StripLayoutUtils.getGroupedTabs(
                        mModel, stripTabs, reorderingGroupTitle.getTabGroupId());
        StripLayoutTab firstTabInGroup = groupedTabs.get(0);
        StripLayoutTab lastTabInGroup = groupedTabs.get(groupedTabs.size() - 1);

        // Fake a successful reorder in the target direction.
        float offset = MathUtils.flipSignIf(Float.MAX_VALUE, toLeft);
        reorderGroupIfThresholdReached(
                groupTitles,
                stripTabs,
                offset,
                firstTabInGroup,
                lastTabInGroup,
                reorderingGroupTitle.getTabGroupId());

        // Animate the reordering view. Ensure all the views are foregrounded.
        reorderingView.setIsForegrounded(/* isForegrounded= */ true);
        for (StripLayoutTab tab : groupedTabs) {
            tab.setIsForegrounded(/* isForegrounded= */ true);
            animateViewSliding(tab);
        }
        animateViewSliding(
                reorderingView,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        reorderingView.setIsForegrounded(/* isForegrounded= */ false);
                        for (StripLayoutTab tab : groupedTabs) {
                            tab.setIsForegrounded(/* isForegrounded= */ false);
                        }
                    }
                });
    }

    /**
     * See {@link #reorderGroupIfThresholdReached(StripLayoutGroupTitle[], StripLayoutTab[], float,
     * StripLayoutTab, StripLayoutTab, Token)}
     */
    private boolean reorderGroupIfThresholdReached(
            StripLayoutGroupTitle[] groupTitles, StripLayoutTab[] stripTabs, float offset) {
        return reorderGroupIfThresholdReached(
                groupTitles,
                stripTabs,
                offset,
                mFirstTabInGroup,
                mLastTabInGroup,
                mInteractingGroupTitle.getTabGroupId());
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
     * @param firstTabInGroup The first {@link StripLayoutTab} in the interacting group.
     * @param lastTabInGroup The last {@link StripLayoutTab} in the interacting group.
     * @param interactingTabGroupId The tab group ID of the interacting group.
     * @return {@code True} if the reorder was successful. {@code False} otherwise.
     */
    private boolean reorderGroupIfThresholdReached(
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            float offset,
            StripLayoutTab firstTabInGroup,
            StripLayoutTab lastTabInGroup,
            Token interactingTabGroupId) {
        boolean towardEnd = isOffsetTowardEnd(offset);
        int firstTabIndex = StripLayoutUtils.findIndexForTab(stripTabs, firstTabInGroup.getTabId());
        int lastTabIndex = StripLayoutUtils.findIndexForTab(stripTabs, lastTabInGroup.getTabId());

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

            movePastAdjacentGroup(stripTabs, interactingTabGroupId, adjTitle, towardEnd);
        } else {
            // Case B: Attempt to drab past ungrouped tab.
            if (Math.abs(offset) <= getTabSwapThreshold()) return false;

            @TabId int tabId = mTabGroupModelFilter.getGroupLastShownTabId(interactingTabGroupId);
            mTabGroupModelFilter.moveRelatedTabs(tabId, adjTabIndex);
            animateViewSliding(adjStripTab);
        }

        return true;
    }

    /**
     * Reorders the interacting group past an adjacent group. Animates accordingly.
     *
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param interactingTabGroupId The tab group ID of the interacting group.
     * @param adjTitle The {@link StripLayoutGroupTitle} of the adjacent group.
     * @param towardEnd True if dragging towards the end of the strip.
     */
    private void movePastAdjacentGroup(
            StripLayoutTab[] stripTabs,
            Token interactingTabGroupId,
            StripLayoutGroupTitle adjTitle,
            boolean towardEnd) {
        // Move the interacting group to its new position.
        List<Tab> adjTabs = mTabGroupModelFilter.getTabsInGroup(adjTitle.getTabGroupId());
        int indexTowardStart = TabGroupUtils.getFirstTabModelIndexForList(mModel, adjTabs);
        int indexTowardEnd = TabGroupUtils.getLastTabModelIndexForList(mModel, adjTabs);
        int destIndex = towardEnd ? indexTowardEnd : indexTowardStart;
        @TabId int tabId = mTabGroupModelFilter.getGroupLastShownTabId(interactingTabGroupId);
        mTabGroupModelFilter.moveRelatedTabs(tabId, destIndex);

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
        mAnimationHost.queueAnimations(animators, /* listener= */ null);
    }
}

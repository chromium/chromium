// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.graphics.PointF;
import android.view.View;

import org.chromium.base.MathUtils;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
import org.chromium.ui.base.LocalizationUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Tab group reorder - drag collapsed or expanded group within the tab strip. */
@NullMarked
public class GroupReorderStrategy extends ReorderStrategyBase {
    private @Nullable StripLayoutGroupTitle mInteractingGroupTitle;
    private final ArrayList<StripLayoutView> mInteractingViews = new ArrayList<>();
    private @Nullable StripLayoutTab mSelectedTab;
    private @Nullable StripLayoutTab mFirstTabInGroup;
    private @Nullable StripLayoutTab mLastTabInGroup;

    GroupReorderStrategy(
            ReorderDelegate reorderDelegate,
            StripUpdateDelegate stripUpdateDelegate,
            AnimationHost animationHost,
            ScrollDelegate scrollDelegate,
            TabModel model,
            TabGroupModelFilter tabGroupModelFilter,
            View containerView,
            ObservableSupplierImpl<@Nullable Token> groupIdToHideSupplier,
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
            StripLayoutView interactingView,
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
        if (mInteractingGroupTitle
                .getTabGroupId()
                .equals(mModel.getTabAtChecked(index).getTabGroupId())) {
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
        assumeNonNull(mInteractingGroupTitle);
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
        assumeNonNull(mFirstTabInGroup);
        assumeNonNull(mLastTabInGroup);
        int firstTabIndex =
                StripLayoutUtils.findIndexForTab(stripTabs, mFirstTabInGroup.getTabId());
        int lastTabIndex = StripLayoutUtils.findIndexForTab(stripTabs, mLastTabInGroup.getTabId());

        // Case 1. if the group is at the strip's edge(first or last position) trim the x-offset
        // based on the relevant margin(e.g. start, trailing, or group drag out threshold).
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        if (firstTabIndex == 0 || lastTabIndex == stripTabs.length - 1) {
            if (firstTabIndex == 0) {
                offset = isRtl ? Math.min(0, offset) : Math.max(0, offset);
            }
            if (lastTabIndex == stripTabs.length - 1) {
                offset = isRtl ? Math.max(0, offset) : Math.min(0, offset);
            }
        } else {
            // Case 2. If the tab strip has both pinned and unpinned tabs, clamp the offset when
            // dragging the group toward the start. The limit is determined by the boundary of the
            // first view on the tab strip.
            StripLayoutView firstView = stripViews[0];
            boolean firstTabPinned =
                    firstView instanceof StripLayoutTab firstTab && firstTab.getIsPinned();
            if (firstTabPinned && !isOffsetTowardEnd(offset)) {
                float limit = getDragOffsetLimit(mInteractingViews.get(0), firstView, offset > 0);
                offset = isRtl ? Math.min(limit, offset) : Math.max(limit, offset);
            }
        }
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
    public @Nullable StripLayoutView getInteractingView() {
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
        reorderingView.setIsNonDragReordering(/* isNonDragReordering= */ true);
        for (StripLayoutTab tab : groupedTabs) {
            tab.setIsForegrounded(/* isForegrounded= */ true);
            tabDelegate.setIsTabNonDragReordering(tab, /* isNonDragReordering= */ true);
            animateViewSliding(tab);
        }
        animateViewSliding(
                reorderingView,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        reorderingView.setIsForegrounded(/* isForegrounded= */ false);
                        reorderingView.setIsNonDragReordering(/* isNonDragReordering= */ false);
                        for (StripLayoutTab tab : groupedTabs) {
                            tab.setIsForegrounded(/* isForegrounded= */ false);
                            tabDelegate.setIsTabNonDragReordering(
                                    tab, /* isNonDragReordering= */ false);
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
                assumeNonNull(mFirstTabInGroup),
                assumeNonNull(mLastTabInGroup),
                assumeNonNull(mInteractingGroupTitle).getTabGroupId());
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

        if (adjTab.getIsPinned()) return false;

        if (mTabGroupModelFilter.isTabInTabGroup(
                mModel.getTabByIdChecked(adjStripTab.getTabId()))) {
            // Case A: Attempt to drag past adjacent group.
            StripLayoutGroupTitle adjTitle =
                    StripLayoutUtils.findGroupTitle(groupTitles, adjTab.getTabGroupId());
            assert adjTitle != null : "No matching group title on the tab strip.";
            if (Math.abs(offset) <= getGroupSwapThreshold(adjTitle)) return false;

            movePastAdjacentGroup(stripTabs, interactingTabGroupId, adjTitle, towardEnd);
        } else {
            // Case B: Attempt to drab past ungrouped tab.
            if (Math.abs(offset) <= getTabSwapThreshold(/* isPinned= */ false)) return false;

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
            for (Tab tab : adjTabs) {
                // Find using the IDs, since the provided stripTabs may be stale.
                StripLayoutTab stripTab = StripLayoutUtils.findTabById(stripTabs, tab.getId());
                if (stripTab != null) animators.add(getViewSlidingAnimator(stripTab));
            }
        }
        animators.add(getViewSlidingAnimator(adjTitle));
        mAnimationHost.queueAnimations(animators, /* listener= */ null);
    }
}

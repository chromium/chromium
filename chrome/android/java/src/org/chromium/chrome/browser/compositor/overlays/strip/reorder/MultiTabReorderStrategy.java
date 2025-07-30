// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.graphics.PointF;
import android.view.View;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.AnimationHost;
import org.chromium.chrome.browser.compositor.overlays.strip.ScrollDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.StripTabModelActionListener;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.StripUpdateDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;

/**
 * A reorder strategy for handling the dragging of multiple selected tabs as a single contiguous
 * block within the tab strip.
 */
@NullMarked
public class MultiTabReorderStrategy extends ReorderStrategyBase {
    private final List<StripLayoutTab> mInteractingTabs = new ArrayList<>();
    private final HashSet<Integer> mInteractingTabIds = new HashSet<>();
    @Nullable private StripLayoutTab mPrimaryInteractingStripTab;
    @Nullable private StripLayoutTab mFirstTabInBlock;
    @Nullable private StripLayoutTab mLastTabInBlock;
    private final Supplier<Boolean> mInReorderModeSupplier;

    /**
     * Constructs a new reorder strategy for handling a multi-tab drag selection.
     *
     * @param reorderDelegate The {@link ReorderDelegate} to handle reorder operations.
     * @param stripUpdateDelegate The {@link StripUpdateDelegate} for strip UI updates.
     * @param animationHost The {@link AnimationHost} for running animations.
     * @param scrollDelegate The {@link ScrollDelegate} for handling strip scrolling.
     * @param model The {@link TabModel} for tab data.
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} for group operations.
     * @param containerView The container {@link View}.
     * @param groupIdToHideSupplier The supplier for the group ID to hide.
     * @param tabWidthSupplier The supplier for the tab width.
     * @param lastReorderScrollTimeSupplier The supplier for the last reorder scroll time.
     * @param inReorderModeSupplier The supplier to check if reorder mode is active.
     */
    MultiTabReorderStrategy(
            ReorderDelegate reorderDelegate,
            StripUpdateDelegate stripUpdateDelegate,
            AnimationHost animationHost,
            ScrollDelegate scrollDelegate,
            TabModel model,
            TabGroupModelFilter tabGroupModelFilter,
            View containerView,
            ObservableSupplierImpl<Token> groupIdToHideSupplier,
            Supplier<Float> tabWidthSupplier,
            Supplier<Long> lastReorderScrollTimeSupplier,
            Supplier<Boolean> inReorderModeSupplier) {
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
        mInReorderModeSupplier = inReorderModeSupplier;
    }

    @Override
    public void startReorderMode(
            StripLayoutView[] stripViews,
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle[] stripGroupTitles,
            StripLayoutView interactingView,
            PointF startPoint) {
        mPrimaryInteractingStripTab = (StripLayoutTab) interactingView;
        Tab primaryTab = mModel.getTabById(mPrimaryInteractingStripTab.getTabId());
        if (primaryTab == null) return;

        TabModelUtils.setIndex(mModel, mModel.indexOf(primaryTab));

        List<Tab> selectedTabs = getSortedSelectedTabs(stripTabs);
        for (Tab tab : selectedTabs) {
            int tabId = tab.getId();
            mInteractingTabs.add(StripLayoutUtils.findTabById(stripTabs, tabId));
            mInteractingTabIds.add(tabId);
        }
        mFirstTabInBlock = mInteractingTabs.get(0);
        mLastTabInBlock = mInteractingTabs.get(mInteractingTabs.size() - 1);

        boolean isPrimaryTabInGroup = mTabGroupModelFilter.isTabInTabGroup(primaryTab);
        boolean notAllTabsInPrimaryGroupAreSelected = false;
        List<Tab> tabsInGroup = mTabGroupModelFilter.getTabsInGroup(primaryTab.getTabGroupId());
        for (Tab tab : tabsInGroup) {
            if (!mInteractingTabIds.contains(tab.getId())) {
                notAllTabsInPrimaryGroupAreSelected = true;
                break;
            }
        }

        if (isPrimaryTabInGroup && notAllTabsInPrimaryGroupAreSelected) {
            Token destinationGroupId = primaryTab.getTabGroupId();
            assert destinationGroupId != null;
            int primaryTabIndexInGroup = tabsInGroup.indexOf(primaryTab);
            mTabGroupModelFilter.mergeListOfTabsToGroup(
                    selectedTabs,
                    primaryTab,
                    /* indexInGroup= */ primaryTabIndexInGroup,
                    /* notify= */ false);
        } else {
            ungroupInteractingBlock();
            int primaryTabModelIndex = mModel.indexOf(primaryTab);
            int primaryTabIndexInSelection = selectedTabs.indexOf(primaryTab);

            int targetGatherIndex = primaryTabModelIndex - primaryTabIndexInSelection;
            for (int i = 0; i <= selectedTabs.size() - 1; i++) {
                Tab tab = selectedTabs.get(i);
                int currentTabModelIndex = mModel.indexOf(tab);
                if (currentTabModelIndex > targetGatherIndex) {
                    targetGatherIndex++;
                } else if (currentTabModelIndex == targetGatherIndex) {
                    continue;
                }
                mModel.moveTab(selectedTabs.get(i).getId(), targetGatherIndex);
            }
        }

        for (StripLayoutView view : mInteractingTabs) {
            view.setIsForegrounded(/* isForegrounded= */ true);
        }

        // TODO(crbug.com/434911413): Gather step requires animation.
        mAnimationHost.finishAnimationsAndPushTabUpdates();
        setEdgeMarginsForReorder(stripTabs);

        ArrayList<Animator> animationList = new ArrayList<>();
        updateTabAttachState(mPrimaryInteractingStripTab, /* attached= */ false, animationList);
        StripLayoutUtils.performHapticFeedback(mContainerView);
        mAnimationHost.startAnimations(animationList, /* listener= */ null);
    }

    @Override
    public void updateReorderPosition(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            float endX,
            float deltaX,
            @ReorderType int reorderType) {
        assert !mInteractingTabs.isEmpty();
        assert mPrimaryInteractingStripTab != null;

        int curIndex =
                StripLayoutUtils.findIndexForTab(stripTabs, mPrimaryInteractingStripTab.getTabId());
        if (curIndex == TabModel.INVALID_TAB_INDEX) return;

        float oldIdealX = mPrimaryInteractingStripTab.getIdealX();
        float oldScrollOffset = mScrollDelegate.getScrollOffset();
        float oldStartMargin = mScrollDelegate.getReorderStartMargin();
        float offset = mPrimaryInteractingStripTab.getOffsetX() + deltaX;

        if (reorderBlockIfThresholdReached(stripViews, groupTitles, stripTabs, offset)) {
            if (!Boolean.TRUE.equals(mInReorderModeSupplier.get())) return;
            setEdgeMarginsForReorder(stripTabs);

            offset =
                    adjustOffsetAfterReorder(
                            mPrimaryInteractingStripTab,
                            offset,
                            deltaX,
                            oldIdealX,
                            oldScrollOffset,
                            oldStartMargin);
        }

        StripLayoutTab firstTabInBlock = mFirstTabInBlock;
        StripLayoutTab lastTabInBlock = mLastTabInBlock;
        if (firstTabInBlock == null || lastTabInBlock == null) return;

        int firstTabIndex = StripLayoutUtils.findIndexForTab(stripTabs, firstTabInBlock.getTabId());
        int lastTabIndex = StripLayoutUtils.findIndexForTab(stripTabs, lastTabInBlock.getTabId());
        boolean isRtl = org.chromium.ui.base.LocalizationUtils.isLayoutRtl();
        if (firstTabIndex == 0) {
            float limit =
                    (stripViews[0] instanceof StripLayoutGroupTitle groupTitle)
                            ? getDragOutThreshold(groupTitle, /* towardEnd= */ false)
                            : mScrollDelegate.getReorderStartMargin();
            offset = isRtl ? Math.min(limit, offset) : Math.max(-limit, offset);
        }
        if (lastTabIndex == stripTabs.length - 1) {
            offset =
                    isRtl
                            ? Math.max(-lastTabInBlock.getTrailingMargin(), offset)
                            : Math.min(lastTabInBlock.getTrailingMargin(), offset);
        }

        for (StripLayoutTab tab : mInteractingTabs) {
            tab.setOffsetX(offset);
        }
    }

    /**
     * Determines if the drag offset has surpassed a reorder threshold and executes the reorder if
     * it has. A reorder can be a simple tab swap, moving past a group, merging into a group, or
     * dragging out of a group.
     *
     * @param stripViews All views currently in the strip.
     * @param groupTitles All group title views in the strip.
     * @param stripTabs All tab views in the strip.
     * @param offset The current drag offset from the tab's ideal position.
     * @return True if a reorder occurred, false otherwise.
     */
    private boolean reorderBlockIfThresholdReached(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            float offset) {
        assert mPrimaryInteractingStripTab != null;

        boolean towardEnd = isOffsetTowardEnd(offset);
        Tab primaryTab = mModel.getTabById(mPrimaryInteractingStripTab.getTabId());
        if (primaryTab == null) return false;

        StripLayoutTab edgeTabInBlock = towardEnd ? mLastTabInBlock : mFirstTabInBlock;
        if (edgeTabInBlock == null) return false;
        int edgeTabIndexInStrip =
                StripLayoutUtils.findIndexForTab(stripTabs, edgeTabInBlock.getTabId());

        int adjTabIndex = towardEnd ? edgeTabIndexInStrip + 1 : edgeTabIndexInStrip - 1;
        Tab adjTab = mModel.getTabAt(adjTabIndex);

        boolean isInGroup = mTabGroupModelFilter.isTabInTabGroup(primaryTab);
        boolean mayDragInOrOutOfGroup =
                adjTab == null
                        ? isInGroup
                        : StripLayoutUtils.notRelatedAndEitherTabInGroup(
                                mTabGroupModelFilter, primaryTab, adjTab);

        if (adjTab != null && mInteractingTabIds.contains(adjTab.getId())) return false;

        // Not interacting with tab groups.
        if (!mayDragInOrOutOfGroup) {
            if (adjTab == null || Math.abs(offset) <= getTabSwapThreshold()) return false;
            moveAdjacentTabPastBlock(adjTab, towardEnd);
            animateViewSliding(StripLayoutUtils.findTabById(stripTabs, adjTab.getId()));
            return true;
        }

        // Maybe drag out of group.
        if (isInGroup) {
            StripLayoutGroupTitle interactingGroupTitle =
                    StripLayoutUtils.findGroupTitle(groupTitles, primaryTab.getTabGroupId());
            float threshold = getDragOutThreshold(interactingGroupTitle, towardEnd);
            if (Math.abs(offset) <= threshold) return false;
            List<StripLayoutTab> interactingTabs = new ArrayList<>(mInteractingTabs);
            if (towardEnd) Collections.reverse(interactingTabs);
            moveInteractingTabsOutOfGroup(
                    stripViews,
                    groupTitles,
                    interactingTabs,
                    interactingGroupTitle,
                    towardEnd,
                    StripTabModelActionListener.ActionType.REORDER);
            return true;
        }

        StripLayoutGroupTitle interactingGroupTitle =
                StripLayoutUtils.findGroupTitle(groupTitles, assumeNonNull(adjTab).getTabGroupId());

        if (interactingGroupTitle == null) return false;

        if (interactingGroupTitle.isCollapsed()) {
            // Maybe drag past collapsed group.
            float threshold = getGroupSwapThreshold(interactingGroupTitle);
            if (Math.abs(offset) <= threshold) return false;

            moveAdjacentGroupPastBlock(interactingGroupTitle, towardEnd);
            animateViewSliding(interactingGroupTitle);
            return true;
        } else {
            // Maybe merge to group.
            if (Math.abs(offset) <= getDragInThreshold()) return false;
            mergeBlockIntoGroup(stripTabs, adjTab, interactingGroupTitle, towardEnd);
            return true;
        }
    }

    @Override
    public void stopReorderMode(StripLayoutView[] stripViews, StripLayoutGroupTitle[] groupTitles) {
        mAnimationHost.finishAnimationsAndPushTabUpdates();
        ArrayList<Animator> animationList = new ArrayList<>();
        Runnable onAnimationEnd =
                () -> {
                    for (StripLayoutTab tab : mInteractingTabs) {
                        if (tab != null) {
                            tab.setIsForegrounded(false);
                        }
                    }
                    mInteractingTabs.clear();
                    mInteractingTabIds.clear();
                    mPrimaryInteractingStripTab = null;
                };
        List<StripLayoutView> interactingViews = new ArrayList<>(mInteractingTabs);
        handleStopReorderMode(
                stripViews,
                groupTitles,
                interactingViews,
                mPrimaryInteractingStripTab,
                animationList,
                onAnimationEnd);
    }

    @Override
    public @Nullable StripLayoutView getInteractingView() {
        return mPrimaryInteractingStripTab;
    }

    /**
     * Moves a single adjacent tab to the other side of the interacting block of tabs.
     *
     * @param adjTab The adjacent tab to move.
     * @param towardEnd True if the drag is toward the end of the strip (RTL: left, LTR: right).
     */
    private void moveAdjacentTabPastBlock(Tab adjTab, boolean towardEnd) {
        int destinationIndex = getDestinationIndex(towardEnd);
        mModel.moveTab(adjTab.getId(), destinationIndex);
    }

    /**
     * Moves an entire adjacent tab group to the other side of the interacting block of tabs.
     *
     * @param adjGroupTitle The title of the group to move.
     * @param towardEnd True if the drag is toward the end of the strip (RTL: left, LTR: right).
     */
    private void moveAdjacentGroupPastBlock(
            StripLayoutGroupTitle adjGroupTitle, boolean towardEnd) {
        int destinationIndex = getDestinationIndex(towardEnd);
        Tab firstTabInAdjGroup =
                mTabGroupModelFilter.getTabsInGroup(adjGroupTitle.getTabGroupId()).get(0);
        mTabGroupModelFilter.moveRelatedTabs(firstTabInAdjGroup.getId(), destinationIndex);
    }

    /**
     * Calculates the destination index in the {@link TabModel} for an item being moved past the
     * interacting block.
     *
     * @param towardEnd True if the drag is toward the end of the strip (RTL: left, LTR: right).
     * @return The model index where the item should be moved.
     */
    private int getDestinationIndex(boolean towardEnd) {
        int destinationIndex;
        if (towardEnd) {
            // The block is moving toward the end, so move the adjacent item to the start of the
            // block.
            assert mFirstTabInBlock != null;
            StripLayoutTab firstTabInBlock = mFirstTabInBlock;
            destinationIndex = mModel.indexOf(mModel.getTabById(firstTabInBlock.getTabId()));
        } else {
            // The block is moving toward the start, so move the adjacent item to the end of the
            // block.
            assert mLastTabInBlock != null;
            StripLayoutTab lastTabInBlock = mLastTabInBlock;
            destinationIndex = mModel.indexOf(mModel.getTabById(lastTabInBlock.getTabId()));
        }
        return destinationIndex;
    }

    /**
     * Merges all tabs in the interacting block into an adjacent, expanded tab group.
     *
     * @param stripTabs The array of tab views on the strip.
     * @param adjTab The adjacent tab, which is part of the destination group.
     * @param adjTitle The title view of the destination group.
     * @param towardEnd True if the drag is toward the end of the strip (RTL: left, LTR: right),
     *     which determines if the block is merged to the front or back of the group.
     */
    private void mergeBlockIntoGroup(
            StripLayoutTab[] stripTabs,
            Tab adjTab,
            StripLayoutGroupTitle adjTitle,
            boolean towardEnd) {
        mTabGroupModelFilter.mergeListOfTabsToGroup(
                getSortedSelectedTabs(stripTabs),
                adjTab,
                /* indexInGroup= */ towardEnd ? 0 : null,
                /* notify= */ false);
        animateGroupIndicatorForTabReorder(adjTitle, /* isMovingOutOfGroup= */ false, towardEnd);
    }

    /**
     * Gets a list of {@link Tab} objects that are currently selected, sorted by their visual order
     * on the tab strip.
     *
     * @param stripTabs The array of tab views on the strip, used to determine visual order.
     * @return A sorted {@link List} of selected {@link Tab}s.
     */
    private List<Tab> getSortedSelectedTabs(StripLayoutTab[] stripTabs) {
        List<Tab> sortedTabs = new ArrayList<>();
        for (StripLayoutTab stripTab : stripTabs) {
            if (stripTab != null && mModel.isTabMultiSelected(stripTab.getTabId())) {
                sortedTabs.add(mModel.getTabById(stripTab.getTabId()));
            }
        }
        return sortedTabs;
    }

    /** Ungroups all tabs currently in the interacting block. */
    private void ungroupInteractingBlock() {
        List<Tab> tabsToUngroup = new ArrayList<>();
        for (StripLayoutTab slt : mInteractingTabs) {
            Tab tab = mModel.getTabById(slt.getTabId());
            assert tab != null;
            if (mTabGroupModelFilter.isTabInTabGroup(tab)) tabsToUngroup.add(tab);
        }
        mTabGroupModelFilter
                .getTabUngrouper()
                .ungroupTabs(tabsToUngroup, /* trailing= */ false, /* allowDialog= */ false, null);
    }
}

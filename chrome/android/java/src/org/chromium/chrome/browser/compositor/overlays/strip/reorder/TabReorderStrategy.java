// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.graphics.PointF;
import android.view.View;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.overlays.strip.AnimationHost;
import org.chromium.chrome.browser.compositor.overlays.strip.ScrollDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.StripTabModelActionListener.ActionType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.StripUpdateDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.ui.base.LocalizationUtils;

import java.util.ArrayList;
import java.util.List;

/** Tab reorder - drag tab past other tabs or into/out of groups within the tab strip. */
public class TabReorderStrategy extends ReorderStrategyBase {
    // Tab being reordered.
    private StripLayoutTab mInteractingTab;

    // Dependencies
    private final Supplier<Boolean> mInReorderModeSupplier;

    TabReorderStrategy(
            ReorderDelegate reorderDelegate,
            StripUpdateDelegate stripUpdateDelegate,
            AnimationHost animationHost,
            ScrollDelegate scrollDelegate,
            TabModel model,
            TabGroupModelFilter tabGroupModelFilter,
            View containerView,
            ObservableSupplierImpl<Integer> groupIdToHideSupplier,
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

    /** See {@link ReorderStrategy#startReorderMode} */
    @Override
    public void startReorderMode(
            StripLayoutView[] stripViews,
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle[] stripGroupTitles,
            StripLayoutView interactingView,
            PointF startPoint) {
        // TODO(crbug.com/394945056): Investigate moving to avoid re-emitting when dragging out,
        //  then back onto the source tab strip.
        RecordUserAction.record("MobileToolbarStartReorderTab");
        mInteractingTab = (StripLayoutTab) interactingView;
        interactingView.setIsForegrounded(/* isForegrounded= */ true);

        // 1. Select this tab so that it is always in the foreground.
        TabModelUtils.setIndex(
                mModel, TabModelUtils.getTabIndexById(mModel, mInteractingTab.getTabId()));

        // 2. Set initial state and add edge margins.
        mAnimationHost.finishAnimationsAndPushTabUpdates();
        setEdgeMarginsForReorder(stripTabs);

        // 3. Lift the container off the toolbar and perform haptic feedback.
        ArrayList<Animator> animationList = new ArrayList<>();
        updateTabAttachState(mInteractingTab, /* attached= */ false, animationList);
        StripLayoutUtils.performHapticFeedback(mContainerView);

        // 4. Kick-off animations.
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
        // 1. Return if interacting tab is no longer part of strip tabs.
        int curIndex = StripLayoutUtils.findIndexForTab(stripTabs, mInteractingTab.getTabId());
        if (curIndex == TabModel.INVALID_TAB_INDEX) return;

        // 2. Compute drag position.
        float oldIdealX = mInteractingTab.getIdealX();
        float oldScrollOffset = mScrollDelegate.getScrollOffset();
        float oldStartMargin = mScrollDelegate.getReorderStartMargin();
        float offset = mInteractingTab.getOffsetX() + deltaX;

        // 3. Attempt to move the tab. If successful, update other relevant properties.
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        if (reorderTabIfThresholdReached(
                stripViews, groupTitles, stripTabs, mInteractingTab, offset, curIndex)) {
            // 3.a. We may have exited reorder mode to display the confirmation dialog. If so,
            // we should not set the new offset here, and instead let the tab slide back to its
            // idealX.
            if (!Boolean.TRUE.equals(mInReorderModeSupplier.get())) return;

            // 3.b. Update the edge margins, since we may have merged/removed an edge tab
            // to/from a group.
            setEdgeMarginsForReorder(stripTabs);

            // 3.c. Adjust the drag offset to prevent any apparent movement.
            offset =
                    adjustOffsetAfterReorder(
                            mInteractingTab,
                            offset,
                            deltaX,
                            oldIdealX,
                            oldScrollOffset,
                            oldStartMargin);
        }

        // 4. Limit offset based on tab position. First tab can't drag left, last tab can't drag
        // right. If either is grouped, we allot additional drag distance to allow for dragging
        // out of a group toward the edge of the strip.
        // TODO(crbug.com/331854162): Refactor to set mStripStartMarginForReorder and the final
        //  tab's trailing margin.
        int newIndex = StripLayoutUtils.findIndexForTab(stripTabs, mInteractingTab.getTabId());
        if (newIndex == 0) {
            float limit =
                    (stripViews[0] instanceof StripLayoutGroupTitle groupTitle)
                            ? getDragOutThreshold(groupTitle, /* towardEnd= */ false)
                            : mScrollDelegate.getReorderStartMargin();
            offset = isRtl ? Math.min(limit, offset) : Math.max(-limit, offset);
        }
        if (newIndex == stripTabs.length - 1) {
            float limit = stripTabs[newIndex].getTrailingMargin();
            offset = isRtl ? Math.max(-limit, offset) : Math.min(limit, offset);
        }
        mInteractingTab.setOffsetX(offset);
    }

    @Override
    public void stopReorderMode(StripLayoutView[] stripViews, StripLayoutGroupTitle[] groupTitles) {
        List<Animator> animatorList = new ArrayList<>();
        handleStopReorderMode(stripViews, groupTitles, mInteractingTab, animatorList);
        // Start animations. Reset foregrounded state after the tabs have slid back to their
        // ideal positions, so the z-indexing is retained during the animation.
        mAnimationHost.startAnimations(
                animatorList,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        if (mInteractingTab != null) {
                            mInteractingTab.setIsForegrounded(/* isForegrounded= */ false);
                            mInteractingTab = null;
                        }
                    }
                });
    }

    @Override
    public StripLayoutView getInteractingView() {
        return mInteractingTab;
    }

    /**
     * Handles the four different reordering cases:
     *
     * <pre>
     * A] Tab is not interacting with tab groups. Reorder as normal.
     * B] Tab is in a group. Maybe drag out of group.
     * C] Tab is not in a group.
     *  C.1] Adjacent group is collapsed. Maybe reorder past the collapsed group
     *  C.2] Adjacent group is not collapsed. Maybe merge to group.
     * </pre>
     *
     * If the tab has been dragged past the threshold for the given case, update the {@link
     * TabModel} and return {@code true}. Else, return {@code false}.
     *
     * @param stripViews The list of {@link StripLayoutView}.
     * @param groupTitles The list of {@link StripLayoutGroupTitle}.
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param interactingTab The tab to reorder.
     * @param offset The distance the interacting tab has been dragged from its ideal position.
     * @return {@code True} if the reorder was successful. {@code False} otherwise.
     */
    private boolean reorderTabIfThresholdReached(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            StripLayoutTab interactingTab,
            float offset,
            int curIndex) {
        boolean towardEnd = isOffsetTowardEnd(offset);
        Tab curTab = mModel.getTabAt(curIndex);
        Tab adjTab = mModel.getTabAt(/* index= */ curIndex + (towardEnd ? 1 : -1));
        boolean isInGroup = mTabGroupModelFilter.isTabInTabGroup(curTab);
        boolean mayDragInOrOutOfGroup =
                adjTab == null
                        ? isInGroup
                        : StripLayoutUtils.notRelatedAndEitherTabInGroup(
                                mTabGroupModelFilter, curTab, adjTab);

        // Case A: Not interacting with tab groups.
        if (!mayDragInOrOutOfGroup) {
            if (adjTab == null || Math.abs(offset) <= getTabSwapThreshold()) return false;

            int destIndex = towardEnd ? curIndex + 2 : curIndex - 1;
            mModel.moveTab(interactingTab.getTabId(), destIndex);
            animateViewSliding(stripTabs[curIndex]);
            return true;
        }

        // Case B: Maybe drag out of group.
        if (isInGroup) {
            StripLayoutGroupTitle interactingGroupTitle =
                    StripLayoutUtils.findGroupTitle(groupTitles, curTab.getRootId());
            float threshold = getDragOutThreshold(interactingGroupTitle, towardEnd);
            if (Math.abs(offset) <= threshold) return false;

            moveInteractingTabOutOfGroup(
                    stripViews,
                    groupTitles,
                    interactingTab,
                    interactingGroupTitle,
                    towardEnd,
                    ActionType.REORDER);
            return true;
        }

        StripLayoutGroupTitle interactingGroupTitle =
                StripLayoutUtils.findGroupTitle(groupTitles, adjTab.getRootId());
        if (interactingGroupTitle.isCollapsed()) {
            // Case C.1: Maybe drag past collapsed group.
            float threshold =
                    interactingGroupTitle.getWidth()
                            * StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;
            if (Math.abs(offset) <= threshold) return false;

            movePastCollapsedGroup(interactingTab, interactingGroupTitle, curIndex, towardEnd);
            return true;
        } else {
            // Case C.2: Maybe merge to group.
            if (Math.abs(offset) <= getDragInThreshold()) return false;

            mergeInteractingTabToGroup(
                    adjTab.getId(), interactingTab, interactingGroupTitle, towardEnd);
            return true;
        }
    }

    /**
     * Moves the interacting tab past the adjacent collapsed group. Animates accordingly.
     *
     * @param interactingTab The interacting tab to move past group.
     * @param groupTitle The collapsed group title we are attempting to drag past.
     * @param curIndex The index of the interacting tab.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     */
    private void movePastCollapsedGroup(
            StripLayoutTab interactingTab,
            StripLayoutGroupTitle groupTitle,
            int curIndex,
            boolean towardEnd) {
        // Move the tab, then animate the adjacent group indicator sliding.
        int numTabsToSkip = mTabGroupModelFilter.getTabCountForGroup(groupTitle.getTabGroupId());
        int destIndex = towardEnd ? curIndex + 1 + numTabsToSkip : curIndex - numTabsToSkip;
        mModel.moveTab(interactingTab.getTabId(), destIndex);
        animateViewSliding(groupTitle);
    }

    /**
     * Merges the interacting tab to the given group. Animates accordingly.
     *
     * @param destinationTabId The tab ID to merge the interacting tab to.
     * @param interactingTab The interacting tab to merge to group.
     * @param groupTitle The title of the group the interacting tab is attempting to merge to.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     */
    private void mergeInteractingTabToGroup(
            int destinationTabId,
            StripLayoutTab interactingTab,
            StripLayoutGroupTitle groupTitle,
            boolean towardEnd) {
        mTabGroupModelFilter.mergeTabsToGroup(
                interactingTab.getTabId(), destinationTabId, /* skipUpdateTabModel= */ true);
        RecordUserAction.record("MobileToolbarReorderTab.TabAddedToGroup");

        // Animate the group indicator after updating the tab model.
        animateGroupIndicatorForTabReorder(groupTitle, /* isMovingOutOfGroup= */ false, towardEnd);
    }

    /** Returns the threshold to drag into a group. */
    private float getDragInThreshold() {
        return StripLayoutUtils.getHalfTabWidth(mTabWidthSupplier)
                * StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;
    }

    /**
     * @param groupTitle The group title for the desired group. Must not be null.
     * @param towardEnd True if dragging towards the end of the strip.
     * @return The threshold to drag out of a group.
     */
    private float getDragOutThreshold(StripLayoutGroupTitle groupTitle, boolean towardEnd) {
        float dragOutThreshold =
                StripLayoutUtils.getHalfTabWidth(mTabWidthSupplier)
                        * StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;
        return dragOutThreshold + (towardEnd ? 0 : groupTitle.getWidth());
    }
}

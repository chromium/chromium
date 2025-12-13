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
import org.chromium.chrome.browser.compositor.overlays.strip.StripTabModelActionListener.ActionType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.StripUpdateDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.ui.base.LocalizationUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.function.Supplier;

/** Tab reorder - drag tab past other tabs or into/out of groups within the tab strip. */
@NullMarked
public class TabReorderStrategy extends ReorderStrategyBase {
    // Tab being reordered.
    private @Nullable StripLayoutTab mInteractingTab;

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
            ObservableSupplierImpl<@Nullable Token> groupIdToHideSupplier,
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
        assumeNonNull(mInteractingTab);
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
        StripLayoutView firstView = stripViews[0];
        StripLayoutView lastView = stripViews[stripViews.length - 1];
        int newIndex = StripLayoutUtils.findIndexForTab(stripTabs, mInteractingTab.getTabId());

        // Case 1. if the tab is at the strip's edge(first or last position) trim the x-offset based
        // on the relevant margin(e.g. start, trailing, or group drag out threshold).
        if (newIndex == 0 || newIndex == stripTabs.length - 1) {
            if (newIndex == 0) {
                float limit =
                        (firstView instanceof StripLayoutGroupTitle groupTitle)
                                ? getDragOutThreshold(groupTitle, /* towardEnd= */ false)
                                : mScrollDelegate.getReorderStartMargin();
                offset = isRtl ? Math.min(limit, offset) : Math.max(-limit, offset);
            }
            if (newIndex == stripTabs.length - 1) {
                float limit = stripTabs[newIndex].getTrailingMargin();
                offset = isRtl ? Math.max(-limit, offset) : Math.min(limit, offset);
            }
        } else {
            // case 2. If the tab strip has both pinned and unpinned tabs, clamp the offset when
            // dragging an unpinned tab toward the start or a pinned tab toward the end. The
            // limit is determined by the boundary of the respective first or last view on the tab
            // strip.
            boolean towardEnd = isOffsetTowardEnd(offset);
            boolean isDraggingUnpinnedTabToPinnedStart =
                    !mInteractingTab.getIsPinned()
                            && (firstView instanceof StripLayoutTab firstTab
                                    && firstTab.getIsPinned());
            if (isDraggingUnpinnedTabToPinnedStart && !towardEnd) {
                float limit = getDragOffsetLimit(mInteractingTab, firstView, offset > 0);
                offset = isRtl ? Math.min(limit, offset) : Math.max(limit, offset);
            }
            boolean isLastViewGroupOrUnpinnedTab =
                    lastView instanceof StripLayoutGroupTitle
                            || (lastView instanceof StripLayoutTab lastTab
                                    && !lastTab.getIsPinned());
            boolean isDraggingPinnedTabToUnpinnedEnd =
                    mInteractingTab.getIsPinned() && isLastViewGroupOrUnpinnedTab;
            if (isDraggingPinnedTabToUnpinnedEnd && towardEnd) {
                float limit = getDragOffsetLimit(mInteractingTab, lastView, offset > 0);
                offset = isRtl ? Math.max(limit, offset) : Math.min(limit, offset);
            }
        }
        mInteractingTab.setOffsetX(offset);
    }

    @Override
    public void stopReorderMode(StripLayoutView[] stripViews, StripLayoutGroupTitle[] groupTitles) {
        List<Animator> animatorList = new ArrayList<>();
        Runnable onAnimationEnd =
                () -> {
                    if (mInteractingTab != null) {
                        mInteractingTab.setIsForegrounded(/* isForegrounded= */ false);
                        mInteractingTab = null;
                    }
                };
        handleStopReorderMode(
                stripViews,
                groupTitles,
                Collections.singletonList(mInteractingTab),
                mInteractingTab,
                animatorList,
                onAnimationEnd);
    }

    @Override
    public @Nullable StripLayoutView getInteractingView() {
        return mInteractingTab;
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
        assert reorderingView instanceof StripLayoutTab
                : "Using incorrect ReorderStrategy for view type.";
        StripLayoutTab tab = (StripLayoutTab) reorderingView;

        // Ensure we have a valid current index.
        int curIndex = StripLayoutUtils.findIndexForTab(stripTabs, tab.getTabId());
        assert curIndex != TabModel.INVALID_TAB_INDEX;

        // Fake a successful reorder in the target direction.
        float offset = MathUtils.flipSignIf(Float.MAX_VALUE, toLeft);
        reorderTabIfThresholdReached(
                stripViews,
                groupTitles,
                stripTabs,
                (StripLayoutTab) reorderingView,
                offset,
                curIndex);

        // Animate the reordering view and ensure it's foregrounded.
        tabDelegate.setIsTabNonDragReordering(tab, /* isNonDragReordering= */ true);
        reorderingView.setIsForegrounded(/* isForegrounded= */ true);
        animateViewSliding(
                reorderingView,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        tabDelegate.setIsTabNonDragReordering(
                                tab, /* isNonDragReordering= */ false);
                        reorderingView.setIsForegrounded(/* isForegrounded= */ false);
                    }
                });
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
        Tab curTab = mModel.getTabAtChecked(curIndex);
        Tab adjTab = mModel.getTabAt(/* index= */ curIndex + (towardEnd ? 1 : -1));
        boolean isInGroup = mTabGroupModelFilter.isTabInTabGroup(curTab);
        boolean mayDragInOrOutOfGroup =
                adjTab == null
                        ? isInGroup
                        : StripLayoutUtils.notRelatedAndEitherTabInGroup(
                                mTabGroupModelFilter, curTab, adjTab);

        // Do not allow reorder between pinned and unpinned tabs.
        boolean curTabPinned = curTab != null && curTab.getIsPinned();
        boolean adjTabPinned = adjTab != null && adjTab.getIsPinned();

        boolean crossingPinnedBound = curTabPinned != adjTabPinned;
        boolean draggingTabOutOfGroup = isInGroup && mayDragInOrOutOfGroup;
        if (crossingPinnedBound && !draggingTabOutOfGroup) return false;

        // Case A: Not interacting with tab groups.
        if (!mayDragInOrOutOfGroup) {
            if (adjTab == null || Math.abs(offset) <= getTabSwapThreshold(curTabPinned)) {
                return false;
            }

            int destIndex = towardEnd ? curIndex + 1 : curIndex - 1;
            mModel.moveTab(interactingTab.getTabId(), destIndex);
            animateViewSliding(stripTabs[curIndex]);
            return true;
        }

        // Case B: Maybe drag out of group.
        if (isInGroup) {
            StripLayoutGroupTitle interactingGroupTitle =
                    StripLayoutUtils.findGroupTitle(groupTitles, curTab.getTabGroupId());
            assumeNonNull(interactingGroupTitle);
            float threshold = getDragOutThreshold(interactingGroupTitle, towardEnd);
            if (Math.abs(offset) <= threshold) return false;

            moveInteractingTabsOutOfGroup(
                    stripViews,
                    groupTitles,
                    Collections.singletonList(interactingTab),
                    interactingGroupTitle,
                    towardEnd,
                    ActionType.REORDER);
            return true;
        }

        assumeNonNull(adjTab);
        StripLayoutGroupTitle interactingGroupTitle =
                StripLayoutUtils.findGroupTitle(groupTitles, adjTab.getTabGroupId());
        assumeNonNull(interactingGroupTitle);
        if (interactingGroupTitle.isCollapsed()) {
            // Case C.1: Maybe drag past collapsed group.
            float threshold =
                    interactingGroupTitle.getWidth()
                            * StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;
            if (Math.abs(offset) <= threshold) return false;

            movePastCollapsedGroup(interactingTab, interactingGroupTitle, curIndex, towardEnd);
        } else {
            // Case C.2: Maybe merge to group.
            if (Math.abs(offset) <= getDragInThreshold()) return false;

            mergeInteractingTabToGroup(
                    adjTab.getId(), interactingTab, interactingGroupTitle, towardEnd);
        }
        return true;
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
        int destIndex = towardEnd ? curIndex + numTabsToSkip : curIndex - numTabsToSkip;
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
        Tab tabToMerge = mModel.getTabByIdChecked(interactingTab.getTabId());
        List<Tab> tabsToMarge = Collections.singletonList(tabToMerge);
        Tab destinationTab = mModel.getTabByIdChecked(destinationTabId);

        // If dragging towards the end of the strip, we should insert at the start of the group.
        // Otherwise, we insert at the end of the group (by passing a null index).
        Integer indexInGroup = towardEnd ? 0 : null;

        // TODO(crbug.com/451697001): Investigate if we still need to suppress the notifications.
        mTabGroupModelFilter.mergeListOfTabsToGroup(
                tabsToMarge, destinationTab, indexInGroup, MergeNotificationType.DONT_NOTIFY);
        RecordUserAction.record("MobileToolbarReorderTab.TabAddedToGroup");

        // Animate the group indicator after updating the tab model.
        animateGroupIndicatorForTabReorder(groupTitle, /* isMovingOutOfGroup= */ false, towardEnd);
    }
}

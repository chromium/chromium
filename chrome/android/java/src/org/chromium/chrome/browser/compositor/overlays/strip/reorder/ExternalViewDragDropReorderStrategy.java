// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.graphics.PointF;
import android.view.View;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.AnimationHost;
import org.chromium.chrome.browser.compositor.overlays.strip.ScrollDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.StripUpdateDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.base.LocalizationUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.function.Supplier;

/** Drag and drop reorder - drag external view onto / out-of strip and reorder within strip. */
@NullMarked
public class ExternalViewDragDropReorderStrategy extends ReorderStrategyBase {
    // View on the strip being hovered on by the dragged view.
    private @Nullable StripLayoutView mInteractingView;

    // View on the strip last hovered on by dragged view. This can be used post stop reorder to
    // handle drop event (eg: re-parenting dropped tab).
    private @Nullable StripLayoutView mInteractingViewDuringStop;

    ExternalViewDragDropReorderStrategy(
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

    /** Initiate reorder when external view is dragged onto strip. */
    @Override
    public void startReorderMode(
            StripLayoutView[] stripViews,
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle[] stripGroupTitles,
            StripLayoutView interactingView,
            PointF startPoint) {
        // 1. Set initial state and add edge margins.
        mInteractingView = interactingView;
        mInteractingViewDuringStop = null;
        mAnimationHost.finishAnimationsAndPushTabUpdates();
        setEdgeMarginsForReorder(stripTabs);

        // 2. Add a trailing margin to the interacting view to indicate where the view will be
        // inserted should the drag be dropped.
        ArrayList<Animator> animationList = new ArrayList<>();
        setInteractingStateForView(
                interactingView,
                stripGroupTitles,
                stripTabs,
                /* isInteracting= */ true,
                animationList);

        // 3. Kick-off animations and request an update.
        mAnimationHost.startAnimations(animationList, null);
    }

    // TODO(crbug.com/441144131): Investigate supporting mixed pin state for multi-select.
    @Override
    public void updateReorderPosition(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            float endX,
            float deltaX,
            @ReorderType int reorderType) {
        // 1. Adjust by a half tab-width so that we target the nearest tab gap.
        boolean isDraggedItemPinned = TabStripDragHandler.isDraggingPinnedItem();
        float adjustedXForDrop =
                StripLayoutUtils.adjustXForTabDrop(endX, mTabWidthSupplier, isDraggedItemPinned);

        // 2. Clear previous "interacting" view if inserting at the start of the strip.
        final float leftEdge;
        final float rightEdge;
        if (stripViews[0] instanceof StripLayoutTab tab) {
            leftEdge = tab.getTouchTargetLeft();
            rightEdge = tab.getTouchTargetRight();
        } else {
            StripLayoutGroupTitle groupTitle = (StripLayoutGroupTitle) stripViews[0];
            leftEdge = groupTitle.getDrawX();
            rightEdge = groupTitle.getDrawX() + groupTitle.getWidth();
        }
        boolean inStartGap =
                LocalizationUtils.isLayoutRtl()
                        ? adjustedXForDrop > rightEdge
                        : adjustedXForDrop < leftEdge;

        if (inStartGap
                && mInteractingView != null
                && isDraggedItemPinned == isHoveredViewPinned(stripViews[0])) {
            mScrollDelegate.setReorderStartMargin(
                    /* newStartMargin= */ StripLayoutUtils.getHalfTabWidth(
                            mTabWidthSupplier, isDraggedItemPinned));

            mAnimationHost.finishAnimations();
            ArrayList<Animator> animationList = new ArrayList<>();
            setInteractingStateForView(
                    mInteractingView,
                    groupTitles,
                    stripTabs,
                    /* isInteracting= */ false,
                    animationList);
            mInteractingView = null;
            mAnimationHost.startAnimations(animationList, null);

            // 2.a. Early-out if we just entered the start gap.
            return;
        }
        // 3. Otherwise, update drop indicator if necessary.
        StripLayoutView hoveredView =
                StripLayoutUtils.findViewAtPositionX(
                        stripViews, adjustedXForDrop, /* includeGroupTitles= */ true);

        if (hoveredView != null && hoveredView != mInteractingView) {
            mAnimationHost.finishAnimations();

            // 3.a. Reset the state for the previous "interacting" view.
            ArrayList<Animator> animationList = new ArrayList<>();
            if (mInteractingView != null) {
                setInteractingStateForView(
                        mInteractingView,
                        groupTitles,
                        stripTabs,
                        /* isInteracting= */ false,
                        animationList);
            }

            // 3.b. Set state for the new "interacting" view.
            setInteractingStateForView(
                    hoveredView, groupTitles, stripTabs, /* isInteracting= */ true, animationList);
            mInteractingView = hoveredView;

            // 3.c. Animate.
            mAnimationHost.startAnimations(animationList, null);
        }
    }

    @Override
    public void stopReorderMode(StripLayoutView[] stripViews, StripLayoutGroupTitle[] groupTitles) {
        List<Animator> animatorList = new ArrayList<>();
        mInteractingViewDuringStop = mInteractingView;
        Runnable onAnimationEnd = () -> mInteractingView = null;
        handleStopReorderMode(
                stripViews,
                groupTitles,
                Collections.singletonList(mInteractingView),
                null,
                animatorList,
                onAnimationEnd);
    }

    @Override
    public @Nullable StripLayoutView getInteractingView() {
        return mInteractingView;
    }

    @Override
    public boolean shouldAllowAutoScroll() {
        // Do not allow auto-scroll when a pinned tab is dragged over unpinned tabs; pinned tabs can
        // only be dropped into the pinned section.
        return !TabStripDragHandler.isDraggingPinnedItem();
    }

    /** Merges dropped tabs to interacting view's tab group, if one exists. */
    boolean handleDrop(StripLayoutGroupTitle[] groupTitles, List<Integer> tabIds, int dropIndex) {
        if (mInteractingViewDuringStop == null) return false;

        @Nullable StripLayoutGroupTitle groupTitle;
        final int destinationTabId;
        if (mInteractingViewDuringStop instanceof StripLayoutTab interactingStripTab) {
            Tab interactingTab = mModel.getTabByIdChecked(interactingStripTab.getTabId());
            groupTitle =
                    StripLayoutUtils.findGroupTitle(groupTitles, interactingTab.getTabGroupId());
            destinationTabId = interactingTab.getId();
        } else {
            groupTitle = (StripLayoutGroupTitle) mInteractingViewDuringStop;
            Token destinationTabGroupId = groupTitle.getTabGroupId();
            destinationTabId = mTabGroupModelFilter.getGroupLastShownTabId(destinationTabGroupId);
        }

        // 1. If hovered on view is not part of group or is collapsed, no-op.
        if (groupTitle == null || groupTitle.isCollapsed()) {
            mInteractingViewDuringStop = null;
            return false;
        }

        // 2. Merge all tabs in dragged tab group to hovered tab's group at drop index.
        List<Tab> tabsToMerge = new ArrayList<>();
        for (int tabId : tabIds) {
            // Need to reverse, since the list of tab ids was reversed.
            tabsToMerge.add(0, mModel.getTabByIdChecked(tabId));
        }
        List<Tab> destinationTabList = mTabGroupModelFilter.getRelatedTabList(destinationTabId);
        int mergeIndex = dropIndex - mModel.indexOf(destinationTabList.get(0));
        mTabGroupModelFilter.mergeListOfTabsToGroup(
                tabsToMerge,
                mModel.getTabByIdChecked(destinationTabId),
                mergeIndex,
                MergeNotificationType.DONT_NOTIFY);

        // 3. Animate bottom indicator. Done after merging the dragged tab group to group,
        // so that the calculated bottom indicator width will be correct.
        runOnDropAnimation(groupTitle);
        return true;
    }

    private void runOnDropAnimation(StripLayoutGroupTitle groupTitle) {
        List<Animator> animators = new ArrayList<>();
        updateBottomIndicatorWidthForTabReorder(
                mAnimationHost.getAnimationHandler(),
                mTabGroupModelFilter,
                groupTitle,
                /* isMovingOutOfGroup= */ false,
                /* throughGroupTitle= */ false,
                animators);
        mAnimationHost.startAnimations(
                animators,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mInteractingViewDuringStop = null;
                    }
                });
    }

    /** Wrapper for #setTrailingMarginForView and #shouldHaveTrailingMargin. */
    protected void setInteractingStateForView(
            StripLayoutView stripView,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            boolean isInteracting,
            List<Animator> animationList) {
        setTrailingMarginForView(
                stripView,
                groupTitles,
                shouldHaveTrailingMargin(stripTabs, stripView, isInteracting),
                animationList);
    }

    private boolean shouldHaveTrailingMargin(
            StripLayoutTab[] stripTabs, StripLayoutView interactingView, boolean isInteracting) {
        if (!isInteracting) return false;

        if (TabStripDragHandler.isDraggingPinnedItem() != isHoveredViewPinned(interactingView)) {
            return StripLayoutUtils.isLastPinnedTab(stripTabs, interactingView);
        }

        // If the dragged item is a tab (not a group) and it’s unpinned, allow merge into a group.
        if (TabStripDragHandler.isDraggingUnpinnedTab()) return true;

        // Skip applying trailing margin for grouped views (like expanded group titles or tabs) when
        // merging on drop is not allowed.
        if (interactingView instanceof StripLayoutGroupTitle groupTitle) {
            return groupTitle.isCollapsed();
        } else {
            assert interactingView instanceof StripLayoutTab : "Unexpected view type";
            return !StripLayoutUtils.isNonTrailingTabInGroup(
                    mTabGroupModelFilter, mModel, (StripLayoutTab) interactingView);
        }
    }

    private boolean isHoveredViewPinned(StripLayoutView hoveredView) {
        return (hoveredView instanceof StripLayoutTab tab) && tab.getIsPinned();
    }

    // ============================================================================================
    // IN-TEST
    // ============================================================================================

    @Nullable StripLayoutView getInteractingViewDuringStopForTesting() {
        return mInteractingViewDuringStop;
    }
}

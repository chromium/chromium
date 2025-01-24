// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import android.graphics.PointF;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

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
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;
import org.chromium.chrome.browser.tasks.tab_management.TabShareUtils;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

/**
 * Drag and drop reorder - start dragging strip view. Subsequently drag out of, within and back onto
 * strip.
 */
class SourceViewDragDropReorderStrategy extends ReorderStrategyBase {
    // Drag helpers
    private final TabDragSource mTabDragSource;
    private final ActionConfirmationManager mActionConfirmationManager;
    private final ReorderStrategy mTabStrategy;

    // View on strip being dragged.
    private StripLayoutView mViewBeingDragged;

    // View offsetX when it was dragged off the strip. Used to re-position the view when dragged
    // back onto strip.
    private float mLastOffsetX;

    // Whether sub-strategy is in progress.
    private boolean mTabStrategyInProgress;

    SourceViewDragDropReorderStrategy(
            ReorderDelegate reorderDelegate,
            StripUpdateDelegate stripUpdateDelegate,
            AnimationHost animationHost,
            ScrollDelegate scrollDelegate,
            TabModel model,
            TabGroupModelFilter tabGroupModelFilter,
            View containerView,
            ObservableSupplierImpl<Integer> groupIdToHideSupplier,
            Supplier<Float> tabWidthSupplier,
            @NonNull TabDragSource tabDragSource,
            @NonNull ActionConfirmationManager actionConfirmationManager,
            ReorderStrategy tabStrategy) {
        super(
                reorderDelegate,
                stripUpdateDelegate,
                animationHost,
                scrollDelegate,
                model,
                tabGroupModelFilter,
                containerView,
                groupIdToHideSupplier,
                tabWidthSupplier);
        mTabDragSource = tabDragSource;
        mActionConfirmationManager = actionConfirmationManager;
        mTabStrategy = tabStrategy;
    }

    /** Initiate Android Drag-Drop for interactingView. */
    @Override
    public void startReorderMode(
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle[] stripGroupTitles,
            @NonNull StripLayoutView interactingView,
            PointF startPoint) {
        Tab tab = mModel.getTabById(((StripLayoutTab) interactingView).getTabId());
        boolean dragStarted =
                mTabDragSource.startTabDragAction(
                        mContainerView,
                        tab,
                        startPoint,
                        interactingView.getDrawX(),
                        interactingView.getWidth());
        if (dragStarted) {
            mViewBeingDragged = interactingView;
            mLastOffsetX = 0.f;
        } else {
            // Drag did not start. Stop reorder.
            mReorderDelegate.stopReorderMode(stripGroupTitles, stripTabs);

            // Fallback to reorder view in strip.
            mReorderDelegate.startReorderMode(
                    stripTabs,
                    stripGroupTitles,
                    interactingView,
                    startPoint,
                    ReorderType.DRAG_WITHIN_STRIP);
        }
    }

    @Override
    public void updateReorderPosition(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            float endX,
            float deltaX,
            @ReorderType int reorderType) {
        StripLayoutTab draggedTab = (StripLayoutTab) mViewBeingDragged;
        if (reorderType == ReorderType.DRAG_ONTO_STRIP) {
            // 1. Bring dragged view onto strip, resize strip views accordingly.
            mAnimationHost.finishAnimationsAndPushTabUpdates();
            bringViewOntoStrip(draggedTab);
            mStripUpdateDelegate.resizeTabStrip(
                    /* animate= */ false, /* tabToAnimate= */ null, /* animateTabAdded= */ false);

            // 2. Start to reorder within strip - delegate to another strategy.
            mTabStrategy.startReorderMode(
                    stripTabs, groupTitles, mViewBeingDragged, new PointF(endX, 0f));
            mTabStrategyInProgress = true;
        } else if (reorderType == ReorderType.DRAG_WITHIN_STRIP) {
            // Drag within strip - delegate to another strategy.
            mTabStrategy.updateReorderPosition(
                    stripViews, groupTitles, stripTabs, endX, deltaX, reorderType);
        } else if (reorderType == ReorderType.DRAG_OUT_OF_STRIP) {
            // 1. Maybe show user prompt when last tab in group is dragged out.
            // Stop reorder and return if so.
            boolean draggedLastTabInGroupWithPrompt = shouldShowUserPrompt(draggedTab);
            if (draggedLastTabInGroupWithPrompt) {
                moveInteractingTabOutOfGroup(
                        groupTitles,
                        stripTabs,
                        draggedTab,
                        /* groupTitleToAnimate= */ null,
                        /* towardEnd= */ false,
                        ActionType.DRAG_OFF_STRIP);
                return;
            }

            // 2. Prompt not shown - Store reorder state, then exit reorder within strip.
            mLastOffsetX = draggedTab.getOffsetX();
            mTabStrategy.stopReorderMode(groupTitles, stripTabs);
            mTabStrategyInProgress = false;

            // 3. Immediately hide the dragged tab container, as if it were being translated
            // off like a closed tab. Resize strip views accordingly.
            mAnimationHost.finishAnimationsAndPushTabUpdates();
            removeViewOutOfStrip(draggedTab);
            mStripUpdateDelegate.resizeTabStrip(
                    /* animate= */ true, draggedTab, /* animateTabAdded= */ false);
        }
    }

    @Override
    public void stopReorderMode(StripLayoutGroupTitle[] groupTitles, StripLayoutTab[] stripTabs) {
        // If the dragged view was re-parented, it will no longer be present in model.
        // If this is not the case, attempt to restore view to its original position.
        StripLayoutTab draggedTab = (StripLayoutTab) mViewBeingDragged;
        if (draggedTab != null
                && mModel.getTabById(draggedTab.getTabId()) != null
                && draggedTab.isDraggedOffStrip()) {
            // If tab was ungrouped during drag, restore group indicator.
            if (StripLayoutUtils.isLastTabInGroup(mTabGroupModelFilter, draggedTab.getTabId())
                    && mActionConfirmationManager.willSkipUngroupTabAttempt()) {
                mGroupIdToHideSupplier.set(Tab.INVALID_TAB_ID);
            }
            mAnimationHost.finishAnimationsAndPushTabUpdates();
            draggedTab.setIsDraggedOffStrip(false);
            // Animate the tab translating back up onto the tab strip.
            draggedTab.setWidth(0.f);
            mStripUpdateDelegate.resizeTabStrip(
                    /* animate= */ true, draggedTab, /* animateTabAdded= */ true);
        }
        if (mTabStrategyInProgress) {
            mTabStrategy.stopReorderMode(groupTitles, stripTabs);
        }
        mViewBeingDragged = null;
        mLastOffsetX = 0;
    }

    @Override
    public StripLayoutView getInteractingView() {
        return mViewBeingDragged;
    }

    private void removeViewOutOfStrip(StripLayoutTab draggedTab) {
        draggedTab.setIsDraggedOffStrip(true);
        draggedTab.setDrawX(draggedTab.getIdealX());
        draggedTab.setDrawY(draggedTab.getHeight());
        draggedTab.setOffsetY(draggedTab.getHeight());
    }

    private void bringViewOntoStrip(StripLayoutTab draggedTab) {
        draggedTab.setIsDraggedOffStrip(false);
        draggedTab.setOffsetX(mLastOffsetX);
        draggedTab.setOffsetY(0);
        mLastOffsetX = 0f;
    }

    private boolean shouldShowUserPrompt(StripLayoutTab draggedTab) {
        int tabId = draggedTab.getTabId();
        boolean draggingLastTabInGroup =
                StripLayoutUtils.isLastTabInGroup(mTabGroupModelFilter, tabId);
        boolean willSkipDialog =
                mActionConfirmationManager.willSkipUngroupTabAttempt()
                        && !isTabInCollaboration(tabId);
        return draggingLastTabInGroup
                && !mTabGroupModelFilter.isIncognitoBranded()
                && !willSkipDialog;
    }

    private boolean isTabInCollaboration(int tabId) {
        @Nullable
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(
                        mTabGroupModelFilter.getTabModel().getProfile());
        @Nullable
        String collaborationId =
                TabShareUtils.getCollaborationIdOrNull(
                        tabId, mTabGroupModelFilter.getTabModel(), tabGroupSyncService);
        return TabShareUtils.isCollaborationIdValid(collaborationId);
    }

    // ============================================================================================
    // IN-TEST
    // ============================================================================================

    StripLayoutView getViewBeingDraggedForTesting() {
        return mViewBeingDragged;
    }

    float getDragLastOffsetXForTesting() {
        return mLastOffsetX;
    }
}

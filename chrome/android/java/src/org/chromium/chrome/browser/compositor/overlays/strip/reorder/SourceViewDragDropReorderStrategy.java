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
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabShareUtils;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.List;

/**
 * Drag and drop reorder - start dragging strip view. Subsequently drag out of, within and back onto
 * strip.
 */
class SourceViewDragDropReorderStrategy extends ReorderStrategyBase {
    // Drag helpers
    private final TabDragSource mTabDragSource;
    private final ActionConfirmationManager mActionConfirmationManager;
    private final ReorderSubStrategy mTabSubStrategy;
    private final ReorderSubStrategy mGroupSubStrategy;

    // View on strip being dragged.
    private StripLayoutView mViewBeingDragged;

    // View offsetX when it was dragged off the strip. Used to re-position the view when dragged
    // back onto strip.
    private float mLastOffsetX;

    // Active sub-strategy based on interacting view class.
    private ReorderSubStrategy mActiveSubStrategy;

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
            Supplier<Long> lastReorderScrollTimeSupplier,
            @NonNull TabDragSource tabDragSource,
            @NonNull ActionConfirmationManager actionConfirmationManager,
            ReorderStrategy tabStrategy,
            ReorderStrategy groupStrategy) {
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
        mTabDragSource = tabDragSource;
        mActionConfirmationManager = actionConfirmationManager;
        mTabSubStrategy = new TabReorderSubStrategy(tabStrategy);
        mGroupSubStrategy = new GroupReorderSubStrategy(groupStrategy);
    }

    /** Initiate Android Drag-Drop for interactingView. */
    @Override
    public void startReorderMode(
            StripLayoutView[] stripViews,
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle[] stripGroupTitles,
            @NonNull StripLayoutView interactingView,
            PointF startPoint) {
        // Set initial state.
        mViewBeingDragged = interactingView;
        mLastOffsetX = 0.f;

        // Set the correct sub-strategy.
        if (interactingView instanceof StripLayoutTab) {
            mActiveSubStrategy = mTabSubStrategy;
        } else if (interactingView instanceof StripLayoutGroupTitle) {
            mActiveSubStrategy = mGroupSubStrategy;
        }

        // Attempt to start a drag and drop action. If the drag successfully started, early-out.
        if (mActiveSubStrategy != null
                && mActiveSubStrategy.startViewDragAction(stripTabs, startPoint)) {
            return;
        }

        // Drag did not start. Stop reorder, and fallback to reordering within the strip.
        mReorderDelegate.stopReorderMode(stripViews, stripGroupTitles);
        mReorderDelegate.startReorderMode(
                stripViews,
                stripTabs,
                stripGroupTitles,
                interactingView,
                startPoint,
                ReorderType.DRAG_WITHIN_STRIP);
    }

    @Override
    public void updateReorderPosition(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            float endX,
            float deltaX,
            @ReorderType int reorderType) {
        assert mActiveSubStrategy != null : "Attempted to drag without an active sub-strategy.";
        // Delegate to the active substrategy.
        if (reorderType == ReorderType.DRAG_ONTO_STRIP) {
            mActiveSubStrategy.startReorderMode(
                    stripViews, stripTabs, groupTitles, mViewBeingDragged, new PointF(endX, 0f));
        } else if (reorderType == ReorderType.DRAG_WITHIN_STRIP) {
            if (mActiveSubStrategy.mInProgress) {
                // We evidently can get DRAG_WITHIN_STRIP events without first getting a
                // DRAG_ONTO_STRIP event. e.g. Through auto-scroll handling.
                mActiveSubStrategy.updateReorderPosition(
                        stripViews, groupTitles, stripTabs, endX, deltaX, reorderType);
            }
        } else if (reorderType == ReorderType.DRAG_OUT_OF_STRIP) {
            mActiveSubStrategy.stopReorderMode(stripViews, groupTitles);
        }
    }

    @Override
    public void stopReorderMode(StripLayoutView[] stripViews, StripLayoutGroupTitle[] groupTitles) {
        if (mActiveSubStrategy != null) {
            mActiveSubStrategy.onStopViewDragAction(stripViews, groupTitles);
        }
        mActiveSubStrategy = null;
        mViewBeingDragged = null;
        mLastOffsetX = 0;
    }

    @Override
    public StripLayoutView getInteractingView() {
        return mViewBeingDragged;
    }

    boolean isReorderingTab() {
        return mActiveSubStrategy == mTabSubStrategy;
    }

    private void removeViewOutOfStrip(StripLayoutView draggedView) {
        draggedView.setIsDraggedOffStrip(true);
        draggedView.setDrawX(draggedView.getIdealX());
        draggedView.setDrawY(draggedView.getHeight());
        draggedView.setOffsetY(draggedView.getHeight());
    }

    private void bringViewOntoStripAndOffset(StripLayoutView draggedView) {
        bringViewOntoStrip(draggedView);
        draggedView.setOffsetX(mLastOffsetX);
        mLastOffsetX = 0f;
    }

    private void bringViewOntoStrip(StripLayoutView draggedView) {
        draggedView.setIsDraggedOffStrip(false);
        draggedView.setOffsetY(0);
    }

    private boolean shouldShowUserPrompt(StripLayoutTab draggedTab) {
        int tabId = draggedTab.getTabId();
        boolean draggingLastTabInGroup =
                StripLayoutUtils.isLastTabInGroup(mTabGroupModelFilter, tabId);
        boolean willSkipDialog =
                mActionConfirmationManager.willSkipUngroupTabAttempt()
                        && !isTabInCollaboration(tabId);
        return draggingLastTabInGroup
                && !mTabGroupModelFilter.getTabModel().isIncognitoBranded()
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
    // Sub-strategies
    // ============================================================================================

    /** Wrapper for {@link ReorderStrategy} that runs additional logic for tab/group tearing. */
    private abstract static class ReorderSubStrategy implements ReorderStrategy {
        private final ReorderStrategy mWrappedStrategy;
        private boolean mInProgress;

        private ReorderSubStrategy(ReorderStrategy wrappedStrategy) {
            mWrappedStrategy = wrappedStrategy;
        }

        @Override
        public void startReorderMode(
                StripLayoutView[] stripViews,
                StripLayoutTab[] stripTabs,
                StripLayoutGroupTitle[] stripGroupTitles,
                @NonNull StripLayoutView interactingView,
                PointF startPoint) {
            mWrappedStrategy.startReorderMode(
                    stripViews, stripTabs, stripGroupTitles, interactingView, startPoint);
            mInProgress = true;
        }

        @Override
        public void updateReorderPosition(
                StripLayoutView[] stripViews,
                StripLayoutGroupTitle[] groupTitles,
                StripLayoutTab[] stripTabs,
                float endX,
                float deltaX,
                int reorderType) {
            mWrappedStrategy.updateReorderPosition(
                    stripViews, groupTitles, stripTabs, endX, deltaX, reorderType);
        }

        @Override
        public void stopReorderMode(
                StripLayoutView[] stripViews, StripLayoutGroupTitle[] groupTitles) {
            mWrappedStrategy.stopReorderMode(stripViews, groupTitles);
            mInProgress = false;
        }

        @Override
        public StripLayoutView getInteractingView() {
            return mWrappedStrategy.getInteractingView();
        }

        /**
         * Attempts to start the view tearing action through {@link TabDragSource}.
         *
         * @param stripTabs The list of {@link StripLayoutTab}.
         * @param startPoint The location on-screen that the gesture started at.
         * @return {@code True} if the drag and drop action started, and {@code false} otherwise.
         */
        abstract boolean startViewDragAction(StripLayoutTab[] stripTabs, PointF startPoint);

        /** Called when the view tearing action has completed. */
        void onStopViewDragAction(
                StripLayoutView[] stripViews, StripLayoutGroupTitle[] groupTitles) {
            // Clear the wrapped strategy's state if needed.
            if (mInProgress) mWrappedStrategy.stopReorderMode(stripViews, groupTitles);
        }
    }

    private class TabReorderSubStrategy extends ReorderSubStrategy {
        TabReorderSubStrategy(ReorderStrategy tabReorderStrategy) {
            super(tabReorderStrategy);
        }

        @Override
        public void startReorderMode(
                StripLayoutView[] stripViews,
                StripLayoutTab[] stripTabs,
                StripLayoutGroupTitle[] stripGroupTitles,
                @NonNull StripLayoutView interactingView,
                PointF startPoint) {
            // 1. Hide compositor buttons.
            mStripUpdateDelegate.setCompositorButtonsVisible(false);

            // 2. Bring dragged view onto strip, resize strip views accordingly.
            StripLayoutTab draggedTab = (StripLayoutTab) interactingView;
            mAnimationHost.finishAnimationsAndPushTabUpdates();
            bringViewOntoStripAndOffset(draggedTab);
            mStripUpdateDelegate.resizeTabStrip(
                    /* animate= */ false, /* tabToAnimate= */ null, /* animateTabAdded= */ false);

            // 3. Start to reorder within strip - delegate to the wrapped strategy.
            super.startReorderMode(
                    stripViews, stripTabs, stripGroupTitles, interactingView, startPoint);
        }

        @Override
        public void stopReorderMode(
                StripLayoutView[] stripViews, StripLayoutGroupTitle[] groupTitles) {
            StripLayoutTab draggedTab = (StripLayoutTab) mViewBeingDragged;

            // 1. Show compositor buttons.
            mStripUpdateDelegate.setCompositorButtonsVisible(true);

            // 2. Maybe show user prompt when last tab in group is dragged out. Stop reorder and
            // return if so.
            boolean draggedLastTabInGroupWithPrompt = shouldShowUserPrompt(draggedTab);
            if (draggedLastTabInGroupWithPrompt) {
                moveInteractingTabOutOfGroup(
                        stripViews,
                        groupTitles,
                        draggedTab,
                        /* groupTitleToAnimate= */ null,
                        /* towardEnd= */ false,
                        ActionType.DRAG_OFF_STRIP);
                return;
            }

            // 3. Prompt not shown - Store reorder state, then exit reorder within strip.
            mLastOffsetX = draggedTab.getOffsetX();
            super.stopReorderMode(stripViews, groupTitles);

            // 4. Immediately hide the dragged tab container, as if it were being translated off
            // like a closed tab. Resize strip views accordingly.
            mAnimationHost.finishAnimationsAndPushTabUpdates();
            removeViewOutOfStrip(draggedTab);
            mStripUpdateDelegate.resizeTabStrip(
                    /* animate= */ true, draggedTab, /* animateTabAdded= */ false);
        }

        @Override
        boolean startViewDragAction(StripLayoutTab[] stripTabs, PointF startPoint) {
            Tab tab = mModel.getTabById(((StripLayoutTab) mViewBeingDragged).getTabId());
            assert tab != null : "No matching Tab found.";
            return mTabDragSource.startTabDragAction(
                    mContainerView,
                    tab,
                    startPoint,
                    mViewBeingDragged.getDrawX(),
                    mViewBeingDragged.getWidth());
        }

        @Override
        void onStopViewDragAction(
                StripLayoutView[] stripViews, StripLayoutGroupTitle[] groupTitles) {
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
            super.onStopViewDragAction(stripViews, groupTitles);
        }
    }

    private class GroupReorderSubStrategy extends ReorderSubStrategy {
        final List<StripLayoutView> mViewsBeingDragged = new ArrayList<>();

        GroupReorderSubStrategy(ReorderStrategy groupReorderStrategy) {
            super(groupReorderStrategy);
        }

        @Override
        public void startReorderMode(
                StripLayoutView[] stripViews,
                StripLayoutTab[] stripTabs,
                StripLayoutGroupTitle[] stripGroupTitles,
                StripLayoutView interactingView,
                PointF startPoint) {
            // 1. Hide compositor buttons.
            mStripUpdateDelegate.setCompositorButtonsVisible(false);

            // 2. Bring dragged views onto strip, resize strip accordingly.
            mAnimationHost.finishAnimationsAndPushTabUpdates();
            for (StripLayoutView view : mViewsBeingDragged) {
                bringViewOntoStripAndOffset(view);
            }
            mStripUpdateDelegate.resizeTabStrip(
                    /* animate= */ false, /* tabToAnimate= */ null, /* animateTabAdded= */ false);

            // 3. Start to reorder within strip - delegate to the wrapped strategy.
            super.startReorderMode(
                    stripViews, stripTabs, stripGroupTitles, interactingView, startPoint);
        }

        @Override
        public void stopReorderMode(
                StripLayoutView[] stripViews, StripLayoutGroupTitle[] groupTitles) {
            // 1. Show compositor buttons.
            mStripUpdateDelegate.setCompositorButtonsVisible(true);

            // 2. Store reorder state, then exit reorder within strip.
            mLastOffsetX = mViewBeingDragged.getOffsetX();
            super.stopReorderMode(stripViews, groupTitles);

            // 3. Immediately hide the dragged views without animating. Resize strip accordingly.
            // TODO(crbug.com/384855584): Animate this action.
            mAnimationHost.finishAnimationsAndPushTabUpdates();
            for (StripLayoutView view : mViewsBeingDragged) {
                removeViewOutOfStrip(view);
            }
            mStripUpdateDelegate.resizeTabStrip(
                    /* animate= */ false, /* tabToAnimate= */ null, /* animateTabAdded= */ false);
        }

        @Override
        boolean startViewDragAction(StripLayoutTab[] stripTabs, PointF startPoint) {
            StripLayoutGroupTitle draggedGroupTitle = (StripLayoutGroupTitle) mViewBeingDragged;
            mViewsBeingDragged.add(draggedGroupTitle);
            mViewsBeingDragged.addAll(
                    StripLayoutUtils.getGroupedTabs(
                            mModel, stripTabs, draggedGroupTitle.getRootId()));

            return mTabDragSource.startGroupDragAction(
                    mContainerView,
                    draggedGroupTitle.getTabGroupId(),
                    draggedGroupTitle.isGroupShared(),
                    startPoint,
                    draggedGroupTitle.getDrawX(),
                    draggedGroupTitle.getWidth());
        }

        @Override
        void onStopViewDragAction(
                StripLayoutView[] stripViews, StripLayoutGroupTitle[] groupTitles) {
            // If the dragged group was re-parented, it will no longer be present in model.
            // If this is not the case, attempt to restore the group to its original position.
            StripLayoutGroupTitle draggedGroupTitle = (StripLayoutGroupTitle) mViewBeingDragged;
            if (draggedGroupTitle != null
                    && mTabGroupModelFilter.tabGroupExists(draggedGroupTitle.getTabGroupId())
                    && draggedGroupTitle.isDraggedOffStrip()) {
                mAnimationHost.finishAnimationsAndPushTabUpdates();
                for (StripLayoutView view : mViewsBeingDragged) {
                    bringViewOntoStrip(view);
                }
                mStripUpdateDelegate.resizeTabStrip(
                        /* animate= */ false,
                        /* tabToAnimate= */ null,
                        /* animateTabAdded= */ false);
            }
            mViewsBeingDragged.clear();
            super.onStopViewDragAction(stripViews, groupTitles);
        }
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

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.chromium.build.NullUtil.assumeNonNull;

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
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTabDelegate;
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
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabShareUtils;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.function.Supplier;

/**
 * Drag and drop reorder - start dragging strip view. Subsequently drag out of, within and back onto
 * strip.
 */
@NullMarked
class SourceViewDragDropReorderStrategy extends ReorderStrategyBase {
    // Drag helpers
    private final TabStripDragHandler mTabStripDragHandler;
    private final ActionConfirmationManager mActionConfirmationManager;
    private final ReorderSubStrategy mTabSubStrategy;
    private final ReorderSubStrategy mMultiTabSubStrategy;
    private final ReorderSubStrategy mGroupSubStrategy;

    // View on strip being dragged.
    private @Nullable StripLayoutView mViewBeingDragged;

    // View offsetX when it was dragged off the strip. Used to re-position the view when dragged
    // back onto strip.
    private float mLastOffsetX;

    // Active sub-strategy based on interacting view class.
    private @Nullable ReorderSubStrategy mActiveSubStrategy;

    SourceViewDragDropReorderStrategy(
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
            TabStripDragHandler tabStripDragHandler,
            ActionConfirmationManager actionConfirmationManager,
            ReorderStrategy tabStrategy,
            ReorderStrategy multiTabStrategy,
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
        mTabStripDragHandler = tabStripDragHandler;
        mActionConfirmationManager = actionConfirmationManager;
        mTabSubStrategy = new TabReorderSubStrategy(tabStrategy);
        mMultiTabSubStrategy = new MultiTabReorderSubStrategy(multiTabStrategy);
        mGroupSubStrategy = new GroupReorderSubStrategy(groupStrategy);
    }

    /** Initiate Android Drag-Drop for interactingView. */
    @Override
    public void startReorderMode(
            StripLayoutView[] stripViews,
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle[] stripGroupTitles,
            StripLayoutView interactingView,
            PointF startPoint) {
        // Set initial state.
        mViewBeingDragged = interactingView;
        mLastOffsetX = 0.f;

        // Set the correct sub-strategy.
        if (interactingView instanceof StripLayoutTab tab) {
            if (mModel.isTabMultiSelected(tab.getTabId())
                    && mModel.getMultiSelectedTabsCount() > 1) {
                mActiveSubStrategy = mMultiTabSubStrategy;
            } else {
                mActiveSubStrategy = mTabSubStrategy;
            }
        } else if (interactingView instanceof StripLayoutGroupTitle) {
            mActiveSubStrategy = mGroupSubStrategy;
        }

        // Attempt to start a drag and drop action. If the drag successfully started, early-out.
        // Drag and drop for multi tab selection will be added later.
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
            assumeNonNull(mViewBeingDragged);
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
    public @Nullable StripLayoutView getInteractingView() {
        return mViewBeingDragged;
    }

    @Override
    public boolean shouldAllowAutoScroll() {
        // Do not allow auto-scroll when a pinned tab is dragged over unpinned tabs; pinned tabs can
        // only be dropped into the pinned section.
        return !TabStripDragHandler.isDraggingPinnedItem();
    }

    boolean isReorderingTab() {
        return mActiveSubStrategy == mTabSubStrategy || mActiveSubStrategy == mMultiTabSubStrategy;
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
        if (mTabGroupModelFilter.getTabModel().isIncognitoBranded()) {
            return false;
        }
        int tabId = draggedTab.getTabId();
        boolean draggingLastTabInGroup =
                StripLayoutUtils.isLastTabInGroup(mTabGroupModelFilter, tabId);
        boolean willSkipDialog =
                mActionConfirmationManager.willSkipUngroupTabAttempt()
                        && !isTabInCollaboration(tabId);
        return draggingLastTabInGroup && !willSkipDialog;
    }

    private boolean isTabInCollaboration(int tabId) {

        var profile = assumeNonNull(mTabGroupModelFilter.getTabModel().getProfile());
        @Nullable TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(profile);

        @Nullable String collaborationId =
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
                StripLayoutView interactingView,
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
        public @Nullable StripLayoutView getInteractingView() {
            return mWrappedStrategy.getInteractingView();
        }

        @Override
        public void reorderViewInDirection(
                StripLayoutTabDelegate tabDelegate,
                StripLayoutView[] stripViews,
                StripLayoutGroupTitle[] groupTitles,
                StripLayoutTab[] stripTabs,
                StripLayoutView reorderingView,
                boolean toRight) {
            // Intentionally no-op.
        }

        /**
         * Attempts to start the view tearing action through {@link TabStripDragHandler}.
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
                StripLayoutView interactingView,
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
            assumeNonNull(draggedTab);
            boolean draggedLastTabInGroupWithPrompt = shouldShowUserPrompt(draggedTab);
            if (draggedLastTabInGroupWithPrompt) {
                moveInteractingTabsOutOfGroup(
                        stripViews,
                        groupTitles,
                        Collections.singletonList(draggedTab),
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
            assumeNonNull(mViewBeingDragged);
            Tab tab = mModel.getTabById(((StripLayoutTab) mViewBeingDragged).getTabId());
            assert tab != null : "No matching Tab found.";
            return mTabStripDragHandler.startTabDragAction(
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
                    mGroupIdToHideSupplier.set(null);
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

    private abstract class MultiViewTearingSubStrategy extends ReorderSubStrategy {
        final List<StripLayoutView> mViewsBeingDragged = new ArrayList<>();

        MultiViewTearingSubStrategy(ReorderStrategy reorderStrategy) {
            super(reorderStrategy);
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
            assumeNonNull(mViewBeingDragged);
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
    }

    private class MultiTabReorderSubStrategy extends MultiViewTearingSubStrategy {
        MultiTabReorderSubStrategy(ReorderStrategy multiTabReorderStrategy) {
            super(multiTabReorderStrategy);
        }

        @Override
        boolean startViewDragAction(StripLayoutTab[] stripTabs, PointF startPoint) {
            // Populate the list of views being dragged.
            mViewsBeingDragged.clear();
            List<Tab> selectedTabs = new ArrayList<>();
            HashSet<Integer> tabIdsToUnselect = new HashSet();

            assumeNonNull(mViewBeingDragged);
            StripLayoutTab primaryStripTab = (StripLayoutTab) mViewBeingDragged;
            for (StripLayoutTab stripTab : stripTabs) {
                if (stripTab != null && mModel.isTabMultiSelected(stripTab.getTabId())) {
                    // TODO(crbug.com/441978834):  This is a temporary workaround: if the selection
                    // mixes pinned and unpinned tabs, only keep the tabs have the same pin state
                    // as the primary tab. To match desktop behavior for mixed pinned/unpinned
                    // tabs, when "ungather" them on drop we should:
                    // 1. When drop in pinned range: place pinned tabs at the drop point; snap
                    // unpinned
                    // tabs to the nearest valid indices.
                    // 2. Drop in unpinned range: place unpinned tabs at the drop point; move pinned
                    // tabs to the end of the pinned range.
                    if (stripTab.getIsPinned() == primaryStripTab.getIsPinned()) {
                        mViewsBeingDragged.add(stripTab);
                        selectedTabs.add(mModel.getTabById(stripTab.getTabId()));
                    } else {
                        tabIdsToUnselect.add(stripTab.getTabId());
                    }
                }
            }

            // Deselect the ones that don't move due to a different pin state. If this includes the
            // current tab, switch to the primary tab.
            if (tabIdsToUnselect.contains(TabModelUtils.getCurrentTabId(mModel))) {
                TabModelUtils.setIndex(
                        mModel, TabModelUtils.getTabIndexById(mModel, primaryStripTab.getTabId()));
            }
            if (!tabIdsToUnselect.isEmpty()) {
                mModel.setTabsMultiSelected(tabIdsToUnselect, /* isSelected= */ false);
            }

            if (mViewsBeingDragged.isEmpty()) return false;

            // The primary tab is the one being interacted with.
            Tab primaryTab = mModel.getTabById(primaryStripTab.getTabId());
            assert primaryTab != null : "No matching Tab found.";

            return mTabStripDragHandler.startMultiTabDragAction(
                    mContainerView,
                    selectedTabs,
                    primaryTab,
                    startPoint,
                    mViewBeingDragged.getDrawX(),
                    mViewBeingDragged.getWidth());
        }

        @Override
        void onStopViewDragAction(
                StripLayoutView[] stripViews, StripLayoutGroupTitle[] groupTitles) {
            // If the dragged views were re-parented, they will no longer be present in model.
            // If this is not the case, attempt to restore views to their original position.
            StripLayoutTab draggedTab = (StripLayoutTab) mViewBeingDragged;
            if (draggedTab != null
                    && mModel.getTabById(draggedTab.getTabId()) != null
                    && draggedTab.isDraggedOffStrip()) {
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

    private class GroupReorderSubStrategy extends MultiViewTearingSubStrategy {
        GroupReorderSubStrategy(ReorderStrategy groupReorderStrategy) {
            super(groupReorderStrategy);
        }

        @Override
        boolean startViewDragAction(StripLayoutTab[] stripTabs, PointF startPoint) {
            assumeNonNull(mViewBeingDragged);
            StripLayoutGroupTitle draggedGroupTitle = (StripLayoutGroupTitle) mViewBeingDragged;
            mViewsBeingDragged.add(draggedGroupTitle);
            mViewsBeingDragged.addAll(
                    StripLayoutUtils.getGroupedTabs(
                            mModel, stripTabs, draggedGroupTitle.getTabGroupId()));

            return mTabStripDragHandler.startGroupDragAction(
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

    @Nullable StripLayoutView getViewBeingDraggedForTesting() {
        return mViewBeingDragged;
    }

    float getDragLastOffsetXForTesting() {
        return mLastOffsetX;
    }
}

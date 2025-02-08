// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.ANIM_TAB_MOVE_MS;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.INVALID_TIME;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.TAB_GROUP_BOTTOM_INDICATOR_WIDTH_OFFSET;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.graphics.PointF;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;
import org.chromium.ui.base.LocalizationUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/** Delegate to manage the reordering logic for the tab strip. */
public class ReorderDelegate {
    // Constants.
    private static final int REORDER_SCROLL_NONE = 0;
    private static final int REORDER_SCROLL_LEFT = 1;
    private static final int REORDER_SCROLL_RIGHT = 2;

    private static final float REORDER_EDGE_SCROLL_MAX_SPEED_DP = 1000.f;
    private static final float REORDER_EDGE_SCROLL_START_MIN_DP = 87.4f;
    private static final float REORDER_EDGE_SCROLL_START_MAX_DP = 18.4f;

    @IntDef({
        ReorderType.DRAG_WITHIN_STRIP,
        ReorderType.START_DRAG_DROP,
        ReorderType.DRAG_ONTO_STRIP,
        ReorderType.DRAG_OUT_OF_STRIP
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ReorderType {
        /*
         * Interacting view belongs to strip and is being dragged via Android drag&drop.
         */
        int START_DRAG_DROP = 0;
        /*
         * View is reordered within strip.
         */
        int DRAG_WITHIN_STRIP = 1;
        /*
         * View (eg: tab dragged out of strip OR external view like tab from another strip)
         * is being dragged onto and reordered with-in strip for drop.
         */
        int DRAG_ONTO_STRIP = 2;
        /*
         * View (eg: strip's tab OR external view dragged onto strip) is dragged out of strip.
         */
        int DRAG_OUT_OF_STRIP = 3;
    }

    // Strip update delegate.
    public interface StripUpdateDelegate {

        /**
         * Update strip - resize all views on tab strip.
         *
         * @param animate Whether to animate the resize.
         * @param tabToAnimate Tab to additionally animate. Must be null if animate is false.
         * @param animateTabAdded Run tab added animation on tabToAnimate if true. Run tab closed
         *     animation if false.
         */
        void resizeTabStrip(boolean animate, StripLayoutTab tabToAnimate, boolean animateTabAdded);

        /**
         * Requests an update to strip (view properties etc) based on current state (eg: reorder,
         * scroll). Updated properties won't be available immediately.
         */
        void refresh();

        /**
         * Sets visibility for compositor buttons during reorder.
         *
         * @param visible Whether buttons should be visible.
         */
        void setCompositorButtonsVisible(boolean visible);
    }

    // Tab State.
    private TabGroupModelFilter mTabGroupModelFilter;
    private TabModel mModel;

    // Tab Strip State.
    private AnimationHost mAnimationHost;
    private StripUpdateDelegate mStripUpdateDelegate;
    private ScrollDelegate mScrollDelegate;
    private ObservableSupplierImpl<Integer> mGroupIdToHideSupplier;
    private View mContainerView;

    // Internal State.
    private boolean mInitialized;

    // Reorder State.
    private final ObservableSupplierImpl<Boolean> mInReorderModeSupplier =
            new ObservableSupplierImpl<>(/* initialValue= */ false);

    /** The last x-position we processed for reorder. */
    private float mLastReorderX;

    /** Supplier for current tab width. */
    private Supplier<Float> mTabWidthSupplier;

    private ReorderStrategy mActiveStrategy;
    private TabReorderStrategy mTabStrategy;
    private GroupReorderStrategy mGroupStrategy;
    @Nullable private SourceViewDragDropReorderStrategy mSourceViewDragDropReorderStrategy;
    @Nullable private ExternalViewDragDropReorderStrategy mExternalViewDragDropReorderStrategy;

    // Auto-scroll State.
    private long mLastReorderScrollTime;
    private int mReorderScrollState = REORDER_SCROLL_NONE;

    // ============================================================================================
    // Getters and setters
    // ============================================================================================

    public boolean getInReorderMode() {
        return Boolean.TRUE.equals(mInReorderModeSupplier.get());
    }

    public boolean isReorderingTab() {
        return getInReorderMode()
                && ((mActiveStrategy == mSourceViewDragDropReorderStrategy
                                && mSourceViewDragDropReorderStrategy.isReorderingTab())
                        || mActiveStrategy == mTabStrategy);
    }

    private boolean isReorderingForTabDrop() {
        return getInReorderMode() && mActiveStrategy == mExternalViewDragDropReorderStrategy;
    }

    private ReorderStrategy getReorderStrategy(
            StripLayoutView interactingView, @ReorderType int reorderType) {
        boolean instanceOfTab = interactingView instanceof StripLayoutTab;
        boolean instanceOfGroup = interactingView instanceof StripLayoutGroupTitle;
        boolean shouldDragDropGroup =
                instanceOfGroup
                        && ChromeFeatureList.isEnabled(
                                ChromeFeatureList.TAB_STRIP_GROUP_DRAG_DROP_ANDROID);
        if (mSourceViewDragDropReorderStrategy != null
                && (instanceOfTab || shouldDragDropGroup)
                && reorderType == ReorderType.START_DRAG_DROP) {
            return mSourceViewDragDropReorderStrategy;
        } else if ((instanceOfTab || shouldDragDropGroup)
                && reorderType == ReorderType.DRAG_ONTO_STRIP) {
            // Only external views can be dragged onto strip during startReorderMode.
            assert mExternalViewDragDropReorderStrategy != null;
            return mExternalViewDragDropReorderStrategy;
        } else {
            if (instanceOfTab) {
                return mTabStrategy;
            } else if (instanceOfGroup) {
                return mGroupStrategy;
            }
        }
        assert false : "Attempted to start reorder on an unexpected view type: " + interactingView;
        return null;
    }

    // ============================================================================================
    // Initialization
    // ============================================================================================

    /**
     * Passes the dependencies needed in this delegate. Passed here as they aren't ready on
     * instantiation.
     *
     * @param animationHost The {@link AnimationHost} for triggering animations.
     * @param stripUpdateDelegate The {@link StripUpdateDelegate} for refreshing all strip views.
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} for accessing tab state.
     * @param scrollDelegate The {@link ScrollDelegate} for updating scroll offset. actions, such as
     *     delete and ungroup.
     * @param tabDragSource The drag-drop manager {@link TabDragSource} for triggering Android
     *     drag-drop and listen to drag events. Builds and manages the drag shadow.
     * @param actionConfirmationManager The {@link ActionConfirmationManager} to show user prompts
     *     during reorder.
     * @param tabWidthSupplier The {@link Supplier} for tab width for reorder computations.
     * @param groupIdToHideSupplier The {@link ObservableSupplierImpl} for the group ID to hide.
     * @param containerView The tab strip container {@link View}.
     */
    public void initialize(
            AnimationHost animationHost,
            StripUpdateDelegate stripUpdateDelegate,
            TabGroupModelFilter tabGroupModelFilter,
            ScrollDelegate scrollDelegate,
            TabDragSource tabDragSource,
            ActionConfirmationManager actionConfirmationManager,
            Supplier<Float> tabWidthSupplier,
            ObservableSupplierImpl<Integer> groupIdToHideSupplier,
            View containerView) {
        mAnimationHost = animationHost;
        mStripUpdateDelegate = stripUpdateDelegate;
        mTabGroupModelFilter = tabGroupModelFilter;
        mScrollDelegate = scrollDelegate;
        mTabWidthSupplier = tabWidthSupplier;
        mGroupIdToHideSupplier = groupIdToHideSupplier;
        mContainerView = containerView;
        mModel = mTabGroupModelFilter.getTabModel();

        mTabStrategy =
                new TabReorderStrategy(
                        /* reorderDelegate= */ this,
                        mStripUpdateDelegate,
                        mAnimationHost,
                        mScrollDelegate,
                        mModel,
                        mTabGroupModelFilter,
                        mContainerView,
                        mGroupIdToHideSupplier,
                        mTabWidthSupplier);
        mGroupStrategy =
                new GroupReorderStrategy(
                        /* reorderDelegate= */ this,
                        mStripUpdateDelegate,
                        mAnimationHost,
                        mScrollDelegate,
                        mModel,
                        mTabGroupModelFilter,
                        mContainerView,
                        mGroupIdToHideSupplier,
                        mTabWidthSupplier);
        if (tabDragSource != null) {
            mSourceViewDragDropReorderStrategy =
                    new SourceViewDragDropReorderStrategy(
                            /* reorderDelegate= */ this,
                            mStripUpdateDelegate,
                            mAnimationHost,
                            mScrollDelegate,
                            mModel,
                            mTabGroupModelFilter,
                            mContainerView,
                            mGroupIdToHideSupplier,
                            mTabWidthSupplier,
                            tabDragSource,
                            actionConfirmationManager,
                            mTabStrategy,
                            mGroupStrategy);
            mExternalViewDragDropReorderStrategy =
                    new ExternalViewDragDropReorderStrategy(
                            /* reorderDelegate= */ this,
                            mStripUpdateDelegate,
                            mAnimationHost,
                            mScrollDelegate,
                            mModel,
                            mTabGroupModelFilter,
                            mContainerView,
                            mGroupIdToHideSupplier,
                            mTabWidthSupplier);
        }
        mInitialized = true;
    }

    // ============================================================================================
    // Reorder API
    // ============================================================================================

    /**
     * See {@link ReorderStrategy#startReorderMode}. Additional params:
     *
     * @param reorderType The type {@link ReorderType} of reorder to start.
     */
    public void startReorderMode(
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle[] stripGroupTitles,
            @NonNull StripLayoutView interactingView,
            PointF startPoint,
            @ReorderType int reorderType) {
        assert mInitialized && mActiveStrategy == null && !getInReorderMode();
        mActiveStrategy = getReorderStrategy(interactingView, reorderType);

        // Set initial state
        mInReorderModeSupplier.set(true);
        mLastReorderScrollTime = INVALID_TIME;
        mReorderScrollState = REORDER_SCROLL_NONE;
        mLastReorderX = startPoint.x;
        mStripUpdateDelegate.setCompositorButtonsVisible(false);

        mActiveStrategy.startReorderMode(stripTabs, stripGroupTitles, interactingView, startPoint);
    }

    /** See {@link ReorderStrategy#updateReorderPosition} */
    public void updateReorderPosition(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            float endX,
            float deltaX,
            @ReorderType int reorderType) {
        assert mActiveStrategy != null && getInReorderMode()
                : "Attempted to update reorder without an active Strategy.";
        // Return if accumulated delta is too small. This isn't the accumulated delta since the
        // beginning of the drag. It accumulates the delta X until a threshold is crossed and then
        // the event gets processed.
        float accumulatedDeltaX = endX - mLastReorderX;
        if (reorderType == ReorderType.DRAG_WITHIN_STRIP) {
            if (Math.abs(accumulatedDeltaX) < 1.f) {
                return;
            }
            // Update reorder scroll state / reorderX.
            updateReorderState(endX, deltaX);
        }
        mActiveStrategy.updateReorderPosition(
                stripViews, groupTitles, stripTabs, endX, accumulatedDeltaX, reorderType);
    }

    /**
     * Enables tab strip auto-scroll when view is dragged into gutters (strip ends) during reorder.
     * Handles reorder updates once auto-scroll ends.
     *
     * @param stripViews The list of {@link StripLayoutView}.
     * @param groupTitles The list of {@link StripLayoutGroupTitle}.
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param time The time when the update is invoked.
     * @param stripWidth The width of tab-strip. Used to compute auto-scroll speed.
     * @param leftMargin The start margin in tab-strip. Used to compute auto-scroll speed.
     * @param rightMargin The end margin in tab-strip. Used to compute auto-scroll speed.
     */
    public void updateReorderPositionAutoScroll(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            long time,
            float stripWidth,
            float leftMargin,
            float rightMargin) {
        assert mActiveStrategy != null && getInReorderMode()
                : "Attempted to update reorder without an active Strategy.";
        float scrollOffsetDelta =
                computeScrollOffsetDeltaForAutoScroll(time, stripWidth, leftMargin, rightMargin);
        if (scrollOffsetDelta != 0f) {
            float deltaX =
                    mScrollDelegate.setScrollOffset(
                            mScrollDelegate.getScrollOffset() + scrollOffsetDelta);
            if (mScrollDelegate.isFinished()) {
                mActiveStrategy.updateReorderPosition(
                        stripViews,
                        groupTitles,
                        stripTabs,
                        mLastReorderX,
                        deltaX,
                        ReorderType.DRAG_WITHIN_STRIP);
            }
            mStripUpdateDelegate.refresh();
        }
    }

    /** See {@link ReorderStrategy#stopReorderMode} */
    public void stopReorderMode(StripLayoutGroupTitle[] groupTitles, StripLayoutTab[] stripTabs) {
        assert mActiveStrategy != null && getInReorderMode()
                : "Attempted to stop reorder without an active Strategy.";
        mActiveStrategy.stopReorderMode(groupTitles, stripTabs);

        // Reset state.
        mReorderScrollState = REORDER_SCROLL_NONE;
        mInReorderModeSupplier.set(false);
        mStripUpdateDelegate.setCompositorButtonsVisible(true);
        mActiveStrategy = null;
    }

    private float computeScrollOffsetDeltaForAutoScroll(
            long time, float stripWidth, float leftMargin, float rightMargin) {
        // 1. Track the delta time since the last auto scroll.
        final float deltaSec =
                mLastReorderScrollTime == INVALID_TIME
                        ? 0.f
                        : (time - mLastReorderScrollTime) / 1000.f;
        mLastReorderScrollTime = time;

        // When we are reordering for tab drop, we are not offsetting the interacting tab. Instead,
        // we are adding a visual indicator (a gap between tabs) to indicate where the tab will be
        // added. As such, we need to base this on the most recent x-position of the drag, rather
        // than the interacting view's drawX.
        final float x =
                isReorderingForTabDrop()
                        ? StripLayoutUtils.adjustXForTabDrop(mLastReorderX, mTabWidthSupplier)
                        : mActiveStrategy.getInteractingView().getDrawX();

        // 2. Calculate the gutters for accelerating the scroll speed.
        // Speed: MAX    MIN                  MIN    MAX
        // |-------|======|--------------------|======|-------|
        final float dragRange = REORDER_EDGE_SCROLL_START_MAX_DP - REORDER_EDGE_SCROLL_START_MIN_DP;
        final float leftMinX = REORDER_EDGE_SCROLL_START_MIN_DP + leftMargin;
        final float leftMaxX = REORDER_EDGE_SCROLL_START_MAX_DP + leftMargin;
        final float rightMinX =
                stripWidth - leftMargin - rightMargin - REORDER_EDGE_SCROLL_START_MIN_DP;
        final float rightMaxX =
                stripWidth - leftMargin - rightMargin - REORDER_EDGE_SCROLL_START_MAX_DP;

        // 3. See if the current draw position is in one of the gutters and figure out how far in.
        // Note that we only allow scrolling in each direction if the user has already manually
        // moved that way.
        final float width =
                isReorderingForTabDrop()
                        ? mTabWidthSupplier.get()
                        : mActiveStrategy.getInteractingView().getWidth();
        float dragSpeedRatio = 0.f;
        if ((mReorderScrollState & REORDER_SCROLL_LEFT) != 0 && x < leftMinX) {
            dragSpeedRatio = -(leftMinX - Math.max(x, leftMaxX)) / dragRange;
        } else if ((mReorderScrollState & REORDER_SCROLL_RIGHT) != 0 && x + width > rightMinX) {
            dragSpeedRatio = (Math.min(x + width, rightMaxX) - rightMinX) / dragRange;
        }

        dragSpeedRatio = MathUtils.flipSignIf(dragSpeedRatio, LocalizationUtils.isLayoutRtl());
        if (dragSpeedRatio != 0.f) {
            // 4.a. We're in a gutter. Return scroll offset delta to update the scroll offset.
            float dragSpeed = REORDER_EDGE_SCROLL_MAX_SPEED_DP * dragSpeedRatio;
            return (dragSpeed * deltaSec);
        } else {
            // 4.b. We're not in a gutter.  Reset the scroll delta time tracker.
            mLastReorderScrollTime = INVALID_TIME;
            return 0f;
        }
    }

    public void addInReorderModeObserver(Callback<Boolean> observer) {
        mInReorderModeSupplier.addObserver(observer);
    }

    public void removeInReorderModeObserver(Callback<Boolean> observer) {
        mInReorderModeSupplier.removeObserver(observer);
    }

    /** Update and animate views for external view drop on strip. */
    public void handleTabDropForExternalView(
            StripLayoutGroupTitle[] groupTitles, int draggedTabId, int dropIndex) {
        assert mInitialized && mExternalViewDragDropReorderStrategy != null;
        mExternalViewDragDropReorderStrategy.handleDrop(groupTitles, draggedTabId, dropIndex);
    }

    // ============================================================================================
    // Margin helpers
    // ============================================================================================

    /**
     * Calculates the start and end margins needed to allow for reordering tabs into/out of groups
     * near the edge of the tab strip. 0 if the first/last tabs aren't grouped, respectively.
     *
     * @param stripTabs The list of {@link StripLayoutTab}.
     */
    public void setEdgeMarginsForReorder(StripLayoutTab[] stripTabs) {
        if (!mInitialized) return;
        ((ReorderStrategyBase) mActiveStrategy).setEdgeMarginsForReorder(stripTabs);
    }

    // ============================================================================================
    // Tab reorder helpers
    // ============================================================================================

    private class TabReorderStrategy extends ReorderStrategyBase {
        // Tab being reordered.
        private StripLayoutTab mInteractingTab;

        TabReorderStrategy(
                ReorderDelegate reorderDelegate,
                StripUpdateDelegate stripUpdateDelegate,
                AnimationHost animationHost,
                ScrollDelegate scrollDelegate,
                TabModel model,
                TabGroupModelFilter tabGroupModelFilter,
                View containerView,
                ObservableSupplierImpl<Integer> groupIdToHideSupplier,
                Supplier<Float> tabWidthSupplier) {
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
        }

        /** See {@link ReorderStrategy#startReorderMode} */
        @Override
        public void startReorderMode(
                StripLayoutTab[] stripTabs,
                StripLayoutGroupTitle[] stripGroupTitles,
                StripLayoutView interactingTab,
                PointF startPoint) {
            // TODO(crbug.com/394945056): Investigate moving to avoid re-emitting when dragging out,
            //  then back onto the source tab strip.
            RecordUserAction.record("MobileToolbarStartReorderTab");
            mInteractingTab = (StripLayoutTab) interactingTab;
            interactingTab.setIsForegrounded(/* isForegrounded= */ true);

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
                    groupTitles, stripTabs, mInteractingTab, offset, curIndex)) {
                // 3.a. We may have exited reorder mode to display the confirmation dialog. If so,
                // we should not set the new offset here, and instead let the tab slide back to its
                // idealX.
                if (!getInReorderMode()) return;

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
        public void stopReorderMode(
                StripLayoutGroupTitle[] groupTitles, StripLayoutTab[] stripTabs) {
            List<Animator> animatorList = new ArrayList<>();
            // 1. Reset the state variables.
            mReorderScrollState = REORDER_SCROLL_NONE;
            handleStopReorderMode(groupTitles, stripTabs, mInteractingTab, animatorList);
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
         * @param groupTitles The list of {@link StripLayoutGroupTitle}.
         * @param stripTabs The list of {@link StripLayoutTab}.
         * @param interactingTab The tab to reorder.
         * @param offset The distance the interacting tab has been dragged from its ideal position.
         * @return {@code True} if the reorder was successful. {@code False} otherwise.
         */
        private boolean reorderTabIfThresholdReached(
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
                        groupTitles,
                        stripTabs,
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
         * @param towardEnd True if the interacting tab is being dragged toward the end of the
         *     strip.
         */
        private void movePastCollapsedGroup(
                StripLayoutTab interactingTab,
                StripLayoutGroupTitle groupTitle,
                int curIndex,
                boolean towardEnd) {
            // Move the tab, then animate the adjacent group indicator sliding.
            int numTabsToSkip =
                    mTabGroupModelFilter.getRelatedTabCountForRootId(groupTitle.getRootId());
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
         * @param towardEnd True if the interacting tab is being dragged toward the end of the
         *     strip.
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
            animateGroupIndicatorForTabReorder(
                    groupTitle, /* isMovingOutOfGroup= */ false, towardEnd);
        }

        /** Returns the threshold to drag into a group. */
        private float getDragInThreshold() {
            return StripLayoutUtils.getHalfTabWidth(mTabWidthSupplier)
                    * StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;
        }
    }

    private void updateReorderState(float endX, float deltaX) {
        if (!LocalizationUtils.isLayoutRtl()) {
            if (deltaX >= 1.f) {
                mReorderScrollState |= REORDER_SCROLL_RIGHT;
            } else if (deltaX <= -1.f) {
                mReorderScrollState |= REORDER_SCROLL_LEFT;
            }
        } else {
            if (deltaX >= 1.f) {
                mReorderScrollState |= REORDER_SCROLL_LEFT;
            } else if (deltaX <= -1.f) {
                mReorderScrollState |= REORDER_SCROLL_RIGHT;
            }
        }

        mLastReorderX = endX;
    }

    /**
     * @param offset The offset of the current drag.
     * @return {@code true} if we're dragging towards the end of the strip. {@code false} otherwise.
     */
    private boolean isOffsetTowardEnd(float offset) {
        return (offset >= 0) ^ LocalizationUtils.isLayoutRtl();
    }

    /**
     * Adjusts the drag offset such that no apparent movement occurs for the view after a reorder is
     * processed. e.g. account for new idealX, scroll offset clamping, etc.
     *
     * @param interactingView The view that is being dragged for reorder.
     * @param offset The previous drag offset.
     * @param deltaX The change in drag offset since it was last processed.
     * @param oldIdealX The interacting view's {@code idealX} prior to the reorder.
     * @param oldScrollOffset The scroll offset prior to the reorder.
     * @param oldStartMargin The start margin prior to the reorder.
     * @return The new drag offset to prevent any apparent movement.
     */
    private float adjustOffsetAfterReorder(
            StripLayoutView interactingView,
            float offset,
            float deltaX,
            float oldIdealX,
            float oldScrollOffset,
            float oldStartMargin) {
        // Account for the new idealX after reorder.
        offset += oldIdealX - interactingView.getIdealX();
        // When the strip is scrolling, deltaX is already accounted for by idealX. This is because
        // it uses the scroll offset which has already been adjusted by deltaX.
        if (mLastReorderScrollTime != 0) offset -= deltaX;
        // When scrolled near the end of the strip, the scrollOffset being clamped can affect the
        // apparent position.
        oldScrollOffset += oldStartMargin - mScrollDelegate.getReorderStartMargin();
        float scrollOffsetDelta = mScrollDelegate.getScrollOffset() - oldScrollOffset;
        offset -= MathUtils.flipSignIf(scrollOffsetDelta, LocalizationUtils.isLayoutRtl());
        return offset;
    }

    /** Returns the threshold to swap the interacting views with an adjacent tab. */
    private float getTabSwapThreshold() {
        return StripLayoutUtils.getEffectiveTabWidth(mTabWidthSupplier)
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

    // ============================================================================================
    // Group reorder helpers
    // ============================================================================================

    private class GroupReorderStrategy extends ReorderStrategyBase {
        StripLayoutGroupTitle mInteractingGroupTitle;
        ArrayList<StripLayoutView> mInteractingViews = new ArrayList<>();
        StripLayoutTab mSelectedTab;
        StripLayoutTab mFirstTabInGroup;
        StripLayoutTab mLastTabInGroup;

        GroupReorderStrategy(
                ReorderDelegate reorderDelegate,
                StripUpdateDelegate stripUpdateDelegate,
                AnimationHost animationHost,
                ScrollDelegate scrollDelegate,
                TabModel model,
                TabGroupModelFilter tabGroupModelFilter,
                View containerView,
                ObservableSupplierImpl<Integer> groupIdToHideSupplier,
                Supplier<Float> tabWidthSupplier) {
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
        }

        @Override
        public void startReorderMode(
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
                @ReorderType int reorderType) {
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
            int lastTabIndex =
                    StripLayoutUtils.findIndexForTab(stripTabs, mLastTabInGroup.getTabId());
            if (firstTabIndex == 0) offset = Math.max(0, offset);
            if (lastTabIndex == stripTabs.length - 1) offset = Math.min(0, offset);
            for (StripLayoutView view : mInteractingViews) {
                view.setOffsetX(offset);
            }
        }

        @Override
        public void stopReorderMode(
                StripLayoutGroupTitle[] groupTitles, StripLayoutTab[] stripTabs) {
            assert getInReorderMode()
                    : "Tried to stop reorder mode, without first starting reorder mode.";

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
            int lastTabIndex =
                    StripLayoutUtils.findIndexForTab(stripTabs, mLastTabInGroup.getTabId());

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

    // ============================================================================================
    // IN-TEST
    // ============================================================================================

    public void setInReorderModeForTesting(boolean inReorderMode) {
        mInReorderModeSupplier.set(inReorderMode);
    }

    public float getLastReorderXForTesting() {
        return mLastReorderX;
    }

    public StripLayoutTab getInteractingTabForTesting() {
        return (StripLayoutTab) mActiveStrategy.getInteractingView();
    }
}

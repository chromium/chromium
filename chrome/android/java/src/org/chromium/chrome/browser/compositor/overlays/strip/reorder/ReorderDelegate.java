// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.INVALID_TIME;

import android.graphics.PointF;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.MathUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.overlays.strip.AnimationHost;
import org.chromium.chrome.browser.compositor.overlays.strip.ScrollDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.base.LocalizationUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
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
    private final ObservableSupplierImpl<Long> mLastReorderScrollTimeSupplier =
            new ObservableSupplierImpl<>(INVALID_TIME);
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
                        mTabWidthSupplier,
                        mLastReorderScrollTimeSupplier,
                        mInReorderModeSupplier);
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
                        mTabWidthSupplier,
                        mLastReorderScrollTimeSupplier);
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
                            mLastReorderScrollTimeSupplier,
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
                            mTabWidthSupplier,
                            mLastReorderScrollTimeSupplier);
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
            StripLayoutView[] stripViews,
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle[] stripGroupTitles,
            @NonNull StripLayoutView interactingView,
            PointF startPoint,
            @ReorderType int reorderType) {
        assert mInitialized && mActiveStrategy == null && !getInReorderMode();
        mActiveStrategy = getReorderStrategy(interactingView, reorderType);

        // Set initial state
        mInReorderModeSupplier.set(true);
        mLastReorderScrollTimeSupplier.set(INVALID_TIME);
        mReorderScrollState = REORDER_SCROLL_NONE;
        mLastReorderX = startPoint.x;
        mStripUpdateDelegate.setCompositorButtonsVisible(false);

        mActiveStrategy.startReorderMode(
                stripViews, stripTabs, stripGroupTitles, interactingView, startPoint);
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
    public void stopReorderMode(StripLayoutView[] stripViews, StripLayoutGroupTitle[] groupTitles) {
        assert mActiveStrategy != null && getInReorderMode()
                : "Attempted to stop reorder without an active Strategy.";
        mActiveStrategy.stopReorderMode(stripViews, groupTitles);

        // Reset state.
        mReorderScrollState = REORDER_SCROLL_NONE;
        mInReorderModeSupplier.set(false);
        mStripUpdateDelegate.setCompositorButtonsVisible(true);
        mActiveStrategy = null;
    }

    /** See {@link ReorderStrategy#getInteractingView()} */
    public StripLayoutView getInteractingView() {
        return mActiveStrategy != null ? mActiveStrategy.getInteractingView() : null;
    }

    private float computeScrollOffsetDeltaForAutoScroll(
            long time, float stripWidth, float leftMargin, float rightMargin) {
        // 1. Track the delta time since the last auto scroll.
        final float deltaSec =
                mLastReorderScrollTimeSupplier.get() == INVALID_TIME
                        ? 0.f
                        : (time - mLastReorderScrollTimeSupplier.get()) / 1000.f;
        mLastReorderScrollTimeSupplier.set(time);

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
            mLastReorderScrollTimeSupplier.set(INVALID_TIME);
            return 0f;
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

    /** Update and animate views for external tabs to drop on strip. */
    public boolean handleDropForExternalView(
            StripLayoutGroupTitle[] groupTitles, List<Integer> tabIds, int dropIndex) {
        assert mInitialized && mExternalViewDragDropReorderStrategy != null;
        return mExternalViewDragDropReorderStrategy.handleDrop(groupTitles, tabIds, dropIndex);
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

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.ANIM_TAB_MOVE_MS;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.ANIM_TAB_SLIDE_OUT_MS;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.INVALID_TIME;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.graphics.PointF;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.compositor.overlays.strip.StripTabModelActionListener.ActionType;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.interpolators.Interpolators;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Delegate to manage the reordering logic for the tab strip. */
public class ReorderDelegate {
    // Constants.
    private static final int REORDER_SCROLL_NONE = 0;
    private static final int REORDER_SCROLL_LEFT = 1;
    private static final int REORDER_SCROLL_RIGHT = 2;
    private static final int ANIM_FOLIO_DETACH_MS = 75;

    static final float FOLIO_ATTACHED_BOTTOM_MARGIN_DP = 0.f;
    private static final float FOLIO_ANIM_INTERMEDIATE_MARGIN_DP = -12.f;
    static final float FOLIO_DETACHED_BOTTOM_MARGIN_DP = 4.f;

    // Tab State.
    private TabGroupModelFilter mTabGroupModelFilter;
    private TabModel mModel;

    // Tab Strip State.
    private AnimationHost mAnimationHost;
    private ScrollDelegate mScrollDelegate;
    private ObservableSupplierImpl<Integer> mGroupIdToHideSupplier;
    private View mContainerView;

    // Internal State.
    private boolean mInitialized;

    // Reorder State.
    private final ObservableSupplierImpl<Boolean> mInReorderModeSupplier =
            new ObservableSupplierImpl<>(/* initialValue= */ false);
    // TODO(crbug.com/381285152): Cleanup mReorderingForTabDrop - duplicate of
    // ReorderType.EXTERNAL_VIEW_IN_STRIP.
    private boolean mReorderingForTabDrop;

    @IntDef({ReorderType.VIEW_IN_STRIP, ReorderType.VIEW_DRAG, ReorderType.EXTERNAL_VIEW_IN_STRIP})
    @Retention(RetentionPolicy.SOURCE)
    @interface ReorderType {
        /*
         * Interacting view belongs to and is reordered within strip.
         */
        int VIEW_IN_STRIP = 0;
        /*
         * Interacting view belongs to strip and could being dragged out-of / on-to strip.
         */
        int VIEW_DRAG = 1;
        /*
         * An external view (eg: tab from another strip) is being dragged onto and reordered
         * with-in strip for drop. Interacting view here is the view being hovered on by the
         * external view.
         */
        int EXTERNAL_VIEW_IN_STRIP = 2;
    }

    /** The last x-position we processed for reorder. */
    private float mLastReorderX;

    /** The effective tab width (accounting for overlap) at the time that we started reordering. */
    private float mEffectiveTabWidth;

    private StripLayoutTab mInteractingTab;

    private ReorderStrategy mActiveStrategy;
    private final TabReorderStrategy mTabStrategy = new TabReorderStrategy();
    private final GroupReorderStrategy mGroupStrategy = new GroupReorderStrategy();
    @Nullable private SourceViewDragDropReorderStrategy mDragDropStrategy;

    // Auto-scroll State.
    private long mLastReorderScrollTime;
    private int mReorderScrollState = REORDER_SCROLL_NONE;

    // ============================================================================================
    // Getters and setters
    // ============================================================================================

    boolean getInReorderMode() {
        return Boolean.TRUE.equals(mInReorderModeSupplier.get());
    }

    void setInReorderMode(boolean inReorderMode) {
        mInReorderModeSupplier.set(inReorderMode);
    }

    boolean getReorderingForTabDrop() {
        return mReorderingForTabDrop;
    }

    void setReorderingForTabDrop(boolean reorderingForTabDrop) {
        if (mReorderingForTabDrop != reorderingForTabDrop) {
            mReorderingForTabDrop = reorderingForTabDrop;
            if (mReorderingForTabDrop) onReorderingForTabDrop();
        }
    }

    float getLastReorderX() {
        return mLastReorderX;
    }

    void setLastReorderX(float x) {
        mLastReorderX = x;
    }

    StripLayoutTab getInteractingTab() {
        return mInteractingTab;
    }

    void setInteractingTab(StripLayoutTab interactingTab) {
        // Clear reordering state for previous interacting tab, if non-null.
        if (mInteractingTab != null) mInteractingTab.setIsReordering(false);
        // Set reordering state for newly interacting tab, if non-null.
        if (interactingTab != null) interactingTab.setIsReordering(true);

        mInteractingTab = interactingTab;
    }

    private ReorderStrategy getReorderStrategy(
            StripLayoutView interactingView, @ReorderType int reorderType) {
        if (mDragDropStrategy != null
                && interactingView instanceof StripLayoutTab
                && reorderType == ReorderType.VIEW_DRAG) {
            return mDragDropStrategy;
        } else if (interactingView instanceof StripLayoutTab) {
            return mTabStrategy;
        } else if (interactingView instanceof StripLayoutGroupTitle) {
            return mGroupStrategy;
        }
        assert false : "Attempted to start reorder on an unexpected view type: " + interactingView;
        return null;
    }

    // TODO(crbug.com/381285152): Remove get/set viewBeingDragged once DragDropReorderStrategy is
    // complete.
    StripLayoutView getViewBeingDragged() {
        if (mDragDropStrategy == null) return null;
        return mDragDropStrategy.mViewBeingDragged;
    }

    void setViewBeingDragged(StripLayoutView view) {
        if (mDragDropStrategy == null) return;
        mDragDropStrategy.mViewBeingDragged = view;
    }

    // TODO(crbug.com/381285152): Remove get/set dragLastOffsetX once DragDropReorderStrategy is
    // complete.
    float getDragLastOffsetX() {
        if (mDragDropStrategy == null) return 0f;
        return mDragDropStrategy.mLastOffsetX;
    }

    void setDragLastOffsetX(float offsetX) {
        if (mDragDropStrategy == null) return;
        mDragDropStrategy.mLastOffsetX = offsetX;
    }

    // ============================================================================================
    // Initialization
    // ============================================================================================

    /**
     * Passes the dependencies needed in this delegate. Passed here as they aren't ready on
     * instantiation.
     *
     * @param animationHost The {@link AnimationHost} for triggering animations.
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} for accessing tab state.
     * @param scrollDelegate The {@link ScrollDelegate} for updating scroll offset. actions, such as
     *     delete and ungroup.
     * @param tabDragSource The drag-drop manager {@link TabDragSource} for triggering Android
     *     drag-drop and listen to drag events. Builds and manages the drag shadow.
     * @param groupIdToHideSupplier The {@link ObservableSupplierImpl} for the group ID to hide.
     * @param containerView The tab strip container {@link View}.
     */
    void initialize(
            AnimationHost animationHost,
            TabGroupModelFilter tabGroupModelFilter,
            ScrollDelegate scrollDelegate,
            TabDragSource tabDragSource,
            ObservableSupplierImpl<Integer> groupIdToHideSupplier,
            View containerView) {
        mAnimationHost = animationHost;
        mTabGroupModelFilter = tabGroupModelFilter;
        mScrollDelegate = scrollDelegate;
        mGroupIdToHideSupplier = groupIdToHideSupplier;
        mContainerView = containerView;

        mModel = mTabGroupModelFilter.getTabModel();

        if (tabDragSource != null) {
            mDragDropStrategy = new SourceViewDragDropReorderStrategy(tabDragSource);
        }
        mInitialized = true;
    }

    // ============================================================================================
    // Reorder API
    // ============================================================================================

    /** See {@link ReorderStrategy#startReorderMode} */
    void startReorderMode(
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle[] stripGroupTitles,
            @NonNull StripLayoutView interactingView,
            float effectiveTabWidth,
            PointF startPoint,
            @ReorderType int reorderType) {
        assert mInitialized && mActiveStrategy == null && !getInReorderMode();
        mActiveStrategy = getReorderStrategy(interactingView, reorderType);
        mActiveStrategy.startReorderMode(
                stripTabs,
                stripGroupTitles,
                interactingView,
                effectiveTabWidth,
                startPoint,
                reorderType);
    }

    /** See {@link ReorderStrategy#updateReorderPosition} */
    void updateReorderPosition(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            float deltaX) {
        assert mActiveStrategy != null && getInReorderMode()
                : "Attempted to update reorder without an active Strategy.";
        mActiveStrategy.updateReorderPosition(stripViews, groupTitles, stripTabs, deltaX);
    }

    /** See {@link ReorderStrategy#stopReorderMode} */
    void stopReorderMode(StripLayoutGroupTitle[] groupTitles, StripLayoutTab[] stripTabs) {
        assert mActiveStrategy != null && getInReorderMode()
                : "Attempted to stop reorder without an active Strategy.";
        mActiveStrategy.stopReorderMode(groupTitles, stripTabs);
        mActiveStrategy = null;
    }

    void addInReorderModeObserver(Callback<Boolean> observer) {
        mInReorderModeSupplier.addObserver(observer);
    }

    void removeInReorderModeObserver(Callback<Boolean> observer) {
        mInReorderModeSupplier.removeObserver(observer);
    }

    // ============================================================================================
    // Auto-scroll state
    // ============================================================================================

    long getLastReorderScrollTime() {
        return mLastReorderScrollTime;
    }

    void setLastReorderScrollTime(long time) {
        mLastReorderScrollTime = time;
    }

    boolean canReorderScrollLeft() {
        return (mReorderScrollState & REORDER_SCROLL_LEFT) != 0;
    }

    boolean canReorderScrollRight() {
        return (mReorderScrollState & REORDER_SCROLL_RIGHT) != 0;
    }

    void allowReorderScrollLeft() {
        mReorderScrollState |= REORDER_SCROLL_LEFT;
    }

    void allowReorderScrollRight() {
        mReorderScrollState |= REORDER_SCROLL_RIGHT;
    }

    // ============================================================================================
    // Margin helpers
    // ============================================================================================

    /**
     * Calculates the start and end margins needed to allow for reordering tabs into/out of groups
     * near the edge of the tab strip. 0 if the first/last tabs aren't grouped, respectively.
     *
     * @param firstTab The first {@link StripLayoutTab}.
     * @param lastTab The last {@link StripLayoutTab}.
     */
    void setEdgeMarginsForReorder(StripLayoutTab firstTab, StripLayoutTab lastTab) {
        if (!mInitialized) return;
        float marginWidth = getHalfTabWidth() * REORDER_OVERLAP_SWITCH_PERCENTAGE;

        // 1. Set the start margin - margin is applied by updating scrollOffset.
        boolean firstTabIsInGroup =
                mTabGroupModelFilter.isTabInTabGroup(mModel.getTabById(firstTab.getTabId()));
        mScrollDelegate.setReorderStartMargin(firstTabIsInGroup ? marginWidth : 0.f);

        // 2. Set the trailing margin.
        boolean lastTabIsInGroup =
                mTabGroupModelFilter.isTabInTabGroup(mModel.getTabById(lastTab.getTabId()));
        lastTab.setTrailingMargin((lastTabIsInGroup && !lastTab.isCollapsed()) ? marginWidth : 0.f);
    }

    /**
     * Sets the trailing margin for the current tab. Update bottom indicator width for Tab Group
     * Indicators and animates if necessary.
     *
     * @param tab The tab to update.
     * @param groupTitle The group title associated with the tab. Null if tab is not grouped.
     * @param shouldHaveTrailingMargin Whether the tab should have a trailing margin or not.
     * @param animationList The list to add the animation to, or {@code null} if not animating.
     * @return Whether or not the trailing margin for the given tab actually changed.
     */
    boolean setTrailingMarginForTab(
            StripLayoutTab tab,
            StripLayoutGroupTitle groupTitle,
            boolean shouldHaveTrailingMargin,
            @NonNull List<Animator> animationList) {
        // Avoid triggering updates if trailing margin isn't actually changing.
        float trailingMargin = shouldHaveTrailingMargin ? getHalfTabWidth() : 0.f;
        if (tab.getTrailingMargin() == trailingMargin) return false;

        // Update group title bottom indicator width if needed.
        if (groupTitle != null) {
            float startWidth = groupTitle.getBottomIndicatorWidth();
            float endWidth =
                    StripLayoutUtils.calculateBottomIndicatorWidth(
                                    groupTitle,
                                    StripLayoutUtils.getNumOfTabsInGroup(
                                            mTabGroupModelFilter, groupTitle),
                                    mEffectiveTabWidth)
                            + trailingMargin;

            animationList.add(
                    CompositorAnimator.ofFloatProperty(
                            mAnimationHost.getAnimationHandler(),
                            groupTitle,
                            StripLayoutGroupTitle.BOTTOM_INDICATOR_WIDTH,
                            startWidth,
                            endWidth,
                            ANIM_TAB_SLIDE_OUT_MS));
        }

        // Set new trailing margin.
        animationList.add(
                CompositorAnimator.ofFloatProperty(
                        mAnimationHost.getAnimationHandler(),
                        tab,
                        StripLayoutTab.TRAILING_MARGIN,
                        tab.getTrailingMargin(),
                        trailingMargin,
                        ANIM_TAB_SLIDE_OUT_MS));

        return true;
    }

    private void resetTabGroupMargins(
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            @NonNull ArrayList<Animator> animationList) {
        assert !getInReorderMode();

        // TODO(crbug.com/372546700): Investigate only resetting first and last margin, as we now
        //  don't use trailing margins to demarcate tab group bounds.
        for (int i = 0; i < stripTabs.length; i++) {
            final StripLayoutTab stripTab = stripTabs[i];
            final Tab tab = mModel.getTabById(stripTab.getTabId());
            if (tab == null) continue;
            final StripLayoutGroupTitle groupTitle =
                    StripLayoutUtils.findGroupTitle(groupTitles, tab.getRootId());

            setTrailingMarginForTab(
                    stripTab, groupTitle, /* shouldHaveTrailingMargin= */ false, animationList);
        }
        mScrollDelegate.setReorderStartMargin(/* newStartMargin= */ 0.f);
    }

    // ============================================================================================
    // Tab reorder helpers
    // ============================================================================================

    private class TabReorderStrategy implements ReorderStrategy {

        /** See {@link ReorderStrategy#startReorderMode} */
        @Override
        public void startReorderMode(
                StripLayoutTab[] stripTabs,
                StripLayoutGroupTitle[] stripGroupTitles,
                StripLayoutView interactingTab,
                float effectiveTabWidth,
                PointF startPoint,
                @ReorderType int reorderType) {
            RecordUserAction.record("MobileToolbarStartReorderTab");
            setInteractingTab((StripLayoutTab) interactingTab);

            // 1. Set reorder mode to true before selecting this tab to prevent unnecessarily
            // triggering #bringSelectedTabToVisibleArea for edge tabs when the tab strip is full.
            setInReorderMode(true);

            // 2. Select this tab so that it is always in the foreground.
            TabModelUtils.setIndex(
                    mModel, TabModelUtils.getTabIndexById(mModel, mInteractingTab.getTabId()));

            // 3. Set initial state.
            prepareStripForReorder(stripTabs, effectiveTabWidth, startPoint.x);

            // 4. Lift the container off the toolbar and perform haptic feedback.
            ArrayList<Animator> animationList = new ArrayList<>();
            updateTabAttachState(mInteractingTab, /* attached= */ false, animationList);
            StripLayoutUtils.performHapticFeedback(mContainerView);

            // 5. Kick-off animations and request an update.
            mAnimationHost.startAnimations(animationList, /* listener= */ null);
        }

        @Override
        public void updateReorderPosition(
                StripLayoutView[] stripViews,
                StripLayoutGroupTitle[] groupTitles,
                StripLayoutTab[] stripTabs,
                float deltaX) {
            if (!getInReorderMode() || mInteractingTab == null || mReorderingForTabDrop) return;

            int curIndex = StripLayoutUtils.findIndexForTab(stripTabs, mInteractingTab.getTabId());
            if (curIndex == TabModel.INVALID_TAB_INDEX) return;

            // 1. Compute drag position.
            float oldIdealX = mInteractingTab.getIdealX();
            float oldScrollOffset = mScrollDelegate.getScrollOffset();
            float offset = mInteractingTab.getOffsetX() + deltaX;

            // 2. Attempt to move the tab. If successful, update other relevant properties.
            boolean isRtl = LocalizationUtils.isLayoutRtl();
            if (reorderTabIfThresholdReached(groupTitles, stripTabs, offset, curIndex)) {
                // 2.a. We may have exited reorder mode to display the confirmation dialog. If so,
                // we should not set the new offset here, and instead let the tab slide back to its
                // idealX.
                if (!getInReorderMode()) return;
                // 2.b. Since we just moved the tab we're dragging, adjust its offset so it stays in
                // the same apparent position.
                offset += oldIdealX - mInteractingTab.getIdealX();
                // 2.c. When the strip is scrolling, deltaX is already accounted for by idealX. This
                // is because it uses the scroll offset which has already been adjusted by deltaX.
                if (mLastReorderScrollTime != 0) offset -= deltaX;
                // 2.d. Group titles can affect minScrollOffset. When scrolled near the end of the
                // strip, the scrollOffset being clamped can affect the apparent position.
                offset -=
                        MathUtils.flipSignIf(
                                (mScrollDelegate.getScrollOffset() - oldScrollOffset), isRtl);
            }

            // 3. Limit offset based on tab position. First tab can't drag left, last tab can't drag
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
            assert getInReorderMode()
                    : "Tried to stop reorder mode, without first starting reorder mode.";
            ArrayList<Animator> animationList = new ArrayList<>();

            // 1. Reset the state variables.
            mReorderScrollState = REORDER_SCROLL_NONE;
            setInReorderMode(false);

            // 2. Reset the interacting view (clear any offset and reattach the container).
            mAnimationHost.finishAnimationsAndPushTabUpdates();
            if (mInteractingTab != null) {
                // TODO(crbug.com/372546700): mInteractingTab may be null if reordering for tab
                // drop.
                animationList.add(
                        CompositorAnimator.ofFloatProperty(
                                mAnimationHost.getAnimationHandler(),
                                mInteractingTab,
                                StripLayoutView.X_OFFSET,
                                mInteractingTab.getOffsetX(),
                                0f,
                                ANIM_TAB_MOVE_MS));

                // Skip reattachment for tab drop to avoid exposing bottom indicator underneath the
                // tab container.
                if (!mReorderingForTabDrop || !mInteractingTab.getFolioAttached()) {
                    updateTabAttachState(mInteractingTab, true, animationList);
                }
            }

            // 3. Clear any tab group margins.
            resetTabGroupMargins(groupTitles, stripTabs, animationList);

            // 4. Clear the interacting view.
            setInteractingTab(null);

            // 5. Reset the tab drop state. Must occur after the rest of the state is reset, since
            // some logic depends on these values.
            mReorderingForTabDrop = false;

            // 6. Start animations.
            mAnimationHost.startAnimations(animationList, /* listener= */ null);
        }
    }

    void prepareStripForReorder(StripLayoutTab[] stripTabs, float effectiveTabWidth, float startX) {
        // 1. Set initial state parameters.
        mAnimationHost.finishAnimationsAndPushTabUpdates();
        mLastReorderScrollTime = INVALID_TIME;
        mReorderScrollState = REORDER_SCROLL_NONE;
        mEffectiveTabWidth = effectiveTabWidth;
        mLastReorderX = startX;

        // 2. Set edge margins.
        setEdgeMarginsForReorder(stripTabs[0], stripTabs[stripTabs.length - 1]);
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
     * @param offset The distance the interacting tab has been dragged from its ideal position.
     * @return {@code True} if the reorder was successful. {@code False} otherwise.
     */
    private boolean reorderTabIfThresholdReached(
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            float offset,
            int curIndex) {
        boolean towardEnd = (offset >= 0) ^ LocalizationUtils.isLayoutRtl();
        Tab curTab = mModel.getTabAt(curIndex);
        Tab adjTab = mModel.getTabAt(curIndex + (towardEnd ? 1 : -1));
        boolean isInGroup = mTabGroupModelFilter.isTabInTabGroup(curTab);
        boolean mayDragInOrOutOfGroup =
                adjTab == null
                        ? isInGroup
                        : StripLayoutUtils.notRelatedAndEitherTabInGroup(
                                mTabGroupModelFilter, curTab, adjTab);

        // Case A: Not interacting with tab groups.
        if (!mayDragInOrOutOfGroup) {
            return maybeSwapTab(stripTabs, offset, curIndex);
        }

        // Case B: Maybe drag out of group.
        if (isInGroup) {
            StripLayoutGroupTitle interactingGroupTitle =
                    StripLayoutUtils.findGroupTitle(groupTitles, curTab.getRootId());
            float threshold = getDragOutThreshold(interactingGroupTitle, towardEnd);
            if (Math.abs(offset) <= threshold) return false;

            moveInteractingTabOutOfGroup(groupTitles, stripTabs, interactingGroupTitle, towardEnd);
            return true;
        }

        StripLayoutGroupTitle interactingGroupTitle =
                StripLayoutUtils.findGroupTitle(groupTitles, adjTab.getRootId());
        if (interactingGroupTitle.isCollapsed()) {
            // Case C.1: Maybe drag past collapsed group.
            float threshold = interactingGroupTitle.getWidth() * REORDER_OVERLAP_SWITCH_PERCENTAGE;
            if (Math.abs(offset) <= threshold) return false;

            movePastCollapsedGroup(interactingGroupTitle, curIndex, towardEnd);
            return true;
        } else {
            // Case C.2: Maybe merge to group.
            if (Math.abs(offset) <= getDragInThreshold()) return false;

            mergeInteractingTabToGroup(adjTab.getId(), interactingGroupTitle, towardEnd);
            return true;
        }
    }

    /**
     * Attempts to move the interacting tab out of its group. May prompt the user with a
     * confirmation dialog if the tab removal will result in a group deletion. Animates accordingly.
     *
     * @param groupTitles The list of {@link StripLayoutGroupTitle}.
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param groupTitle The title of the group the interacting tab is attempting to move out of.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     */
    private void moveInteractingTabOutOfGroup(
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle groupTitle,
            boolean towardEnd) {
        final int tabId = mInteractingTab.getTabId();
        // Exit reorder mode if the dialog will show. Tab drag and drop is cancelled elsewhere.
        Runnable beforeSyncDialogRunnable = () -> stopReorderMode(groupTitles, stripTabs);
        Runnable onSuccess =
                () -> RecordUserAction.record("MobileToolbarReorderTab.TabRemovedFromGroup");

        Tab tab = mModel.getTabById(tabId);
        // When dragging the last tab out of group, the tab group delete dialog will show and we
        // will hide the indicators for the interacting tab group until the user confirms the next
        // action. e.g delete tab group when user confirms the delete, or restore indicators back on
        // strip when user cancel the delete.
        StripTabModelActionListener listener =
                new StripTabModelActionListener(
                        tab.getRootId(),
                        ActionType.REORDER,
                        mGroupIdToHideSupplier,
                        mContainerView,
                        beforeSyncDialogRunnable,
                        onSuccess);
        mTabGroupModelFilter
                .getTabUngrouper()
                .ungroupTabs(
                        Collections.singletonList(tab),
                        towardEnd,
                        /* allowDialog= */ true,
                        listener);

        // Run indicator animations. Find the group title after handling the removal, since the
        // group may have been deleted OR the rootID may have changed.
        if (StripLayoutUtils.arrayContains(groupTitles, groupTitle)) {
            animateGroupIndicatorForTabReorder(
                    groupTitle, /* isMovingOutOfGroup= */ true, towardEnd);
        }
    }

    /**
     * Merges the interacting tab to the given group. Animates accordingly.
     *
     * @param destinationTabId The tab ID to merge the interacting tab to.
     * @param groupTitle The title of the group the interacting tab is attempting to merge to.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     */
    private void mergeInteractingTabToGroup(
            int destinationTabId, StripLayoutGroupTitle groupTitle, boolean towardEnd) {
        mTabGroupModelFilter.mergeTabsToGroup(
                mInteractingTab.getTabId(), destinationTabId, /* skipUpdateTabModel= */ true);
        RecordUserAction.record("MobileToolbarReorderTab.TabAddedToGroup");

        // Animate the group indicator after updating the tab model.
        animateGroupIndicatorForTabReorder(groupTitle, /* isMovingOutOfGroup= */ false, towardEnd);
    }

    /**
     * Moves the interacting tab past the adjacent collapsed group. Animates accordingly.
     *
     * @param groupTitle The collapsed group title we are attempting to drag past.
     * @param curIndex The index of the interacting tab.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     */
    private void movePastCollapsedGroup(
            StripLayoutGroupTitle groupTitle, int curIndex, boolean towardEnd) {
        // Move the tab, then animate the adjacent group indicator sliding.
        int numTabsToSkip =
                mTabGroupModelFilter.getRelatedTabCountForRootId(groupTitle.getRootId());
        int destIndex = towardEnd ? curIndex + 1 + numTabsToSkip : curIndex - numTabsToSkip;
        mModel.moveTab(mInteractingTab.getTabId(), destIndex);
        animateViewSliding(groupTitle);
    }

    /**
     * Swaps the interacting tab with the adjacent tab, if the drag threshold has been reached.
     * Animates accordingly.
     *
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param offset The distance the interacting tab has been dragged from its ideal position.
     * @param curIndex The index of the interacting tab.
     * @return {@code True} if the reorder was successful. {@code False} otherwise.
     */
    private boolean maybeSwapTab(StripLayoutTab[] stripTabs, float offset, int curIndex) {
        // TODO(crbug.com/372546700): Migrate to the pattern we use for the other reorder cases.
        //  i.e. check if we've reached the drag threshold in #reorderTabIfThresholdReached.
        final float moveThreshold = REORDER_OVERLAP_SWITCH_PERCENTAGE * mEffectiveTabWidth;
        boolean pastLeftThreshold = offset < -moveThreshold;
        boolean pastRightThreshold = offset > moveThreshold;
        boolean isNotRightMost = curIndex < stripTabs.length - 1;
        boolean isNotLeftMost = curIndex > 0;

        if (LocalizationUtils.isLayoutRtl()) {
            boolean oldLeft = pastLeftThreshold;
            pastLeftThreshold = pastRightThreshold;
            pastRightThreshold = oldLeft;
        }

        int destIndex = Tab.INVALID_TAB_ID;
        if (pastRightThreshold && isNotRightMost) {
            destIndex = curIndex + 2;
        } else if (pastLeftThreshold && isNotLeftMost) {
            destIndex = curIndex - 1;
        }

        if (destIndex == Tab.INVALID_TAB_ID) return false;

        // Move the tab, then animate the adjacent tab sliding.
        mModel.moveTab(mInteractingTab.getTabId(), destIndex);
        animateViewSliding(stripTabs[curIndex]);
        return true;
    }

    /**
     * @param groupTitle The group title for the desired group. Must not be null.
     * @param towardEnd True if dragging towards the end of the strip.
     * @return The threshold to drag out of a group.
     */
    private float getDragOutThreshold(StripLayoutGroupTitle groupTitle, boolean towardEnd) {
        float dragOutThreshold = getHalfTabWidth() * REORDER_OVERLAP_SWITCH_PERCENTAGE;
        return dragOutThreshold + (towardEnd ? 0 : groupTitle.getWidth());
    }

    /**
     * @return The threshold to drag into a group.
     */
    private float getDragInThreshold() {
        return getHalfTabWidth() * REORDER_OVERLAP_SWITCH_PERCENTAGE;
    }

    /**
     * @return Half of mEffectiveTabWidth.
     */
    private float getHalfTabWidth() {
        return mEffectiveTabWidth / 2;
    }

    // ============================================================================================
    // Group reorder helpers
    // ============================================================================================

    private static class GroupReorderStrategy implements ReorderStrategy {
        @Override
        public void startReorderMode(
                StripLayoutTab[] stripTabs,
                StripLayoutGroupTitle[] stripGroupTitles,
                @NonNull StripLayoutView interactingView,
                float effectiveTabWidth,
                PointF startPoint,
                @ReorderType int reorderType) {
            // TODO(crbug.com/376069497): Implement.
        }

        @Override
        public void updateReorderPosition(
                StripLayoutView[] stripViews,
                StripLayoutGroupTitle[] groupTitles,
                StripLayoutTab[] stripTabs,
                float deltaX) {
            // TODO(crbug.com/376069497): Implement.
        }

        @Override
        public void stopReorderMode(
                StripLayoutGroupTitle[] groupTitles, StripLayoutTab[] stripTabs) {
            // TODO(crbug.com/376069497): Implement.
        }
    }

    // ============================================================================================
    // Drag and drop reorder - drag external view onto / out of and within strip.
    // ============================================================================================

    void onReorderingForTabDrop() {
        // TODO(crbug.com/381285152): Implement separate strategy for Drag and Drop on destination
        //  tab strip. This is only needed because we reuse the TabReorderStrategy#stopReorder for
        //  DnD, but can likely be replaced by implementing a DnD-specific ReorderStrategy.
        mActiveStrategy = mTabStrategy;
    }

    // ============================================================================================
    // Drag and drop reorder - start dragging strip view. Subsequently drag out of,
    // within and back onto strip.
    // ============================================================================================

    private class SourceViewDragDropReorderStrategy implements ReorderStrategy {
        // Drag helpers
        private final TabDragSource mTabDragSource;

        // View on strip being dragged.
        private StripLayoutView mViewBeingDragged;
        // View offsetX when it was dragged off the strip. Used to re-position the view when dragged
        // back onto strip.
        private float mLastOffsetX;

        public SourceViewDragDropReorderStrategy(@NonNull TabDragSource tabDragSource) {
            mTabDragSource = tabDragSource;
        }

        /** Initiate Android Drag-Drop for interactingView. */
        @Override
        public void startReorderMode(
                StripLayoutTab[] stripTabs,
                StripLayoutGroupTitle[] stripGroupTitles,
                @NonNull StripLayoutView interactingView,
                float effectiveTabWidth,
                PointF startPoint,
                @ReorderType int reorderType) {
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
                // Set active strategy to null since current impl falls back to TabStrategy.
                // TODO(crbug.com/381285152): Remove once updateReorder() is implemented.
                mActiveStrategy = null;
            } else {
                // Drag did not start. Stop this strategy to reset state.
                // TODO(crbug.com/381285152): Call ReorderDelegate#stopReorderMode instead to
                // cleanup any parent state and reset activeStrategy.
                stopReorderMode(stripGroupTitles, stripTabs);
                mActiveStrategy = null;

                // Fallback to reorder view in strip.
                ReorderDelegate.this.startReorderMode(
                        stripTabs,
                        stripGroupTitles,
                        interactingView,
                        effectiveTabWidth,
                        startPoint,
                        ReorderType.VIEW_IN_STRIP);
            }
        }

        @Override
        public void updateReorderPosition(
                StripLayoutView[] stripViews,
                StripLayoutGroupTitle[] groupTitles,
                StripLayoutTab[] stripTabs,
                float deltaX) {
            // TODO(crbug.com/381285152): Implement.
        }

        @Override
        public void stopReorderMode(
                StripLayoutGroupTitle[] groupTitles, StripLayoutTab[] stripTabs) {
            // TODO(crbug.com/381285152): Implement.
            mViewBeingDragged = null;
            mLastOffsetX = 0;
        }
    }

    // ============================================================================================
    // Animation helpers
    // ============================================================================================

    /**
     * Animates a group indicator after a tab has been dragged out of or into its group and the
     * {@link TabGroupModelFilter} has been updated.
     *
     * @param groupTitle the group title that is sliding for tab reorder.
     * @param isMovingOutOfGroup Whether the action is merging/removing a tab to/from a group.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     */
    private void animateGroupIndicatorForTabReorder(
            StripLayoutGroupTitle groupTitle, boolean isMovingOutOfGroup, boolean towardEnd) {
        List<Animator> animators = new ArrayList<>();

        // Add the group title swapping animation if the tab is passing through group title.
        boolean throughGroupTitle = isMovingOutOfGroup ^ towardEnd;
        if (throughGroupTitle) animators.add(getViewSlidingAnimator(groupTitle));

        // Update bottom indicator width.
        updateBottomIndicatorWidthForTabReorder(
                mAnimationHost.getAnimationHandler(),
                mTabGroupModelFilter,
                groupTitle,
                isMovingOutOfGroup,
                throughGroupTitle,
                animators);

        mAnimationHost.startAnimations(animators, /* listener= */ null);
    }

    /**
     * Creates & starts a sliding {@link CompositorAnimator} for the given {@link StripLayoutView}.
     *
     * @param view The {@link StripLayoutView} to create a sliding {@link CompositorAnimator} for.
     */
    private void animateViewSliding(StripLayoutView view) {
        List<Animator> animators = new ArrayList<>();
        animators.add(getViewSlidingAnimator(view));
        mAnimationHost.startAnimations(animators, /* listener= */ null);
    }

    /**
     * @param view The {@link StripLayoutView} to create a sliding {@link CompositorAnimator} for.
     * @return The sliding {@link CompositorAnimator}.
     */
    private Animator getViewSlidingAnimator(StripLayoutView view) {
        return CompositorAnimator.ofFloatProperty(
                mAnimationHost.getAnimationHandler(),
                view,
                StripLayoutView.X_OFFSET,
                /* startValue= */ view.getDrawX() - view.getIdealX(),
                /* endValue= */ 0,
                ANIM_TAB_MOVE_MS);
    }

    /**
     * Set the new bottom indicator width after a tab has been merged to or moved out of a tab
     * group. Animate iff a list of animators is provided.
     *
     * @param animationHandler The {@link CompositorAnimationHandler}.
     * @param modelFilter The {@link TabGroupModelFilter}.
     * @param groupTitle The {@link StripLayoutGroupTitle} of the interacting group.
     * @param isMovingOutOfGroup Whether the action is merging/removing a tab to/from a group.
     * @param throughGroupTitle True if the tab is passing the {@link StripLayoutGroupTitle}.
     * @param animators The list of animators to add to. If {@code null}, then immediately set the
     *     new width instead of animating to it.
     */
    void updateBottomIndicatorWidthForTabReorder(
            CompositorAnimationHandler animationHandler,
            TabGroupModelFilter modelFilter,
            StripLayoutGroupTitle groupTitle,
            boolean isMovingOutOfGroup,
            boolean throughGroupTitle,
            @NonNull List<Animator> animators) {
        float endWidth =
                StripLayoutUtils.calculateBottomIndicatorWidth(
                        groupTitle,
                        StripLayoutUtils.getNumOfTabsInGroup(modelFilter, groupTitle),
                        mEffectiveTabWidth);
        float startWidth = endWidth + MathUtils.flipSignIf(mEffectiveTabWidth, !isMovingOutOfGroup);

        animators.add(
                CompositorAnimator.ofFloatProperty(
                        animationHandler,
                        groupTitle,
                        StripLayoutGroupTitle.BOTTOM_INDICATOR_WIDTH,
                        startWidth,
                        endWidth,
                        throughGroupTitle ? ANIM_TAB_MOVE_MS : ANIM_TAB_SLIDE_OUT_MS));
    }

    @VisibleForTesting
    void updateTabAttachState(
            StripLayoutTab tab, boolean attached, @NonNull ArrayList<Animator> animationList) {
        float startValue =
                attached ? FOLIO_DETACHED_BOTTOM_MARGIN_DP : FOLIO_ATTACHED_BOTTOM_MARGIN_DP;
        float intermediateValue = FOLIO_ANIM_INTERMEDIATE_MARGIN_DP;
        float endValue =
                attached ? FOLIO_ATTACHED_BOTTOM_MARGIN_DP : FOLIO_DETACHED_BOTTOM_MARGIN_DP;

        ArrayList<Animator> attachAnimationList = new ArrayList<>();
        CompositorAnimator dropAnimation =
                CompositorAnimator.ofFloatProperty(
                        mAnimationHost.getAnimationHandler(),
                        tab,
                        StripLayoutTab.BOTTOM_MARGIN,
                        startValue,
                        intermediateValue,
                        ANIM_FOLIO_DETACH_MS,
                        Interpolators.EMPHASIZED_ACCELERATE);
        CompositorAnimator riseAnimation =
                CompositorAnimator.ofFloatProperty(
                        mAnimationHost.getAnimationHandler(),
                        tab,
                        StripLayoutTab.BOTTOM_MARGIN,
                        intermediateValue,
                        endValue,
                        ANIM_FOLIO_DETACH_MS,
                        Interpolators.EMPHASIZED_DECELERATE);
        dropAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        tab.setFolioAttached(attached);
                    }
                });
        attachAnimationList.add(dropAnimation);
        attachAnimationList.add(riseAnimation);

        AnimatorSet set = new AnimatorSet();
        set.playSequentially(attachAnimationList);
        animationList.add(set);
    }
}

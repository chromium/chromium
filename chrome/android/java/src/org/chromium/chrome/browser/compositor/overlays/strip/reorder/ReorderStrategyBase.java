// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.ANIM_TAB_MOVE_MS;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.ANIM_TAB_SLIDE_OUT_MS;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.TAB_GROUP_BOTTOM_INDICATOR_WIDTH_OFFSET;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
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
import org.chromium.chrome.browser.compositor.overlays.strip.StripTabModelActionListener;
import org.chromium.chrome.browser.compositor.overlays.strip.StripTabModelActionListener.ActionType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.StripUpdateDelegate;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.ui.base.LocalizationUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Base class for {@link ReorderStrategy} implementations. */
@NullMarked
abstract class ReorderStrategyBase implements ReorderStrategy {

    // Constants
    private static final int ANIM_FOLIO_DETACH_MS = 75;
    private static final float FOLIO_ANIM_INTERMEDIATE_MARGIN_DP = -12.f;

    // Delegates
    protected final ReorderDelegate mReorderDelegate;
    protected final StripUpdateDelegate mStripUpdateDelegate;
    protected final AnimationHost mAnimationHost;
    protected final ScrollDelegate mScrollDelegate;

    // Dependencies
    protected final TabModel mModel;
    protected final TabGroupModelFilter mTabGroupModelFilter;
    protected final View mContainerView;
    protected final ObservableSupplierImpl<@Nullable Token> mGroupIdToHideSupplier;
    protected final Supplier<Float> mTabWidthSupplier;
    private final Supplier<Long> mLastReorderScrollTimeSupplier;

    ReorderStrategyBase(
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
        // TODO(crbug.com/409392603): Investigate splitting this class even further.
        mModel = model;
        mTabGroupModelFilter = tabGroupModelFilter;
        mContainerView = containerView;
        mAnimationHost = animationHost;
        mScrollDelegate = scrollDelegate;
        mStripUpdateDelegate = stripUpdateDelegate;
        mReorderDelegate = reorderDelegate;
        mGroupIdToHideSupplier = groupIdToHideSupplier;
        mTabWidthSupplier = tabWidthSupplier;
        mLastReorderScrollTimeSupplier = lastReorderScrollTimeSupplier;
    }

    // ============================================================================================
    // Reorder API
    // ============================================================================================

    @Override
    public void reorderViewInDirection(
            StripLayoutTabDelegate tabDelegate,
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            StripLayoutView reorderingView,
            boolean toRight) {
        // Default implementation is intentionally no-op.
    }

    // ============================================================================================
    // Group helpers
    // ============================================================================================

    /**
     * Attempts to move the interacting tabs out of their group. May prompt the user with a
     * confirmation dialog if the tab removal will result in a group deletion. Animates accordingly.
     *
     * @param stripViews The list of {@link StripLayoutView}.
     * @param groupTitles The list of {@link StripLayoutGroupTitle}.
     * @param interactingTabs The tabs to move out of group.
     * @param groupTitleToAnimate The title of the group the interacting tab is attempting to move
     *     out of.Used for animation. Null if animation is not needed.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     * @param actionType The action type {@link ActionType} to determine which user prompt to show.
     */
    protected void moveInteractingTabsOutOfGroup(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            List<StripLayoutTab> interactingTabs,
            @Nullable StripLayoutGroupTitle groupTitleToAnimate,
            boolean towardEnd,
            @ActionType int actionType) {
        // Exit reorder mode if the dialog will show. Tab drag and drop is cancelled elsewhere.
        Runnable beforeSyncDialogRunnable =
                () -> mReorderDelegate.stopReorderMode(stripViews, groupTitles);
        String userAction =
                interactingTabs.size() > 1
                        ? "MobileToolbarReorderTab.TabsRemovedFromGroup"
                        : "MobileToolbarReorderTab.TabRemovedFromGroup";
        Runnable onSuccess = () -> RecordUserAction.record(userAction);

        List<Integer> tabIds = new ArrayList<>();
        for (StripLayoutTab stripTab : interactingTabs) {
            tabIds.add(stripTab.getTabId());
        }

        List<Tab> tabs = TabModelUtils.getTabsById(tabIds, mModel, /* allowClosing= */ false);
        // When dragging the last tab out of group, the tab group delete dialog will show and we
        // will hide the indicators for the interacting tab group until the user confirms the next
        // action. e.g delete tab group when user confirms the delete, or restore indicators back on
        // strip when user cancel the delete.
        Token groupId = assertNonNull(tabs.get(0).getTabGroupId());
        StripTabModelActionListener listener =
                new StripTabModelActionListener(
                        groupId,
                        actionType,
                        mGroupIdToHideSupplier,
                        mContainerView,
                        beforeSyncDialogRunnable,
                        onSuccess);
        mTabGroupModelFilter
                .getTabUngrouper()
                .ungroupTabs(tabs, towardEnd, /* allowDialog= */ true, listener);

        // Run indicator animations. Find the group title after handling the removal, since the
        // group may have been deleted.
        if (groupTitleToAnimate != null
                && StripLayoutUtils.arrayContains(groupTitles, groupTitleToAnimate)) {
            animateGroupIndicatorForTabReorder(
                    groupTitleToAnimate, /* isMovingOutOfGroup= */ true, towardEnd);
        }
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
    protected void updateBottomIndicatorWidthForTabReorder(
            CompositorAnimationHandler animationHandler,
            TabGroupModelFilter modelFilter,
            StripLayoutGroupTitle groupTitle,
            boolean isMovingOutOfGroup,
            boolean throughGroupTitle,
            List<Animator> animators) {
        float effectiveTabWidth =
                StripLayoutUtils.getEffectiveTabWidth(mTabWidthSupplier, /* isPinned= */ false);
        float endWidth =
                StripLayoutUtils.calculateBottomIndicatorWidth(
                        groupTitle,
                        StripLayoutUtils.getNumOfTabsInGroup(modelFilter, groupTitle),
                        effectiveTabWidth);
        float startWidth = endWidth + MathUtils.flipSignIf(effectiveTabWidth, !isMovingOutOfGroup);

        animators.add(
                CompositorAnimator.ofFloatProperty(
                        animationHandler,
                        groupTitle,
                        StripLayoutGroupTitle.BOTTOM_INDICATOR_WIDTH,
                        startWidth,
                        endWidth,
                        throughGroupTitle ? ANIM_TAB_MOVE_MS : ANIM_TAB_SLIDE_OUT_MS));
    }

    // ============================================================================================
    // Margin helpers
    // ============================================================================================

    protected void setTrailingMarginForView(
            StripLayoutView stripView,
            StripLayoutGroupTitle[] groupTitles,
            boolean shouldHaveTrailingMargin,
            List<Animator> animationList) {
        final StripLayoutGroupTitle groupTitle;
        if (stripView instanceof StripLayoutTab stripTab) {
            final Tab tab = mModel.getTabById(stripTab.getTabId());
            if (tab == null) return;
            groupTitle = StripLayoutUtils.findGroupTitle(groupTitles, tab.getTabGroupId());
        } else {
            groupTitle = (StripLayoutGroupTitle) stripView;
        }

        setTrailingMarginForView(stripView, groupTitle, shouldHaveTrailingMargin, animationList);
    }

    /**
     * Sets the trailing margin for the current stripView. Update bottom indicator width for Tab
     * Group Indicators and animates if necessary.
     *
     * @param stripView The stripView to update.
     * @param groupTitle The group title associated with the stripView. Null if stripView is not
     *     grouped.
     * @param shouldHaveTrailingMargin Whether the stripView should have a trailing margin or not.
     * @param animationList The list to add the animation to.
     */
    private void setTrailingMarginForView(
            StripLayoutView stripView,
            @Nullable StripLayoutGroupTitle groupTitle,
            boolean shouldHaveTrailingMargin,
            List<Animator> animationList) {
        // Avoid triggering updates if trailing margin isn't actually changing.
        float trailingMargin =
                shouldHaveTrailingMargin
                        ? StripLayoutUtils.getHalfTabWidth(
                                mTabWidthSupplier, TabStripDragHandler.isDraggingPinnedItem())
                        : 0f;
        if (stripView.getTrailingMargin() == trailingMargin) return;

        // Update group title bottom indicator width if needed.
        boolean isExpandedGroup = groupTitle != null && !groupTitle.isCollapsed();
        boolean shouldAnimateBottomIndicator =
                !shouldHaveTrailingMargin || TabStripDragHandler.isDraggingUnpinnedTab();
        if (isExpandedGroup && shouldAnimateBottomIndicator) {
            assumeNonNull(groupTitle);
            float startWidth = groupTitle.getBottomIndicatorWidth();
            float endWidth =
                    StripLayoutUtils.calculateBottomIndicatorWidth(
                                    groupTitle,
                                    StripLayoutUtils.getNumOfTabsInGroup(
                                            mTabGroupModelFilter, groupTitle),
                                    StripLayoutUtils.getEffectiveTabWidth(
                                            mTabWidthSupplier, /* isPinned= */ false))
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
                        stripView,
                        StripLayoutView.TRAILING_MARGIN,
                        stripView.getTrailingMargin(),
                        trailingMargin,
                        ANIM_TAB_SLIDE_OUT_MS));
    }

    private void resetTabGroupMargins(
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutView[] stripViews,
            List<Animator> animationList) {
        // TODO(crbug.com/372546700): Investigate only resetting first and last margin, as we now
        //  don't use trailing margins to demarcate tab group bounds.
        for (int i = 0; i < stripViews.length; i++) {
            setTrailingMarginForView(
                    stripViews[i],
                    groupTitles,
                    /* shouldHaveTrailingMargin= */ false,
                    animationList);
        }
        mScrollDelegate.setReorderStartMargin(/* newStartMargin= */ 0.f);
    }

    /**
     * Calculates the start and end margins needed to allow for reordering tabs into/out of groups
     * near the edge of the tab strip. 0 if the first/last tabs aren't grouped, respectively.
     *
     * @param stripTabs The list of {@link StripLayoutTab}.
     */
    protected void setEdgeMarginsForReorder(StripLayoutTab[] stripTabs) {
        float marginWidth =
                StripLayoutUtils.getHalfTabWidth(mTabWidthSupplier, /* isPinned= */ false)
                        * StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;

        // 1. Set the start margin. Update the scroll offset to prevent any apparent movement.
        StripLayoutTab firstTab = stripTabs[0];
        boolean firstTabIsInGroup =
                mTabGroupModelFilter.isTabInTabGroup(mModel.getTabByIdChecked(firstTab.getTabId()));
        if (firstTabIsInGroup) mScrollDelegate.setReorderStartMargin(marginWidth);

        // 2. Set the trailing margin.
        StripLayoutTab lastTab = stripTabs[stripTabs.length - 1];
        boolean lastTabIsInGroup =
                mTabGroupModelFilter.isTabInTabGroup(mModel.getTabByIdChecked(lastTab.getTabId()));
        lastTab.setTrailingMargin((lastTabIsInGroup && !lastTab.isCollapsed()) ? marginWidth : 0.f);

        // 3. Clear the "previous last" tab's trailing margin after reorder. For MultiTabs reorder,
        // the "previous last" could be any tab after bulk moves, so loop backward and clear the
        // first non-zero trailing margin.
        for (int i = stripTabs.length - 2; i >= 0; i--) {
            StripLayoutTab stripTab = stripTabs[i];
            if (stripTab.getTrailingMargin() != 0) {
                stripTab.setTrailingMargin(0f);
                break;
            }
        }
    }

    // ============================================================================================
    // Stop reorder helpers
    // ============================================================================================

    protected void handleStopReorderMode(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            List<StripLayoutView> interactingViews,
            @Nullable StripLayoutTab tabToReattach,
            List<Animator> animationList,
            Runnable onAnimationEnd) {
        // Animate any offsets back to 0.
        mAnimationHost.finishAnimationsAndPushTabUpdates();
        for (StripLayoutView view : interactingViews) {
            if (view == null) continue;
            animationList.add(
                    CompositorAnimator.ofFloatProperty(
                            mAnimationHost.getAnimationHandler(),
                            view,
                            StripLayoutView.X_OFFSET,
                            view.getOffsetX(),
                            0f,
                            ANIM_TAB_MOVE_MS));
        }

        // Reattach the selected tab container if needed.
        if (tabToReattach != null && !tabToReattach.getFolioAttached()) {
            updateTabAttachState(tabToReattach, /* attached= */ true, animationList);
        }

        resetTabGroupMargins(groupTitles, stripViews, animationList);

        // Start animations and run cleanup logic on completion.
        mAnimationHost.startAnimations(
                animationList,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        if (onAnimationEnd != null) {
                            onAnimationEnd.run();
                        }
                    }
                });
    }

    protected void updateTabAttachState(
            StripLayoutTab tab, boolean attached, List<Animator> animationList) {
        float startValue =
                attached
                        ? StripLayoutTabDelegate.FOLIO_DETACHED_BOTTOM_MARGIN_DP
                        : StripLayoutTabDelegate.FOLIO_ATTACHED_BOTTOM_MARGIN_DP;
        float intermediateValue = FOLIO_ANIM_INTERMEDIATE_MARGIN_DP;
        float endValue =
                attached
                        ? StripLayoutTabDelegate.FOLIO_ATTACHED_BOTTOM_MARGIN_DP
                        : StripLayoutTabDelegate.FOLIO_DETACHED_BOTTOM_MARGIN_DP;

        ArrayList<Animator> attachAnimationList = new ArrayList<>();
        CompositorAnimator dropAnimation =
                CompositorAnimator.ofFloatProperty(
                        mAnimationHost.getAnimationHandler(),
                        tab,
                        StripLayoutTab.BOTTOM_MARGIN,
                        startValue,
                        intermediateValue,
                        ANIM_FOLIO_DETACH_MS);
        CompositorAnimator riseAnimation =
                CompositorAnimator.ofFloatProperty(
                        mAnimationHost.getAnimationHandler(),
                        tab,
                        StripLayoutTab.BOTTOM_MARGIN,
                        intermediateValue,
                        endValue,
                        ANIM_FOLIO_DETACH_MS);
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

    // ============================================================================================
    // Threshold helpers
    // ============================================================================================

    /**
     * Returns the threshold to swap the interacting views with an adjacent tab.
     *
     * @param isPinned Whether the tab is pinned.
     */
    protected float getTabSwapThreshold(boolean isPinned) {
        return StripLayoutUtils.getEffectiveTabWidth(mTabWidthSupplier, isPinned)
                * StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;
    }

    /** Returns the threshold to drag into a group. */
    protected float getDragInThreshold() {
        return StripLayoutUtils.getHalfTabWidth(mTabWidthSupplier, /* isPinned= */ false)
                * StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;
    }

    /**
     * Returns the threshold to drag out of a group.
     *
     * @param groupTitle The group title for the desired group. Must not be null.
     * @param towardEnd True if dragging towards the end of the strip.
     */
    protected float getDragOutThreshold(StripLayoutGroupTitle groupTitle, boolean towardEnd) {
        float dragOutThreshold =
                StripLayoutUtils.getHalfTabWidth(mTabWidthSupplier, /* isPinned= */ false)
                        * StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;
        return dragOutThreshold + (towardEnd ? 0 : groupTitle.getWidth());
    }

    /**
     * Returns the drag distance required to swap positions with the adjacent group.
     *
     * @param adjTitle The adjacent group title.
     */
    protected float getGroupSwapThreshold(StripLayoutGroupTitle adjTitle) {
        if (adjTitle.isCollapsed()) {
            return adjTitle.getWidth() * StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;
        }
        return (adjTitle.getBottomIndicatorWidth() + TAB_GROUP_BOTTOM_INDICATOR_WIDTH_OFFSET)
                * StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;
    }

    // ============================================================================================
    // Offset helpers
    // ============================================================================================

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
    protected float adjustOffsetAfterReorder(
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
        if (mLastReorderScrollTimeSupplier.get() != 0) offset -= deltaX;
        // When scrolled near the end of the strip, the scrollOffset being clamped can affect the
        // apparent position.
        oldScrollOffset += oldStartMargin - mScrollDelegate.getReorderStartMargin();
        float scrollOffsetDelta = mScrollDelegate.getScrollOffset() - oldScrollOffset;
        offset -= MathUtils.flipSignIf(scrollOffsetDelta, LocalizationUtils.isLayoutRtl());
        return offset;
    }

    /**
     * Returns {@code true} if we're dragging towards the end of the strip. {@code false} otherwise.
     *
     * @param offset The offset of the current drag.
     */
    protected boolean isOffsetTowardEnd(float offset) {
        return (offset >= 0) ^ LocalizationUtils.isLayoutRtl();
    }

    /**
     * Returns the maximum allowed horizontal drag offset for the interactingView, effectively
     * clamping its movement so it doesn't move past the start or end view on the tab strip.
     *
     * @param interactingView The view currently being dragged.
     * @param boundaryView The view defining the boundary of the drag (e.g. first view or last view
     *     on the tab strip).
     * @param toRight {@code true} if the drag direction is toward the right of the strip.
     */
    protected float getDragOffsetLimit(
            StripLayoutView interactingView, StripLayoutView boundaryView, boolean toRight) {
        float boundaryX = boundaryView.getIdealX();
        if (toRight) boundaryX += (boundaryView.getWidth() - interactingView.getWidth());
        return boundaryX - interactingView.getIdealX();
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
    protected void animateGroupIndicatorForTabReorder(
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

        mAnimationHost.queueAnimations(animators, /* listener= */ null);
    }

    /**
     * Returns The sliding {@link CompositorAnimator}.
     *
     * @param view The {@link StripLayoutView} to create a sliding {@link CompositorAnimator} for.
     */
    protected Animator getViewSlidingAnimator(StripLayoutView view) {
        return CompositorAnimator.ofFloatProperty(
                mAnimationHost.getAnimationHandler(),
                view,
                StripLayoutView.X_OFFSET,
                /* startValue= */ view.getDrawX() - view.getIdealX(),
                /* endValue= */ 0,
                ANIM_TAB_MOVE_MS);
    }

    /** See {@link #animateViewSliding(StripLayoutView, AnimatorListener)}. */
    protected void animateViewSliding(StripLayoutView view) {
        animateViewSliding(view, null);
    }

    /**
     * Creates & starts a sliding {@link CompositorAnimator} for the given {@link StripLayoutView}.
     *
     * @param view The {@link StripLayoutView} to create a sliding {@link CompositorAnimator} for.
     * @param listener The {@link AnimatorListener} to add, or {@code null} if none.
     */
    protected void animateViewSliding(StripLayoutView view, @Nullable AnimatorListener listener) {
        List<Animator> animators = new ArrayList<>();
        animators.add(getViewSlidingAnimator(view));
        mAnimationHost.queueAnimations(animators, listener);
    }

    /**
     * Returns {@code true} if the drag is toward the end of the strip; {@code false} otherwise.
     *
     * @param isPinned Whether the tab is pinned; currently always false for grouped tabs.
     */
    protected float getEffectiveTabWidth(boolean isPinned) {
        return StripLayoutUtils.getEffectiveTabWidth(mTabWidthSupplier, isPinned);
    }
}

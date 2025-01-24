// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.ANIM_TAB_MOVE_MS;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.ANIM_TAB_SLIDE_OUT_MS;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

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
import org.chromium.chrome.browser.compositor.overlays.strip.StripTabModelActionListener;
import org.chromium.chrome.browser.compositor.overlays.strip.StripTabModelActionListener.ActionType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.StripUpdateDelegate;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.interpolators.Interpolators;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Base class for {@link ReorderStrategy} implementations. */
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
    protected final ObservableSupplierImpl<Integer> mGroupIdToHideSupplier;
    protected final Supplier<Float> mTabWidthSupplier;

    ReorderStrategyBase(
            ReorderDelegate reorderDelegate,
            StripUpdateDelegate stripUpdateDelegate,
            AnimationHost animationHost,
            ScrollDelegate scrollDelegate,
            TabModel model,
            TabGroupModelFilter tabGroupModelFilter,
            View containerView,
            ObservableSupplierImpl<Integer> groupIdToHideSupplier,
            Supplier<Float> tabWidthSupplier) {
        mModel = model;
        mTabGroupModelFilter = tabGroupModelFilter;
        mContainerView = containerView;
        mAnimationHost = animationHost;
        mScrollDelegate = scrollDelegate;
        mStripUpdateDelegate = stripUpdateDelegate;
        mReorderDelegate = reorderDelegate;
        mGroupIdToHideSupplier = groupIdToHideSupplier;
        mTabWidthSupplier = tabWidthSupplier;
    }

    /**
     * Attempts to move the interacting tab out of its group. May prompt the user with a
     * confirmation dialog if the tab removal will result in a group deletion. Animates accordingly.
     *
     * @param groupTitles The list of {@link StripLayoutGroupTitle}.
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param interactingTab The tab to move out of group.
     * @param groupTitleToAnimate The title of the group the interacting tab is attempting to move
     *     out of.Used for animation. Null if animation is not needed.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     * @param actionType The action type {@link ActionType} to determine which user prompt to show.
     */
    protected void moveInteractingTabOutOfGroup(
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            StripLayoutTab interactingTab,
            StripLayoutGroupTitle groupTitleToAnimate,
            boolean towardEnd,
            @ActionType int actionType) {
        final int tabId = interactingTab.getTabId();
        // Exit reorder mode if the dialog will show. Tab drag and drop is cancelled elsewhere.
        Runnable beforeSyncDialogRunnable =
                () -> mReorderDelegate.stopReorderMode(groupTitles, stripTabs);
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
                        actionType,
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
            @NonNull List<Animator> animators) {
        float effectiveTabWidth = StripLayoutUtils.getEffectiveTabWidth(mTabWidthSupplier);
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

    protected void setTrailingMarginForTab(
            StripLayoutTab stripTab,
            StripLayoutGroupTitle[] groupTitles,
            boolean shouldHaveTrailingMargin,
            @NonNull List<Animator> animationList) {
        final Tab tab = mModel.getTabById(stripTab.getTabId());
        if (tab == null) return;
        final StripLayoutGroupTitle groupTitle =
                StripLayoutUtils.findGroupTitle(groupTitles, tab.getRootId());

        setTrailingMarginForTab(stripTab, groupTitle, shouldHaveTrailingMargin, animationList);
    }

    /**
     * Sets the trailing margin for the current tab. Update bottom indicator width for Tab Group
     * Indicators and animates if necessary.
     *
     * @param tab The tab to update.
     * @param groupTitle The group title associated with the tab. Null if tab is not grouped.
     * @param shouldHaveTrailingMargin Whether the tab should have a trailing margin or not.
     * @param animationList The list to add the animation to, or {@code null} if not animating.
     */
    private void setTrailingMarginForTab(
            StripLayoutTab tab,
            StripLayoutGroupTitle groupTitle,
            boolean shouldHaveTrailingMargin,
            @NonNull List<Animator> animationList) {
        // Avoid triggering updates if trailing margin isn't actually changing.
        float trailingMargin =
                shouldHaveTrailingMargin
                        ? StripLayoutUtils.getHalfTabWidth(mTabWidthSupplier)
                        : 0.f;
        if (tab.getTrailingMargin() == trailingMargin) return;

        // Update group title bottom indicator width if needed.
        if (groupTitle != null) {
            float startWidth = groupTitle.getBottomIndicatorWidth();
            float endWidth =
                    StripLayoutUtils.calculateBottomIndicatorWidth(
                                    groupTitle,
                                    StripLayoutUtils.getNumOfTabsInGroup(
                                            mTabGroupModelFilter, groupTitle),
                                    StripLayoutUtils.getEffectiveTabWidth(mTabWidthSupplier))
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
    }

    protected void handleStopReorderMode(
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            StripLayoutView interactingView,
            List<Animator> animationList) {

        // 2. Animate offsets back to 0, reattach the container, and clear the margins.
        mAnimationHost.finishAnimationsAndPushTabUpdates();
        // interactingView may be null if reordering for external view drag drop.
        if (interactingView != null) {
            animationList.add(
                    CompositorAnimator.ofFloatProperty(
                            mAnimationHost.getAnimationHandler(),
                            interactingView,
                            StripLayoutView.X_OFFSET,
                            interactingView.getOffsetX(),
                            /* endValue= */ 0f,
                            ANIM_TAB_MOVE_MS));

            if (interactingView instanceof StripLayoutTab tab && !tab.getFolioAttached()) {
                updateTabAttachState(tab, /* attached= */ true, animationList);
            }
        }
        resetTabGroupMargins(groupTitles, stripTabs, animationList);
    }

    @VisibleForTesting
    void updateTabAttachState(
            StripLayoutTab tab, boolean attached, @NonNull List<Animator> animationList) {
        float startValue =
                attached
                        ? StripLayoutUtils.FOLIO_DETACHED_BOTTOM_MARGIN_DP
                        : StripLayoutUtils.FOLIO_ATTACHED_BOTTOM_MARGIN_DP;
        float intermediateValue = FOLIO_ANIM_INTERMEDIATE_MARGIN_DP;
        float endValue =
                attached
                        ? StripLayoutUtils.FOLIO_ATTACHED_BOTTOM_MARGIN_DP
                        : StripLayoutUtils.FOLIO_DETACHED_BOTTOM_MARGIN_DP;

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

    private void resetTabGroupMargins(
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            @NonNull List<Animator> animationList) {
        // TODO(crbug.com/372546700): Investigate only resetting first and last margin, as we now
        //  don't use trailing margins to demarcate tab group bounds.
        for (int i = 0; i < stripTabs.length; i++) {
            final StripLayoutTab stripTab = stripTabs[i];
            setTrailingMarginForTab(
                    stripTab, groupTitles, /* shouldHaveTrailingMargin= */ false, animationList);
        }
        mScrollDelegate.setReorderStartMargin(/* newStartMargin= */ 0.f);
    }

    /**
     * Calculates the start and end margins needed to allow for reordering tabs into/out of groups
     * near the edge of the tab strip. 0 if the first/last tabs aren't grouped, respectively.
     *
     * @param stripTabs The list of {@link StripLayoutTab}.
     */
    void setEdgeMarginsForReorder(StripLayoutTab[] stripTabs) {
        float marginWidth =
                StripLayoutUtils.getHalfTabWidth(mTabWidthSupplier)
                        * StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;

        // 1. Set the start margin. Update the scroll offset to prevent any apparent movement.
        StripLayoutTab firstTab = stripTabs[0];
        boolean firstTabIsInGroup =
                mTabGroupModelFilter.isTabInTabGroup(mModel.getTabById(firstTab.getTabId()));
        if (firstTabIsInGroup) mScrollDelegate.setReorderStartMargin(marginWidth);

        // 2. Set the trailing margin.
        StripLayoutTab lastTab = stripTabs[stripTabs.length - 1];
        boolean lastTabIsInGroup =
                mTabGroupModelFilter.isTabInTabGroup(mModel.getTabById(lastTab.getTabId()));
        lastTab.setTrailingMargin((lastTabIsInGroup && !lastTab.isCollapsed()) ? marginWidth : 0.f);

        // 3. Ensure the second-to-last tab doesn't have a trailing margin after reorder.
        if (stripTabs.length > 1) stripTabs[stripTabs.length - 2].setTrailingMargin(0f);
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

        mAnimationHost.startAnimations(animators, /* listener= */ null);
    }

    /**
     * @param view The {@link StripLayoutView} to create a sliding {@link CompositorAnimator} for.
     * @return The sliding {@link CompositorAnimator}.
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

    /**
     * Creates & starts a sliding {@link CompositorAnimator} for the given {@link StripLayoutView}.
     *
     * @param view The {@link StripLayoutView} to create a sliding {@link CompositorAnimator} for.
     */
    protected void animateViewSliding(StripLayoutView view) {
        List<Animator> animators = new ArrayList<>();
        animators.add(getViewSlidingAnimator(view));
        mAnimationHost.startAnimations(animators, /* listener= */ null);
    }
}

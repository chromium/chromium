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
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.ui.interpolators.Interpolators;

import java.util.ArrayList;
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

    // Test State.
    private boolean mAnimationsDisabledForTesting;

    // Tab State.
    private TabGroupModelFilter mTabGroupModelFilter;
    private TabModel mModel;

    // Tab Strip State.
    private AnimationHost mAnimationHost;
    private ScrollDelegate mScrollDelegate;
    private View mContainerView;

    // Internal State.
    private boolean mInitialized;

    // Reorder State.
    private boolean mInReorderMode;
    private boolean mReorderingForTabDrop;

    /** The last x-position we processed for reorder. */
    private float mLastReorderX;

    /** The effective tab width (accounting for overlap) at the time that we started reordering. */
    private float mEffectiveTabWidth;

    private StripLayoutTab mInteractingTab;

    // Auto-scroll State.
    private long mLastReorderScrollTime;
    private int mReorderScrollState = REORDER_SCROLL_NONE;

    // ============================================================================================
    // Getters and setters
    // ============================================================================================

    boolean getInReorderMode() {
        return mInReorderMode;
    }

    void setInReorderMode(boolean inReorderMode) {
        mInReorderMode = inReorderMode;
    }

    boolean getReorderingForTabDrop() {
        return mReorderingForTabDrop;
    }

    void setReorderingForTabDrop(boolean reorderingForTabDrop) {
        mReorderingForTabDrop = reorderingForTabDrop;
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

    // ============================================================================================
    // Initialization
    // ============================================================================================

    /**
     * Passes the dependencies needed in this delegate. Passed here as they aren't ready on
     * instantiation.
     *
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} linked to this delegate.
     * @param scrollDelegate The {@link ScrollDelegate} linked to this delegate.
     */
    void initialize(
            AnimationHost animationHost,
            TabGroupModelFilter tabGroupModelFilter,
            ScrollDelegate scrollDelegate,
            View containerView) {
        mAnimationHost = animationHost;
        mTabGroupModelFilter = tabGroupModelFilter;
        mScrollDelegate = scrollDelegate;
        mContainerView = containerView;

        mModel = mTabGroupModelFilter.getTabModel();
        mInitialized = true;
    }

    // ============================================================================================
    // Reorder API
    // ============================================================================================

    /**
     * Begin reordering the interacting tab.
     *
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param interactingTab The interacting {@link StripLayoutTab}.
     * @param effectiveTabWidth The width of a tab, accounting for overlap.
     * @param x The x coordinate that the reorder action began at.
     */
    void startReorderTab(
            StripLayoutTab[] stripTabs,
            StripLayoutTab interactingTab,
            float effectiveTabWidth,
            float x) {
        RecordUserAction.record("MobileToolbarStartReorderTab");
        mInteractingTab = interactingTab;

        // 1. Set reorder mode to true before selecting this tab to prevent unnecessary triggering
        // of #bringSelectedTabToVisibleArea for edge tabs when the tab strip is full.
        mInReorderMode = true;

        // 2. Select this tab so that it is always in the foreground.
        TabModelUtils.setIndex(
                mModel, TabModelUtils.getTabIndexById(mModel, mInteractingTab.getTabId()));

        // 3. Set initial state.
        prepareStripForReorder(stripTabs, effectiveTabWidth, x);

        // 4. Lift the container off the toolbar and perform haptic feedback.
        ArrayList<Animator> animationList =
                mAnimationsDisabledForTesting ? null : new ArrayList<>();
        updateTabAttachState(mInteractingTab, /* attached= */ false, animationList);
        StripLayoutUtils.performHapticFeedback(mContainerView);

        // 5. Kick-off animations and request an update.
        if (animationList != null) {
            mAnimationHost.startAnimations(animationList, /* listener= */ null);
        }
        // TODO(crbug.com/372546700): Clean-up when mAnimationsDisabledForTesting is removed.
        mAnimationHost.requestUpdate();
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

    void clearReorderScrollState() {
        mReorderScrollState = REORDER_SCROLL_NONE;
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
        lastTab.setTrailingMargin((lastTabIsInGroup || mReorderingForTabDrop) ? marginWidth : 0.f);
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
            @Nullable List<Animator> animationList) {
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

            if (animationList != null) {
                animationList.add(
                        CompositorAnimator.ofFloatProperty(
                                mAnimationHost.getAnimationHandler(),
                                groupTitle,
                                StripLayoutGroupTitle.BOTTOM_INDICATOR_WIDTH,
                                startWidth,
                                endWidth,
                                ANIM_TAB_SLIDE_OUT_MS));
            } else {
                groupTitle.setBottomIndicatorWidth(endWidth);
            }
        }

        // Set new trailing margin.
        if (animationList != null) {
            animationList.add(
                    CompositorAnimator.ofFloatProperty(
                            mAnimationHost.getAnimationHandler(),
                            tab,
                            StripLayoutTab.TRAILING_MARGIN,
                            tab.getTrailingMargin(),
                            trailingMargin,
                            ANIM_TAB_SLIDE_OUT_MS));
        } else {
            tab.setTrailingMargin(trailingMargin);
        }

        return true;
    }

    // ============================================================================================
    // Tab reorder helpers
    // ============================================================================================

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
     * Wrapper for {@link TabGroupModelFilter#moveTabOutOfGroupInDirection} that also records the
     * tab-strip specific User Action.
     */
    void moveTabOutOfGroupInDirection(int tabId, boolean towardEnd) {
        mTabGroupModelFilter.moveTabOutOfGroupInDirection(tabId, towardEnd);
        RecordUserAction.record("MobileToolbarReorderTab.TabRemovedFromGroup");
    }

    /**
     * @param groupTitle The group title for the desired group. Must not be null.
     * @param towardEnd True if dragging towards the end of the strip.
     * @return The threshold to drag out of a group.
     */
    float getDragOutThreshold(StripLayoutGroupTitle groupTitle, boolean towardEnd) {
        float dragOutThreshold = getHalfTabWidth() * REORDER_OVERLAP_SWITCH_PERCENTAGE;
        return dragOutThreshold + (towardEnd ? 0 : groupTitle.getWidth());
    }

    /**
     * @return The threshold to drag into a group.
     */
    float getDragInThreshold() {
        return getHalfTabWidth() * REORDER_OVERLAP_SWITCH_PERCENTAGE;
    }

    /**
     * @return Half of mEffectiveTabWidth.
     */
    private float getHalfTabWidth() {
        return mEffectiveTabWidth / 2;
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
    void animateGroupIndicatorForTabReorder(
            StripLayoutGroupTitle groupTitle, boolean isMovingOutOfGroup, boolean towardEnd) {
        // TODO(crbug.com/372546700): Disable animations in tests using CompositorAnimationHandler.
        if (mAnimationsDisabledForTesting) return;

        List<Animator> animators = new ArrayList<>();

        // Add the group title swapping animation if the tab is passing through group title.
        boolean throughGroupTitle = isMovingOutOfGroup ^ towardEnd;
        if (throughGroupTitle) {
            // If not animating, no action needed. The group title will have its new position
            // correctly calculated on the next layout pass.
            animators.add(
                    CompositorAnimator.ofFloatProperty(
                            mAnimationHost.getAnimationHandler(),
                            groupTitle,
                            StripLayoutView.X_OFFSET,
                            /* startValue= */ groupTitle.getDrawX() - groupTitle.getIdealX(),
                            /* endValue= */ 0,
                            ANIM_TAB_MOVE_MS));
        }

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
            List<Animator> animators) {
        float endWidth =
                StripLayoutUtils.calculateBottomIndicatorWidth(
                        groupTitle,
                        StripLayoutUtils.getNumOfTabsInGroup(modelFilter, groupTitle),
                        mEffectiveTabWidth);
        float startWidth = endWidth + MathUtils.flipSignIf(mEffectiveTabWidth, !isMovingOutOfGroup);

        if (animators != null) {
            animators.add(
                    CompositorAnimator.ofFloatProperty(
                            animationHandler,
                            groupTitle,
                            StripLayoutGroupTitle.BOTTOM_INDICATOR_WIDTH,
                            startWidth,
                            endWidth,
                            throughGroupTitle ? ANIM_TAB_MOVE_MS : ANIM_TAB_SLIDE_OUT_MS));
        } else {
            groupTitle.setBottomIndicatorWidth(endWidth);
        }
    }

    void updateTabAttachState(
            StripLayoutTab tab, boolean attached, @Nullable ArrayList<Animator> animationList) {
        float startValue =
                attached ? FOLIO_DETACHED_BOTTOM_MARGIN_DP : FOLIO_ATTACHED_BOTTOM_MARGIN_DP;
        float intermediateValue = FOLIO_ANIM_INTERMEDIATE_MARGIN_DP;
        float endValue =
                attached ? FOLIO_ATTACHED_BOTTOM_MARGIN_DP : FOLIO_DETACHED_BOTTOM_MARGIN_DP;

        if (animationList == null) {
            tab.setBottomMargin(endValue);
            tab.setFolioAttached(attached);
            return;
        }

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

    // ============================================================================================
    // Test support
    // ============================================================================================

    /** Disables animations for testing purposes. */
    void disableAnimationsForTesting() {
        mAnimationsDisabledForTesting = true;
    }
}

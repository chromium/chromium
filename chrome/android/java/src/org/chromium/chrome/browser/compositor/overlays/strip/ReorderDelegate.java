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

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.tab.Tab;
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
    private ActionConfirmationDelegate mActionConfirmationDelegate;
    private ObservableSupplierImpl<Integer> mGroupIdToHideSupplier;
    private View mContainerView;

    // Internal State.
    private boolean mInitialized;

    // Reorder State.
    private final ObservableSupplierImpl<Boolean> mInReorderModeSupplier =
            new ObservableSupplierImpl<>(/* initialValue= */ false);
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
        return Boolean.TRUE.equals(mInReorderModeSupplier.get());
    }

    void setInReorderMode(boolean inReorderMode) {
        mInReorderModeSupplier.set(inReorderMode);
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
     * @param animationHost The {@link AnimationHost} for triggering animations.
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} for accessing tab state.
     * @param scrollDelegate The {@link ScrollDelegate} for updating scroll offset.
     * @param actionConfirmationDelegate The {@link ActionConfirmationDelegate} for confirming group
     *     actions, such as delete and ungroup.
     * @param groupIdToHideSupplier The {@link ObservableSupplierImpl} for the group ID to hide.
     * @param containerView The tab strip container {@link View}.
     */
    void initialize(
            AnimationHost animationHost,
            TabGroupModelFilter tabGroupModelFilter,
            ScrollDelegate scrollDelegate,
            ActionConfirmationDelegate actionConfirmationDelegate,
            ObservableSupplierImpl<Integer> groupIdToHideSupplier,
            View containerView) {
        mAnimationHost = animationHost;
        mTabGroupModelFilter = tabGroupModelFilter;
        mScrollDelegate = scrollDelegate;
        mActionConfirmationDelegate = actionConfirmationDelegate;
        mGroupIdToHideSupplier = groupIdToHideSupplier;
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
        setInteractingTab(interactingTab);

        // 1. Set reorder mode to true before selecting this tab to prevent unnecessary triggering
        // of #bringSelectedTabToVisibleArea for edge tabs when the tab strip is full.
        setInReorderMode(true);

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

    /**
     * Stop reorder mode and clear any relevant state. Don't call if not in reorder mode.
     *
     * @param groupTitles The list of {@link StripLayoutGroupTitle}.
     * @param stripTabs The list of {@link StripLayoutTab}.
     */
    void stopReorderMode(StripLayoutGroupTitle[] groupTitles, StripLayoutTab[] stripTabs) {
        assert getInReorderMode()
                : "Tried to stop reorder mode, without first starting reorder mode.";
        ArrayList<Animator> animationList = null;
        // TODO(crbug.com/372546700): Clean-up when mAnimationsDisabledForTesting is removed.
        if (!mAnimationsDisabledForTesting) animationList = new ArrayList<>();

        // 1. Reset the state variables.
        mReorderScrollState = REORDER_SCROLL_NONE;
        setInReorderMode(false);

        // 2. Reset the interacting view (clear any offset and reattach the container).
        mAnimationHost.finishAnimationsAndPushTabUpdates();
        if (mInteractingTab != null) {
            // TODO(crbug.com/372546700): mInteractingTab may be null if reordering for tab drop.
            if (animationList != null) {
                animationList.add(
                        CompositorAnimator.ofFloatProperty(
                                mAnimationHost.getAnimationHandler(),
                                mInteractingTab,
                                StripLayoutView.X_OFFSET,
                                mInteractingTab.getOffsetX(),
                                0f,
                                ANIM_TAB_MOVE_MS));
            } else {
                mInteractingTab.setOffsetX(0f);
            }

            // Skip reattachment for tab drop to avoid exposing bottom indicator underneath the tab
            // container.
            if (!mReorderingForTabDrop || !mInteractingTab.getFolioAttached()) {
                updateTabAttachState(mInteractingTab, true, animationList);
            }
        }

        // 3. Clear any tab group margins.
        resetTabGroupMargins(groupTitles, stripTabs, animationList);

        // 4. Clear the interacting view.
        setInteractingTab(null);

        // 5. Reset the tab drop state. Must occur after the rest of the state is reset, since some
        // logic depends on these values.
        mReorderingForTabDrop = false;

        // 6. Start animations.
        mAnimationHost.startAnimations(animationList, /* listener= */ null);
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

    private void resetTabGroupMargins(
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            @Nullable ArrayList<Animator> animationList) {
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
     * Attempts to move the interacting tab out of its group. May prompt the user with a
     * confirmation dialog if the tab removal will result in a group deletion. Animates accordingly.
     *
     * @param groupTitles The list of {@link StripLayoutGroupTitle}.
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param towardEnd True if the interacting tab is being dragged toward the end of the strip.
     */
    void moveInteractingTabOutOfGroup(
            StripLayoutGroupTitle[] groupTitles, StripLayoutTab[] stripTabs, boolean towardEnd) {
        final int tabId = mInteractingTab.getTabId();
        // TODO(crbug.com/377750438): Skip creating the ActionConfirmationDelegate for Incognito as
        //  it won't be used here.
        if (StripLayoutUtils.isLastTabInGroup(mTabGroupModelFilter, tabId)
                && mGroupIdToHideSupplier.get() == Tab.INVALID_TAB_ID
                && !mTabGroupModelFilter.isIncognitoBranded()) {
            // When dragging the last tab out of group, the tab group delete dialog will show and we
            // will hide the indicators for the interacting tab group until the user confirms the
            // next action. e.g delete tab group when user confirms the delete, or restore
            // indicators back on strip when user cancel the delete.
            mActionConfirmationDelegate.handleDeleteGroupAction(
                    StripLayoutUtils.getRootId(mModel, mInteractingTab),
                    /* draggingLastTabOffStrip= */ false,
                    /* tabClosing= */ false,
                    () -> moveTabOutOfGroupInDirection(tabId, towardEnd));
            // Exit reorder mode if the dialog will show. Tab drag and drop is cancelled elsewhere.
            if (!mActionConfirmationDelegate.isTabRemoveDialogSkipped()) {
                stopReorderMode(groupTitles, stripTabs);
            }
        } else {
            moveTabOutOfGroupInDirection(tabId, towardEnd);
        }

        // Run indicator animations. Find the group title after handling the removal, since the
        // group may have been deleted OR the rootID may have changed.
        StripLayoutGroupTitle groupTitle =
                StripLayoutUtils.findGroupTitle(
                        groupTitles, StripLayoutUtils.getRootId(mModel, mInteractingTab));
        if (groupTitle != null) {
            animateGroupIndicatorForTabReorder(
                    groupTitle, /* isMovingOutOfGroup= */ true, towardEnd);
        }
    }

    /**
     * Wrapper for {@link TabGroupModelFilter#moveTabOutOfGroupInDirection} that also records the
     * tab-strip specific User Action.
     */
    private void moveTabOutOfGroupInDirection(int tabId, boolean towardEnd) {
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

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.graphics.PointF;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.overlays.strip.AnimationHost;
import org.chromium.chrome.browser.compositor.overlays.strip.ScrollDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.StripUpdateDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.base.LocalizationUtils;

import java.util.ArrayList;
import java.util.List;

/** Drag and drop reorder - drag external view onto / out-of strip and reorder within strip. */
public class ExternalViewDragDropReorderStrategy extends ReorderStrategyBase {
    // View on the strip being hovered on by the dragged view.
    private StripLayoutView mInteractingView;

    // View on the strip last hovered on by dragged view. This can be used post stop reorder to
    // handle drop event (eg: re-parenting dropped tab).
    private StripLayoutView mInteractingViewDuringStop;

    ExternalViewDragDropReorderStrategy(
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

    /** Initiate reorder when external view is dragged onto strip. */
    @Override
    public void startReorderMode(
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle[] stripGroupTitles,
            @NonNull StripLayoutView interactingView,
            PointF startPoint) {
        // 1. Set initial state and add edge margins.
        mInteractingView = interactingView;
        mInteractingViewDuringStop = null;
        mAnimationHost.finishAnimationsAndPushTabUpdates();
        setEdgeMarginsForReorder(stripTabs);

        // 2. Add a trailing margin to the interacting tab to indicate where the tab will be
        // inserted should the drag be dropped.
        ArrayList<Animator> animationList = new ArrayList<>();
        setTrailingMarginForTab(
                (StripLayoutTab) interactingView,
                stripGroupTitles,
                /* shouldHaveTrailingMargin= */ true,
                animationList);

        // 3. Kick-off animations and request an update.
        mAnimationHost.startAnimations(animationList, null);
    }

    @Override
    public void updateReorderPosition(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            float endX,
            float deltaX,
            @ReorderType int reorderType) {
        // 1. Adjust by a half tab-width so that we target the nearest tab gap.
        float adjustedXForDrop = StripLayoutUtils.adjustXForTabDrop(endX, mTabWidthSupplier);

        // 2. Clear previous "interacting" tab if inserting at the start of the strip.
        boolean inStartGap =
                LocalizationUtils.isLayoutRtl()
                        ? adjustedXForDrop > stripTabs[0].getTouchTargetRight()
                        : adjustedXForDrop < stripTabs[0].getTouchTargetLeft();

        if (inStartGap && mInteractingView != null) {
            mScrollDelegate.setReorderStartMargin(
                    /* newStartMargin= */ StripLayoutUtils.getHalfTabWidth(mTabWidthSupplier));

            mAnimationHost.finishAnimations();
            ArrayList<Animator> animationList = new ArrayList<>();
            setTrailingMarginForTab(
                    (StripLayoutTab) mInteractingView,
                    groupTitles,
                    /* shouldHaveTrailingMargin= */ false,
                    animationList);
            mInteractingView = null;
            mAnimationHost.startAnimations(animationList, null);

            // 2.a. Early-out if we just entered the start gap.
            return;
        }
        // 3. Otherwise, update drop indicator if necessary.
        StripLayoutTab hoveredTab =
                (StripLayoutTab)
                        StripLayoutUtils.findViewAtPositionX(
                                stripViews, adjustedXForDrop, /* includeGroupTitles= */ false);

        if (hoveredTab != null && hoveredTab != mInteractingView) {
            mAnimationHost.finishAnimations();

            // 3.a. Reset the state for the previous "interacting" tab.
            ArrayList<Animator> animationList = new ArrayList<>();
            if (mInteractingView != null) {
                setTrailingMarginForTab(
                        (StripLayoutTab) mInteractingView,
                        groupTitles,
                        /* shouldHaveTrailingMargin= */ false,
                        animationList);
            }

            // 3.b. Set state for the new "interacting" tab.
            setTrailingMarginForTab(
                    hoveredTab, groupTitles, /* shouldHaveTrailingMargin= */ true, animationList);
            mInteractingView = hoveredTab;

            // 3.c. Animate.
            mAnimationHost.startAnimations(animationList, null);
        }
    }

    @Override
    public void stopReorderMode(StripLayoutGroupTitle[] groupTitles, StripLayoutTab[] stripTabs) {
        List<Animator> animatorList = new ArrayList<>();
        handleStopReorderMode(groupTitles, stripTabs, mInteractingView, animatorList);
        mInteractingViewDuringStop = mInteractingView;
        // Start animations.
        mAnimationHost.startAnimations(
                animatorList,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mInteractingView = null;
                    }
                });
    }

    @Override
    public StripLayoutView getInteractingView() {
        return mInteractingView;
    }

    /** Merges dropped tab to interacting view's tab group, if one exists. */
    void handleDrop(StripLayoutGroupTitle[] groupTitles, int draggedTabId, int dropIndex) {
        if (mInteractingViewDuringStop == null) return;

        StripLayoutTab interactingView = (StripLayoutTab) mInteractingViewDuringStop;
        Tab interactingTab = mModel.getTabById(interactingView.getTabId());

        // 1. If hovered on tab is not part of group, no-op.
        if (!mTabGroupModelFilter.isTabInTabGroup(interactingTab)) {
            mInteractingViewDuringStop = null;
            return;
        }

        // 2. Merge dragged tab to hovered tab's group at drop index.
        mTabGroupModelFilter.mergeTabsToGroup(
                draggedTabId, interactingTab.getId(), /* skipUpdateTabModel= */ true);
        mModel.moveTab(draggedTabId, dropIndex);

        // 3. Animate bottom indicator. Done after merging the dragged tab to group,
        // so that the calculated bottom indicator width will be correct.
        StripLayoutGroupTitle groupTitle =
                StripLayoutUtils.findGroupTitle(groupTitles, interactingTab.getRootId());
        List<Animator> animators = new ArrayList<>();
        updateBottomIndicatorWidthForTabReorder(
                mAnimationHost.getAnimationHandler(),
                mTabGroupModelFilter,
                groupTitle,
                /* isMovingOutOfGroup= */ false,
                /* throughGroupTitle= */ false,
                animators);
        mAnimationHost.startAnimations(
                animators,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mInteractingViewDuringStop = null;
                    }
                });
    }

    // ============================================================================================
    // IN-TEST
    // ============================================================================================

    StripLayoutView getInteractingViewDuringStopForTesting() {
        return mInteractingViewDuringStop;
    }
}

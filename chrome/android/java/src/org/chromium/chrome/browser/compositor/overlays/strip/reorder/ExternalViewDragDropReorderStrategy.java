// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
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
            Supplier<Float> tabWidthSupplier,
            Supplier<Long> lastReorderScrollTimeSupplier) {
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
    }

    /** Initiate reorder when external view is dragged onto strip. */
    @Override
    public void startReorderMode(
            StripLayoutView[] stripViews,
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle[] stripGroupTitles,
            @NonNull StripLayoutView interactingView,
            PointF startPoint) {
        // 1. Set initial state and add edge margins.
        mInteractingView = interactingView;
        mInteractingViewDuringStop = null;
        mAnimationHost.finishAnimationsAndPushTabUpdates();
        setEdgeMarginsForReorder(stripTabs);

        // 2. Add a trailing margin to the interacting view to indicate where the view will be
        // inserted should the drag be dropped.
        ArrayList<Animator> animationList = new ArrayList<>();
        setInteractingStateForView(
                interactingView, stripGroupTitles, /* isInteracting= */ true, animationList);

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

        // 2. Clear previous "interacting" view if inserting at the start of the strip.
        final float leftEdge;
        final float rightEdge;
        if (stripViews[0] instanceof StripLayoutTab tab) {
            leftEdge = tab.getTouchTargetLeft();
            rightEdge = tab.getTouchTargetRight();
        } else {
            StripLayoutGroupTitle groupTitle = (StripLayoutGroupTitle) stripViews[0];
            leftEdge = groupTitle.getDrawX();
            rightEdge = groupTitle.getDrawX() + groupTitle.getWidth();
        }
        boolean inStartGap =
                LocalizationUtils.isLayoutRtl()
                        ? adjustedXForDrop > rightEdge
                        : adjustedXForDrop < leftEdge;

        if (inStartGap && mInteractingView != null) {
            mScrollDelegate.setReorderStartMargin(
                    /* newStartMargin= */ StripLayoutUtils.getHalfTabWidth(mTabWidthSupplier));

            mAnimationHost.finishAnimations();
            ArrayList<Animator> animationList = new ArrayList<>();
            setInteractingStateForView(
                    mInteractingView, groupTitles, /* isInteracting= */ false, animationList);
            mInteractingView = null;
            mAnimationHost.startAnimations(animationList, null);

            // 2.a. Early-out if we just entered the start gap.
            return;
        }
        // 3. Otherwise, update drop indicator if necessary.
        StripLayoutView hoveredView =
                StripLayoutUtils.findViewAtPositionX(
                        stripViews, adjustedXForDrop, /* includeGroupTitles= */ true);

        if (hoveredView != null && hoveredView != mInteractingView) {
            mAnimationHost.finishAnimations();

            // 3.a. Reset the state for the previous "interacting" view.
            ArrayList<Animator> animationList = new ArrayList<>();
            if (mInteractingView != null) {
                setInteractingStateForView(
                        mInteractingView, groupTitles, /* isInteracting= */ false, animationList);
            }

            // 3.b. Set state for the new "interacting" view.
            setInteractingStateForView(
                    hoveredView, groupTitles, /* isInteracting= */ true, animationList);
            mInteractingView = hoveredView;

            // 3.c. Animate.
            mAnimationHost.startAnimations(animationList, null);
        }
    }

    @Override
    public void stopReorderMode(StripLayoutView[] stripViews, StripLayoutGroupTitle[] groupTitles) {
        List<Animator> animatorList = new ArrayList<>();
        handleStopReorderMode(stripViews, groupTitles, mInteractingView, animatorList);
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

    /** Merges dropped tabs to interacting view's tab group, if one exists. */
    boolean handleDrop(StripLayoutGroupTitle[] groupTitles, List<Integer> tabIds, int dropIndex) {
        if (mInteractingViewDuringStop == null) return false;

        @Nullable StripLayoutGroupTitle groupTitle;
        final int destinationTabId;
        if (mInteractingViewDuringStop instanceof StripLayoutTab interactingStripTab) {
            Tab interactingTab = mModel.getTabById(interactingStripTab.getTabId());
            groupTitle =
                    StripLayoutUtils.findGroupTitle(groupTitles, interactingTab.getTabGroupId());
            destinationTabId = interactingTab.getId();
        } else {
            groupTitle = (StripLayoutGroupTitle) mInteractingViewDuringStop;
            destinationTabId = groupTitle.getRootId();
        }

        // 1. If hovered on view is not part of group or is collapsed, no-op.
        if (groupTitle == null || groupTitle.isCollapsed()) {
            mInteractingViewDuringStop = null;
            return false;
        }

        // 2. Merge all tabs in dragged tab group to hovered tab's group at drop index.
        for (int tabId : tabIds) {
            mTabGroupModelFilter.mergeTabsToGroup(
                    tabId, destinationTabId, /* skipUpdateTabModel= */ true);
            mModel.moveTab(tabId, dropIndex);
        }

        // 3. Animate bottom indicator. Done after merging the dragged tab group to group,
        // so that the calculated bottom indicator width will be correct.
        runOnDropAnimation(groupTitle);
        return true;
    }

    private void runOnDropAnimation(StripLayoutGroupTitle groupTitle) {
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

    /** Wrapper for #setTrailingMarginForView and #shouldHaveTrailingMargin. */
    protected void setInteractingStateForView(
            StripLayoutView stripView,
            StripLayoutGroupTitle[] groupTitles,
            boolean isInteracting,
            List<Animator> animationList) {
        setTrailingMarginForView(
                stripView,
                groupTitles,
                shouldHaveTrailingMargin(stripView, isInteracting),
                animationList);
    }

    private boolean shouldHaveTrailingMargin(
            StripLayoutView interactingView, boolean isInteracting) {
        if (!isInteracting) return false;

        if (TabDragSource.canMergeIntoGroupOnDrop()) return true;

        // Skip applying trailing margin for grouped views (like expanded group titles or tabs) when
        // merging on drop is not allowed.
        if (interactingView instanceof StripLayoutGroupTitle groupTitle) {
            return groupTitle.isCollapsed();
        } else {
            assert interactingView instanceof StripLayoutTab : "Unexpected view type";
            return !StripLayoutUtils.isNonTrailingTabInGroup(
                    mTabGroupModelFilter, mModel, (StripLayoutTab) interactingView);
        }
    }

    // ============================================================================================
    // IN-TEST
    // ============================================================================================

    StripLayoutView getInteractingViewDuringStopForTesting() {
        return mInteractingViewDuringStop;
    }
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.graphics.PointF;
import android.view.View;

import org.chromium.base.MathUtils;
import org.chromium.base.Token;
import org.chromium.base.TriState;
import org.chromium.base.TriStateUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
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
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.StripUpdateDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.ui.base.LocalizationUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.function.Supplier;

/**
 * A reorder strategy for handling the dragging of multiple selected tabs as a single contiguous
 * block within the tab strip.
 */
@NullMarked
public class MultiTabReorderStrategy extends ReorderStrategyBase {
    private final List<StripLayoutView> mInteractingViews = new ArrayList<>();
    private final List<StripLayoutTab> mInteractingTabs = new ArrayList<>();
    private final List<StripLayoutTab> mPinnedTabs = new ArrayList<>();
    private final List<StripLayoutTab> mUnpinnedTabs = new ArrayList<>();
    private final List<StripLayoutView> mUnpinnedViews = new ArrayList<>();
    private final HashSet<Integer> mInteractingTabIds = new HashSet<>();
    private final HashSet<Token> mInteractingGroupIds = new HashSet<>();
    @Nullable private StripLayoutTab mPrimaryInteractingStripTab;
    private final Supplier<Boolean> mInReorderModeSupplier;
    private final Supplier<Float> mPinnedTabsBoundarySupplier;
    private @TriState int mHasMixedPinState;
    private @TriState int mIsPrimaryPinned;
    private float mLastScrollOffset;

    /**
     * Constructs a new reorder strategy for handling a multi-tab drag selection.
     *
     * @param reorderDelegate The {@link ReorderDelegate} to handle reorder operations.
     * @param stripUpdateDelegate The {@link StripUpdateDelegate} for strip UI updates.
     * @param animationHost The {@link AnimationHost} for running animations.
     * @param scrollDelegate The {@link ScrollDelegate} for handling strip scrolling.
     * @param model The {@link TabModel} for tab data.
     * @param containerView The container {@link View}.
     * @param groupIdToHideSupplier The supplier for the group ID to hide.
     * @param tabWidthSupplier The supplier for the tab width.
     * @param lastReorderScrollTimeSupplier The supplier for the last reorder scroll time.
     * @param inReorderModeSupplier The supplier to check if reorder mode is active.
     */
    MultiTabReorderStrategy(
            ReorderDelegate reorderDelegate,
            StripUpdateDelegate stripUpdateDelegate,
            AnimationHost animationHost,
            ScrollDelegate scrollDelegate,
            TabModel model,
            View containerView,
            SettableNullableObservableSupplier<Token> groupIdToHideSupplier,
            Supplier<Float> tabWidthSupplier,
            Supplier<Float> pinnedTabsBoundarySupplier,
            Supplier<Long> lastReorderScrollTimeSupplier,
            Supplier<Boolean> inReorderModeSupplier) {
        super(
                reorderDelegate,
                stripUpdateDelegate,
                animationHost,
                scrollDelegate,
                model,
                containerView,
                groupIdToHideSupplier,
                tabWidthSupplier,
                lastReorderScrollTimeSupplier);
        mInReorderModeSupplier = inReorderModeSupplier;
        mPinnedTabsBoundarySupplier = pinnedTabsBoundarySupplier;
    }

    @Override
    public void startReorderMode(
            StripLayoutView[] stripViews,
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle[] stripGroupTitles,
            StripLayoutView interactingView,
            PointF startPoint) {
        RecordUserAction.record("MobileToolbarStartMultiTabReorder");
        if (!prepareAndGatherBlock((StripLayoutTab) interactingView, stripTabs, stripGroupTitles)) {
            return;
        }
        assumeNonNull(mPrimaryInteractingStripTab);

        for (StripLayoutView view : mInteractingViews) {
            view.setIsForegrounded(/* isForegrounded= */ true);
        }

        // TODO(crbug.com/434911413): Gather step requires animation.
        mAnimationHost.finishAnimationsAndPushTabUpdates();
        setEdgeMarginsForReorder(stripTabs);

        ArrayList<Animator> animationList = new ArrayList<>();
        updateTabAttachState(mPrimaryInteractingStripTab, /* attached= */ false, animationList);
        StripLayoutUtils.performHapticFeedback(mContainerView);
        mAnimationHost.startAnimations(animationList, /* listener= */ null);
        mLastScrollOffset = mScrollDelegate.getScrollOffset();
    }

    @Override
    public void updateReorderPosition(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            float endX,
            float deltaX,
            @ReorderType int reorderType) {
        assert !mInteractingTabs.isEmpty();
        assert mPrimaryInteractingStripTab != null;

        int curIndex =
                StripLayoutUtils.findIndexForTab(stripTabs, mPrimaryInteractingStripTab.getTabId());
        if (curIndex == TabModel.INVALID_TAB_INDEX) return;

        StripLayoutTab tabToReorder;
        List<StripLayoutTab> tabsToReorder;
        if (mHasMixedPinState == TriState.TRUE) {
            boolean isPinnedReordering = isPinnedReordering(deltaX);
            tabsToReorder = isPinnedReordering ? mPinnedTabs : mUnpinnedTabs;
            tabToReorder = tabsToReorder.get(0);
        } else {
            tabsToReorder = mInteractingTabs;
            tabToReorder = mPrimaryInteractingStripTab;
        }
        float offset = tabToReorder.getOffsetX() + deltaX;
        float oldIdealX = tabToReorder.getIdealX();
        float oldScrollOffset = mScrollDelegate.getScrollOffset();
        float oldStartMargin = mScrollDelegate.getReorderStartMargin();

        boolean reordered =
                reorderBlockIfThresholdReached(
                        stripViews, groupTitles, stripTabs, offset, tabsToReorder);
        if (reordered) {
            if (!mInReorderModeSupplier.get()) return;
            setEdgeMarginsForReorder(stripTabs);

            offset =
                    adjustOffsetAfterReorder(
                            tabToReorder,
                            offset,
                            deltaX,
                            oldIdealX,
                            oldScrollOffset,
                            oldStartMargin);
        }

        StripLayoutTab firstTabInBlock = mInteractingTabs.get(0);
        StripLayoutTab lastTabInBlock = mInteractingTabs.get(mInteractingTabs.size() - 1);
        if (firstTabInBlock == null || lastTabInBlock == null) return;

        int firstTabIndex = StripLayoutUtils.findIndexForTab(stripTabs, firstTabInBlock.getTabId());
        int lastTabIndex = StripLayoutUtils.findIndexForTab(stripTabs, lastTabInBlock.getTabId());
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        StripLayoutView firstView = stripViews[0];
        StripLayoutView lastView = stripViews[stripViews.length - 1];

        // Case 1. If the tab block is at the strip's edge (first or last position), trim the
        // movement offset based on the relevant margin(e.g. start, trailing, or group drag out
        // threshold).
        if (firstTabIndex == 0 || lastTabIndex == stripTabs.length - 1) {
            if (firstTabIndex == 0) {
                float limit;
                if (firstView instanceof StripLayoutGroupTitle groupTitle) {
                    limit =
                            mInteractingGroupIds.isEmpty()
                                    ? getDragOutThreshold(groupTitle, /* towardEnd= */ false)
                                    : 0f;
                } else {
                    limit = mScrollDelegate.getReorderStartMargin();
                }
                offset = isRtl ? Math.min(limit, offset) : Math.max(-limit, offset);
                setOffsetXForPartitionAndAnchorOther(offset, firstTabInBlock.getIsPinned());
            }
            if (lastTabIndex == stripTabs.length - 1) {
                float limit =
                        mInteractingGroupIds.isEmpty() ? lastTabInBlock.getTrailingMargin() : 0f;
                offset = isRtl ? Math.max(-limit, offset) : Math.min(limit, offset);
                setOffsetXForPartitionAndAnchorOther(offset, lastTabInBlock.getIsPinned());
            }
        } else {
            // Case 2. If the tab strip has both pinned and unpinned tabs, clamp the x-offset when
            // dragging an unpinned block toward the start or a pinned block toward the end. The
            // limit is determined by the boundary of the respective first or last view on the tab
            // strip.
            boolean towardEnd = isOffsetTowardEnd(offset);
            boolean isDraggingUnpinnedToPinnedStart =
                    !mInteractingTabs.get(0).getIsPinned() && !towardEnd;
            boolean isDraggingPinnedToUnpinnedEnd =
                    mInteractingTabs.get(mInteractingTabs.size() - 1).getIsPinned() && towardEnd;
            if (isDraggingUnpinnedToPinnedStart) {
                float limit = getDragOffsetLimit(mInteractingViews.get(0), firstView, offset > 0);
                offset = isRtl ? Math.min(limit, offset) : Math.max(limit, offset);
                for (StripLayoutView view : mInteractingViews) {
                    view.setOffsetX(offset);
                }
            } else if (isDraggingPinnedToUnpinnedEnd) {
                float limit = getDragOffsetLimit(lastTabInBlock, lastView, offset > 0);
                offset = isRtl ? Math.max(limit, offset) : Math.min(limit, offset);
                for (StripLayoutView view : mInteractingViews) {
                    view.setOffsetX(offset);
                }
            } else {
                // case 3. Drag is not near a strip edge and not crossing the pinned/unpinned
                // boundary, so apply the x-offset without clamping.
                float scrollOffsetDelta =
                        MathUtils.flipSignIf(oldScrollOffset - mLastScrollOffset, isRtl);
                if (reordered) {
                    setOffsetXForPartitionAndAnchorOther(offset, tabToReorder.getIsPinned());
                } else {
                    for (StripLayoutView view : mInteractingViews) {
                        // If no reorder occurs, simply include deltaX and scrollOffsetDelta(Pinned
                        // tab only) to x-offset.
                        view.setOffsetX(
                                view.getOffsetX()
                                        + deltaX
                                        + (isViewPinned(view) ? scrollOffsetDelta : 0f));
                    }
                }
            }
        }
        mLastScrollOffset = oldScrollOffset;
    }

    private void setOffsetXForPartitionAndAnchorOther(float offset, boolean isPinned) {
        List<? extends StripLayoutView> views = isPinned ? mPinnedTabs : mUnpinnedViews;
        for (StripLayoutView view : views) {
            view.setOffsetX(offset);
        }
        setOffsetXForNonReorderedPartition(isPinned);
    }

    private boolean isPinnedReordering(float deltaX) {
        boolean rtl = LocalizationUtils.isLayoutRtl();
        StripLayoutTab firstUnpinnedTab = mUnpinnedTabs.get(0);
        Tab tab = mModel.getTabById(firstUnpinnedTab.getTabId());
        assumeNonNull(tab);
        if (isTabInNonRetainedGroup(tab)) {
            return false;
        }

        StripLayoutView firstUnpinnedView = mUnpinnedViews.get(0);
        float firstUnpinnedPosition =
                firstUnpinnedView.getIdealX()
                        + (rtl ? getEffectiveViewWidth(firstUnpinnedView) : 0f)
                        + firstUnpinnedView.getOffsetX()
                        + deltaX;
        boolean crossIntoPinnedBoundary =
                MathUtils.flipSignIf(
                                firstUnpinnedPosition - mPinnedTabsBoundarySupplier.get(),
                                LocalizationUtils.isLayoutRtl())
                        < 0f;
        return crossIntoPinnedBoundary;
    }

    @Override
    public void reorderViewInDirection(
            StripLayoutTabDelegate tabDelegate,
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            StripLayoutView reorderingView,
            boolean toLeft) {
        // Cast to the correct view type.
        assert reorderingView instanceof StripLayoutTab && mModel.getMultiSelectedTabsCount() > 1
                : "Using incorrect ReorderStrategy for view type.";

        if (!prepareAndGatherBlock((StripLayoutTab) reorderingView, stripTabs, groupTitles)) {
            return;
        }

        // Update stripTabs from stripViews, as the model update in gatherBlock invalidates the
        // original array.
        ArrayList<StripLayoutTab> filteredTabs = new ArrayList<>();
        for (StripLayoutView view : stripViews) {
            if (view instanceof StripLayoutTab tab) {
                filteredTabs.add(tab);
            }
        }
        stripTabs = filteredTabs.toArray(new StripLayoutTab[0]);

        // Fake a successful reorder in the target direction.
        float offset = MathUtils.flipSignIf(Float.MAX_VALUE, toLeft);
        List<StripLayoutTab> tabsToReorder =
                isViewPinned(reorderingView) ? mPinnedTabs : mUnpinnedTabs;
        reorderBlockIfThresholdReached(stripViews, groupTitles, stripTabs, offset, tabsToReorder);

        // Animate the reordering views and ensure they are foregrounded.
        updateInteractingViewsForNonDragReorder(tabDelegate, /* isReordering= */ true);
        for (StripLayoutView view : mInteractingViews) {
            if (view != reorderingView) {
                animateViewSliding(view);
            }
        }
        animateViewSliding(
                reorderingView,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        updateInteractingViewsForNonDragReorder(
                                tabDelegate, /* isReordering= */ false);
                        clearReorderState();
                    }
                });
    }

    private void updateInteractingViewsForNonDragReorder(
            StripLayoutTabDelegate tabDelegate, boolean isReordering) {
        for (StripLayoutView view : mInteractingViews) {
            view.setIsForegrounded(isReordering);
            if (view instanceof StripLayoutTab tab) {
                tabDelegate.setIsTabNonDragReordering(tab, isReordering);
            } else {
                view.setIsNonDragReordering(isReordering);
            }
        }
    }

    @Override
    protected void setEdgeMarginsForReorder(StripLayoutTab[] stripTabs) {
        if (!mInteractingGroupIds.isEmpty()) return;
        super.setEdgeMarginsForReorder(stripTabs);
    }

    @Override
    public void stopReorderMode(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            boolean isDragCancelled) {
        mAnimationHost.finishAnimationsAndPushTabUpdates();
        ArrayList<Animator> animationList = new ArrayList<>();
        Runnable onAnimationEnd =
                () -> {
                    for (StripLayoutView view : mInteractingViews) {
                        if (view != null) {
                            view.setIsForegrounded(false);
                        }
                    }
                    clearReorderState();
                };
        handleStopReorderMode(
                stripViews,
                groupTitles,
                mInteractingViews,
                mPrimaryInteractingStripTab,
                animationList,
                onAnimationEnd);
    }

    private boolean prepareAndGatherBlock(
            StripLayoutTab primaryStripTab,
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle[] groupTitles) {
        mPrimaryInteractingStripTab = primaryStripTab;
        Tab primaryTab = mModel.getTabById(primaryStripTab.getTabId());
        if (primaryTab == null) {
            mPrimaryInteractingStripTab = null;
            return false;
        }
        TabModelUtils.setIndex(mModel, mModel.indexOf(primaryTab));
        List<Tab> selectedTabs = getSortedSelectedTabs(stripTabs);
        setupReorderState(stripTabs, groupTitles, selectedTabs);
        gatherBlock(primaryTab);
        return true;
    }

    private void setupReorderState(
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle[] stripGroupTitles,
            List<Tab> selectedTabs) {
        // Ensure state is clean before starting a new reorder.
        assert mInteractingViews.isEmpty();
        assert mInteractingTabs.isEmpty();
        assert mInteractingTabIds.isEmpty();
        assert mInteractingGroupIds.isEmpty();

        for (Tab tab : selectedTabs) {
            int tabId = tab.getId();
            StripLayoutTab stripTab = StripLayoutUtils.findTabById(stripTabs, tabId);
            mInteractingTabs.add(stripTab);
            mInteractingTabIds.add(tabId);
            if (tab.getIsPinned()) {
                mPinnedTabs.add(stripTab);
            } else {
                Token groupId = tab.getTabGroupId();
                if (mModel.isTabInTabGroup(tab)
                        && groupId != null
                        && !mInteractingGroupIds.contains(groupId)
                        && areAllTabsInGroupSelected(groupId)) {
                    mInteractingGroupIds.add(groupId);
                    StripLayoutGroupTitle groupTitle =
                            StripLayoutUtils.findGroupTitle(stripGroupTitles, groupId);
                    if (groupTitle != null) {
                        mUnpinnedViews.add(groupTitle);
                    }
                }
                mUnpinnedTabs.add(stripTab);
                mUnpinnedViews.add(stripTab);
            }
        }
        mInteractingViews.addAll(mPinnedTabs);
        mInteractingViews.addAll(mUnpinnedViews);
        mIsPrimaryPinned =
                TriStateUtils.from(
                        Objects.requireNonNull(mPrimaryInteractingStripTab).getIsPinned());
        mHasMixedPinState = TriStateUtils.from(mPinnedTabs.size() > 0 && mUnpinnedTabs.size() > 0);
    }

    private boolean areAllTabsInGroupSelected(Token groupId) {
        List<Tab> tabsInGroup = mModel.getTabsInGroup(groupId);
        if (tabsInGroup.isEmpty()) return false;
        for (Tab tab : tabsInGroup) {
            if (!mModel.isTabMultiSelected(tab.getId())) {
                return false;
            }
        }
        return true;
    }

    private boolean isTabInRetainedGroup(Tab tab) {
        return mModel.isTabInTabGroup(tab) && mInteractingGroupIds.contains(tab.getTabGroupId());
    }

    private boolean isTabInNonRetainedGroup(Tab tab) {
        return mModel.isTabInTabGroup(tab) && !mInteractingGroupIds.contains(tab.getTabGroupId());
    }

    private float getEffectiveViewWidth(StripLayoutView view) {
        if (view instanceof StripLayoutTab tab) {
            return tab.isCollapsed() ? 0f : getEffectiveTabWidth(tab.getIsPinned());
        }
        return view.getWidth();
    }

    private static boolean isViewPinned(StripLayoutView view) {
        return view instanceof StripLayoutTab tab && tab.getIsPinned();
    }

    private void clearReorderState() {
        mInteractingViews.clear();
        mInteractingTabs.clear();
        mInteractingTabIds.clear();
        mInteractingGroupIds.clear();
        mPinnedTabs.clear();
        mUnpinnedTabs.clear();
        mUnpinnedViews.clear();
        mIsPrimaryPinned = TriState.NOT_SET;
        mHasMixedPinState = TriState.NOT_SET;
        mPrimaryInteractingStripTab = null;
        mLastScrollOffset = 0f;
    }

    private void gatherBlock(Tab primaryTab) {
        boolean isPrimaryPinned = mIsPrimaryPinned == TriState.TRUE;
        Token destinationGroupId = primaryTab.getTabGroupId();
        if (destinationGroupId != null && mInteractingGroupIds.isEmpty()) {
            int primaryTabIndexInGroup =
                    mModel.getTabsInGroup(destinationGroupId).indexOf(primaryTab);
            mModel.mergeListOfTabsToExistingGroup(
                    convertStripTabsToTabs(mUnpinnedTabs),
                    destinationGroupId,
                    /* indexInGroup= */ primaryTabIndexInGroup,
                    /* notify= */ TabGroupMergeNotificationType.DONT_NOTIFY);
        } else {
            ungroupInteractingBlock();
            List<StripLayoutTab> primaryPartition = isPrimaryPinned ? mPinnedTabs : mUnpinnedTabs;
            int primaryIndexInPartition = primaryPartition.indexOf(mPrimaryInteractingStripTab);
            int leftPartitionIndex = primaryIndexInPartition;
            int rightPartitionIndex = primaryIndexInPartition;
            int leftEdgeIndex = mModel.indexOf(primaryTab);
            int rightEdgeIndex = leftEdgeIndex;
            if (isTabInRetainedGroup(primaryTab)) {
                List<Tab> primaryGroupTabs =
                        mModel.getTabsInGroup(assumeNonNull(primaryTab.getTabGroupId()));
                leftPartitionIndex -= primaryGroupTabs.indexOf(primaryTab);
                rightPartitionIndex = leftPartitionIndex + primaryGroupTabs.size() - 1;
                leftEdgeIndex = mModel.indexOf(primaryGroupTabs.get(0));
                rightEdgeIndex = mModel.indexOf(primaryGroupTabs.get(primaryGroupTabs.size() - 1));
            }

            // Gather tabs/groups before the primary tab/group (iterating right-to-left).
            for (int i = leftPartitionIndex - 1; i >= 0; i--) {
                int numTabs = gatherItemToIndex(primaryPartition.get(i), leftEdgeIndex - 1);
                leftEdgeIndex -= numTabs;
                i -= numTabs - 1;
            }

            // Gather tabs/groups after the primary tab/group (iterating left-to-right).
            for (int i = rightPartitionIndex + 1; i < primaryPartition.size(); i++) {
                int numTabs = gatherItemToIndex(primaryPartition.get(i), rightEdgeIndex + 1);
                rightEdgeIndex += numTabs;
                i += numTabs - 1;
            }
        }

        if (mHasMixedPinState == TriState.TRUE) {
            if (isPrimaryPinned) {
                int firstUnpinnedIndex = mModel.findFirstNonPinnedTabIndex();
                for (int i = 0; i < mUnpinnedTabs.size(); i++) {
                    int numTabs = gatherItemToIndex(mUnpinnedTabs.get(i), firstUnpinnedIndex);
                    firstUnpinnedIndex += numTabs;
                    i += numTabs - 1;
                }
            } else {
                int lastPinnedIndex = mModel.findFirstNonPinnedTabIndex() - 1;
                for (int i = mPinnedTabs.size() - 1; i >= 0; i--) {
                    mModel.moveTab(mPinnedTabs.get(i).getTabId(), lastPinnedIndex--);
                }
            }
            setOffsetXForNonReorderedPartition(isPrimaryPinned);
        }
    }

    /**
     * Moves {@code stripTab} (or its entire group if in a retained group) to {@code targetIndex} if
     * not already there. Note that {@link TabModel#moveGroupToIndex} expects the destination index
     * of the group's last tab when moving rightward (higher index) and its first tab when moving
     * leftward (lower index), which matches {@code targetIndex} when iterating right-to-left and
     * left-to-right respectively.
     *
     * @return The number of tabs in the gathered item (1 for a tab, or group size for a group).
     */
    private int gatherItemToIndex(StripLayoutTab stripTab, int targetIndex) {
        Tab tab = assumeNonNull(mModel.getTabById(stripTab.getTabId()));
        if (isTabInRetainedGroup(tab)) {
            Token groupId = assumeNonNull(tab.getTabGroupId());
            if (mModel.indexOf(tab) != targetIndex) {
                mModel.moveGroupToIndex(groupId, targetIndex);
            }
            return mModel.getTabCountForGroup(groupId);
        }
        if (mModel.indexOf(tab) != targetIndex) {
            mModel.moveTab(tab.getId(), targetIndex);
        }
        return 1;
    }

    /**
     * Positions the non-reordering partition to visually “gather” around the reordered (anchoring)
     * partition. This is used only when the current selection mixes pinned and unpinned tabs.
     * Exactly one partition actually reorders to primary tab in the model; the other partition’s
     * tabs are repositioned by updating their {@code offsetX} so the whole selection appears
     * contiguous.
     *
     * @param anchorOnPinned {@code true} to anchor on the pinned block (position unpinned around
     *     it); {@code false} to anchor on the unpinned block (position pinned around it).
     */
    private void setOffsetXForNonReorderedPartition(boolean anchorOnPinned) {
        if (mHasMixedPinState != TriState.TRUE) return;

        boolean rtl = LocalizationUtils.isLayoutRtl();
        if (anchorOnPinned) {
            StripLayoutTab anchorTab = mPinnedTabs.get(mPinnedTabs.size() - 1);
            float anchorX =
                    anchorTab.getIdealX()
                            + anchorTab.getOffsetX()
                            + (rtl ? 0f : getEffectiveViewWidth(anchorTab));
            for (StripLayoutView view : mUnpinnedViews) {
                float viewWidth = getEffectiveViewWidth(view);
                if (rtl) anchorX -= viewWidth;
                view.setOffsetX(anchorX - view.getIdealX());
                if (!rtl) anchorX += viewWidth;
            }
        } else {
            StripLayoutView anchorView = mUnpinnedViews.get(0);
            float anchorX =
                    anchorView.getIdealX()
                            + anchorView.getOffsetX()
                            + (rtl ? getEffectiveViewWidth(anchorView) : 0f);
            for (int i = mPinnedTabs.size() - 1; i >= 0; i--) {
                StripLayoutTab tab = mPinnedTabs.get(i);
                float tabWidth = getEffectiveViewWidth(tab);
                if (!rtl) anchorX -= tabWidth;
                tab.setOffsetX(anchorX - tab.getIdealX());
                if (rtl) anchorX += tabWidth;
            }
        }
    }

    private List<Tab> convertStripTabsToTabs(List<StripLayoutTab> stripTabs) {
        List<Tab> tabs = new ArrayList<>();
        for (StripLayoutTab stripTab : stripTabs) {
            tabs.add(mModel.getTabById(stripTab.getTabId()));
        }
        return tabs;
    }

    /**
     * Determines if the drag offset has surpassed a reorder threshold and executes the reorder if
     * it has. A reorder can be a simple tab swap, moving past a group, merging into a group, or
     * dragging out of a group.
     *
     * @param stripViews All views currently in the strip.
     * @param groupTitles All group title views in the strip.
     * @param stripTabs All tab views in the strip.
     * @param offset The current drag offset from the tab's ideal position.
     * @return True if a reorder occurred, false otherwise.
     */
    private boolean reorderBlockIfThresholdReached(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            float offset,
            List<StripLayoutTab> tabsToReorder) {
        assert mPrimaryInteractingStripTab != null;

        boolean towardEnd = isOffsetTowardEnd(offset);
        StripLayoutTab reorderEdgeTab =
                towardEnd ? tabsToReorder.get(tabsToReorder.size() - 1) : tabsToReorder.get(0);
        if (reorderEdgeTab == null) return false;
        Tab edgeTab = mModel.getTabById(reorderEdgeTab.getTabId());
        if (edgeTab == null) return false;
        int edgeTabIndexInStrip =
                StripLayoutUtils.findIndexForTab(stripTabs, reorderEdgeTab.getTabId());

        int adjTabIndex = towardEnd ? edgeTabIndexInStrip + 1 : edgeTabIndexInStrip - 1;
        Tab adjTab = mModel.getTabAt(adjTabIndex);

        // If pinned state or selection state is ineligible, adjTab cannot be reordered past.
        if (adjTab != null && adjTab.getIsPinned() != reorderEdgeTab.getIsPinned()) adjTab = null;
        if (adjTab != null && mInteractingTabIds.contains(adjTab.getId())) return false;

        boolean isInNonRetainedGroup = isTabInNonRetainedGroup(edgeTab);
        boolean isAdjInGroup = adjTab != null && mModel.isTabInTabGroup(adjTab);
        boolean areEdgeAndAdjInSameGroup =
                isInNonRetainedGroup
                        && isAdjInGroup
                        && Objects.equals(
                                edgeTab.getTabGroupId(), assumeNonNull(adjTab).getTabGroupId());

        // Case 1: Maybe drag out of non-retained group.
        if (isInNonRetainedGroup && !areEdgeAndAdjInSameGroup) {
            StripLayoutGroupTitle interactingGroupTitle =
                    StripLayoutUtils.findGroupTitle(groupTitles, edgeTab.getTabGroupId());
            assumeNonNull(interactingGroupTitle);
            if (Math.abs(offset) <= getDragOutThreshold(interactingGroupTitle, towardEnd)) {
                return false;
            }
            List<StripLayoutTab> interactingTabs = new ArrayList<>(mUnpinnedTabs);
            if (towardEnd) Collections.reverse(interactingTabs);
            moveInteractingTabsOutOfGroup(
                    stripViews,
                    groupTitles,
                    interactingTabs,
                    interactingGroupTitle,
                    towardEnd,
                    StripTabModelActionListener.ActionType.REORDER);
            return true;
        }

        if (adjTab == null) return false;

        // Case 2: Not interacting with a different group. Maybe swap with adjacent tab.
        if (!isAdjInGroup || areEdgeAndAdjInSameGroup) {
            if (Math.abs(offset) <= getTabSwapThreshold(/* isPinned= */ adjTab.getIsPinned())) {
                return false;
            }
            moveAdjacentTabPastBlock(adjTab, towardEnd);
            StripLayoutTab adjStripLayoutTab =
                    StripLayoutUtils.findTabById(stripTabs, adjTab.getId());
            animateViewSliding(assumeNonNull(adjStripLayoutTab));
            return true;
        }

        // Case 3: Interacting with an adjacent group.
        StripLayoutGroupTitle interactingGroupTitle =
                StripLayoutUtils.findGroupTitle(groupTitles, adjTab.getTabGroupId());
        if (interactingGroupTitle == null) return false;

        if (interactingGroupTitle.isCollapsed() || !mInteractingGroupIds.isEmpty()) {
            // Case 3a: Maybe drag past group (collapsed group, or expanded group when dragging a
            // retained group).
            if (Math.abs(offset) <= getGroupSwapThreshold(interactingGroupTitle)) return false;
            moveAdjacentGroupPastBlock(interactingGroupTitle, adjTab.getIsPinned(), towardEnd);
            animateGroupSliding(interactingGroupTitle, stripTabs);
            return true;
        } else {
            // Case 3b: Maybe merge into group.
            if (Math.abs(offset) <= getDragInThreshold()) return false;
            mergeBlockIntoGroup(interactingGroupTitle, towardEnd);
            return true;
        }
    }

    @Override
    public @Nullable StripLayoutView getInteractingView() {
        return mPrimaryInteractingStripTab;
    }

    /**
     * Moves a single adjacent tab to the other side of the interacting block of tabs.
     *
     * @param adjTab The adjacent tab to move.
     * @param towardEnd True if the drag is toward the end of the strip (RTL: left, LTR: right).
     */
    private void moveAdjacentTabPastBlock(Tab adjTab, boolean towardEnd) {
        int destinationIndex = getDestinationIndex(towardEnd, adjTab.getIsPinned());
        if (destinationIndex == TabModel.INVALID_TAB_INDEX) return;
        mModel.moveTab(adjTab.getId(), destinationIndex);
    }

    /**
     * Moves an entire adjacent tab group to the other side of the interacting block of tabs.
     *
     * @param adjGroupTitle The title of the group to move.
     * @param isAdjPinned Whether the adjacent tab the group is dragging across is pinned.
     * @param towardEnd True if the drag is toward the end of the strip (RTL: left, LTR: right).
     */
    private void moveAdjacentGroupPastBlock(
            StripLayoutGroupTitle adjGroupTitle, boolean isAdjPinned, boolean towardEnd) {
        int destinationIndex = getDestinationIndex(towardEnd, isAdjPinned);
        mModel.moveGroupToIndex(adjGroupTitle.getTabGroupId(), destinationIndex);
    }

    /**
     * Calculates the destination index in the {@link TabModel} for an item being moved past the
     * interacting block.
     *
     * @param towardEnd True if the drag is toward the end of the strip (RTL: left, LTR: right).
     * @param isPinned Whether the interacting tab block is pinned.
     * @return The model index where the item should be moved.
     */
    private int getDestinationIndex(boolean towardEnd, boolean isPinned) {
        List<StripLayoutTab> block = isPinned ? mPinnedTabs : mUnpinnedTabs;
        if (block.isEmpty()) return TabModel.INVALID_TAB_INDEX;
        // If the block is moving toward the end, move the adjacent item to the start of the block;
        // otherwise move it to the end of the block.
        StripLayoutTab edgeTab = towardEnd ? block.get(0) : block.get(block.size() - 1);
        return TabModelUtils.getTabIndexById(mModel, edgeTab.getTabId());
    }

    /**
     * Merges all tabs in the interacting block into an adjacent, expanded tab group.
     *
     * @param adjTitle The title view of the destination group.
     * @param towardEnd True if the drag is toward the end of the strip (RTL: left, LTR: right),
     *     which determines if the block is merged to the front or back of the group.
     */
    private void mergeBlockIntoGroup(StripLayoutGroupTitle adjTitle, boolean towardEnd) {
        RecordUserAction.record("MobileToolbarReorderTab.TabsAddedToGroup");
        mModel.mergeListOfTabsToExistingGroup(
                convertStripTabsToTabs(mUnpinnedTabs),
                adjTitle.getTabGroupId(),
                /* indexInGroup= */ towardEnd ? 0 : null,
                /* notify= */ TabGroupMergeNotificationType.DONT_NOTIFY);
        animateGroupIndicatorForTabReorder(adjTitle, /* isMovingOutOfGroup= */ false, towardEnd);
    }

    /**
     * Gets a list of {@link Tab} objects that are currently selected, sorted by their visual order
     * on the tab strip.
     *
     * @param stripTabs The array of tab views on the strip, used to determine visual order.
     * @return A sorted {@link List} of selected {@link Tab}s.
     */
    private List<Tab> getSortedSelectedTabs(StripLayoutTab[] stripTabs) {
        List<Tab> sortedTabs = new ArrayList<>();
        for (StripLayoutTab stripTab : stripTabs) {
            if (stripTab != null && mModel.isTabMultiSelected(stripTab.getTabId())) {
                sortedTabs.add(mModel.getTabById(stripTab.getTabId()));
            }
        }
        return sortedTabs;
    }

    /** Ungroups all tabs currently in the interacting block that are not in a retained group. */
    private void ungroupInteractingBlock() {
        List<Tab> tabsToUngroup = new ArrayList<>();
        for (StripLayoutTab slt : mInteractingTabs) {
            Tab tab = mModel.getTabById(slt.getTabId());
            assert tab != null;
            if (isTabInNonRetainedGroup(tab)) {
                tabsToUngroup.add(tab);
            }
        }
        mModel.getTabUngrouper()
                .ungroupTabs(tabsToUngroup, /* trailing= */ false, /* allowDialog= */ false, null);
    }

    public void clearReorderStateForTesting() {
        clearReorderState();
    }
}

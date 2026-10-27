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
    private final List<StripLayoutTab> mInteractingTabs = new ArrayList<>();
    private final List<StripLayoutTab> mPinnedTabs = new ArrayList<>();
    private final List<StripLayoutTab> mUnpinnedTabs = new ArrayList<>();
    private final HashSet<Integer> mInteractingTabIds = new HashSet<>();
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
        if (!prepareAndGatherBlock((StripLayoutTab) interactingView, stripTabs)) {
            return;
        }
        assumeNonNull(mPrimaryInteractingStripTab);

        for (StripLayoutView view : mInteractingTabs) {
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
                float limit =
                        (firstView instanceof StripLayoutGroupTitle groupTitle)
                                ? getDragOutThreshold(groupTitle, /* towardEnd= */ false)
                                : mScrollDelegate.getReorderStartMargin();
                offset = isRtl ? Math.min(limit, offset) : Math.max(-limit, offset);
                setOffsetXForPartitionAndAnchorOther(offset, firstTabInBlock.getIsPinned());
            }
            if (lastTabIndex == stripTabs.length - 1) {
                offset =
                        isRtl
                                ? Math.max(-lastTabInBlock.getTrailingMargin(), offset)
                                : Math.min(lastTabInBlock.getTrailingMargin(), offset);
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
                float limit = getDragOffsetLimit(firstTabInBlock, firstView, offset > 0);
                offset = isRtl ? Math.min(limit, offset) : Math.max(limit, offset);
                for (StripLayoutTab tab : mInteractingTabs) {
                    tab.setOffsetX(offset);
                }
            } else if (isDraggingPinnedToUnpinnedEnd) {
                float limit = getDragOffsetLimit(lastTabInBlock, lastView, offset > 0);
                offset = isRtl ? Math.max(limit, offset) : Math.min(limit, offset);
                for (StripLayoutTab tab : mInteractingTabs) {
                    tab.setOffsetX(offset);
                }
            } else {
                // case 3. Drag is not near a strip edge and not crossing the pinned/unpinned
                // boundary, so apply the x-offset without clamping.
                float scrollOffsetDelta =
                        MathUtils.flipSignIf(oldScrollOffset - mLastScrollOffset, isRtl);
                if (reordered) {
                    setOffsetXForPartitionAndAnchorOther(offset, tabToReorder.getIsPinned());
                } else {
                    for (StripLayoutTab tab : mInteractingTabs) {
                        // If no reorder occurs, simply include deltaX and scrollOffsetDelta(Pinned
                        // tab only) to x-offset.
                        tab.setOffsetX(
                                tab.getOffsetX()
                                        + deltaX
                                        + (tab.getIsPinned() ? scrollOffsetDelta : 0f));
                    }
                }
            }
        }
        mLastScrollOffset = oldScrollOffset;
    }

    private void setOffsetXForPartitionAndAnchorOther(float offset, boolean isPinned) {
        List<StripLayoutTab> tabs = isPinned ? mPinnedTabs : mUnpinnedTabs;
        for (StripLayoutTab tab : tabs) {
            tab.setOffsetX(offset);
        }
        setOffsetXForNonReorderedPartition(isPinned);
    }

    private boolean isPinnedReordering(float deltaX) {
        boolean rtl = LocalizationUtils.isLayoutRtl();
        StripLayoutTab firstUnpinnedTab = mUnpinnedTabs.get(0);
        Tab tab = mModel.getTabById(firstUnpinnedTab.getTabId());
        assumeNonNull(tab);
        if (mModel.isTabInTabGroup(tab)) return false;

        float firstUnpinnedPosition =
                firstUnpinnedTab.getIdealX()
                        + (rtl ? getEffectiveTabWidth(/* isPinned= */ false) : 0f)
                        + firstUnpinnedTab.getOffsetX()
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

        if (!prepareAndGatherBlock((StripLayoutTab) reorderingView, stripTabs)) {
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
                reorderingView instanceof StripLayoutTab tab && tab.getIsPinned()
                        ? mPinnedTabs
                        : mUnpinnedTabs;
        reorderBlockIfThresholdReached(stripViews, groupTitles, stripTabs, offset, tabsToReorder);

        // Animate the reordering views and ensure they are foregrounded.
        updateInteractingTabsForNonDragReorder(tabDelegate, /* isReordering= */ true);
        for (StripLayoutTab tab : mInteractingTabs) {
            if (tab != reorderingView) {
                animateViewSliding(tab);
            }
        }
        animateViewSliding(
                reorderingView,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        updateInteractingTabsForNonDragReorder(
                                tabDelegate, /* isReordering= */ false);
                        clearReorderState();
                    }
                });
    }

    private void updateInteractingTabsForNonDragReorder(
            StripLayoutTabDelegate tabDelegate, boolean isReordering) {
        for (StripLayoutTab tab : mInteractingTabs) {
            tab.setIsForegrounded(isReordering);
            tabDelegate.setIsTabNonDragReordering(tab, isReordering);
        }
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
                    for (StripLayoutTab tab : mInteractingTabs) {
                        if (tab != null) {
                            tab.setIsForegrounded(false);
                        }
                    }
                    clearReorderState();
                };
        List<StripLayoutView> interactingViews = new ArrayList<>(mInteractingTabs);
        handleStopReorderMode(
                stripViews,
                groupTitles,
                interactingViews,
                mPrimaryInteractingStripTab,
                animationList,
                onAnimationEnd);
    }

    private boolean prepareAndGatherBlock(
            StripLayoutTab primaryStripTab, StripLayoutTab[] stripTabs) {
        mPrimaryInteractingStripTab = primaryStripTab;
        Tab primaryTab = mModel.getTabById(primaryStripTab.getTabId());
        if (primaryTab == null) {
            mPrimaryInteractingStripTab = null;
            return false;
        }
        TabModelUtils.setIndex(mModel, mModel.indexOf(primaryTab));
        List<Tab> selectedTabs = getSortedSelectedTabs(stripTabs);
        setupReorderState(stripTabs, selectedTabs);
        gatherBlock(primaryTab);
        return true;
    }

    private void setupReorderState(StripLayoutTab[] stripTabs, List<Tab> selectedTabs) {
        // Ensure state is clean before starting a new reorder.
        assert mInteractingTabs.isEmpty();
        assert mInteractingTabIds.isEmpty();

        for (Tab tab : selectedTabs) {
            int tabId = tab.getId();
            StripLayoutTab stripTab = StripLayoutUtils.findTabById(stripTabs, tabId);
            mInteractingTabs.add(stripTab);
            mInteractingTabIds.add(tabId);
            if (tab.getIsPinned()) {
                mPinnedTabs.add(stripTab);
            } else {
                mUnpinnedTabs.add(stripTab);
            }
        }
        mIsPrimaryPinned =
                TriStateUtils.from(
                        Objects.requireNonNull(mPrimaryInteractingStripTab).getIsPinned());
        mHasMixedPinState = TriStateUtils.from(mPinnedTabs.size() > 0 && mUnpinnedTabs.size() > 0);
    }

    private void clearReorderState() {
        mInteractingTabs.clear();
        mInteractingTabIds.clear();
        mPinnedTabs.clear();
        mUnpinnedTabs.clear();
        mIsPrimaryPinned = TriState.NOT_SET;
        mHasMixedPinState = TriState.NOT_SET;
        mPrimaryInteractingStripTab = null;
        mLastScrollOffset = 0f;
    }

    private void gatherBlock(Tab primaryTab) {
        boolean isPrimaryPinned = mIsPrimaryPinned == TriState.TRUE;
        Token destinationGroupId = primaryTab.getTabGroupId();
        boolean notAllTabsInPrimaryGroupAreSelected = false;
        List<Tab> tabsInGroup = mModel.getTabsInGroup(destinationGroupId);
        for (Tab tab : tabsInGroup) {
            if (!mInteractingTabIds.contains(tab.getId())) {
                notAllTabsInPrimaryGroupAreSelected = true;
                break;
            }
        }

        if (destinationGroupId != null && notAllTabsInPrimaryGroupAreSelected) {
            int primaryTabIndexInGroup = tabsInGroup.indexOf(primaryTab);
            mModel.mergeListOfTabsToExistingGroup(
                    convertStripTabsToTabs(mUnpinnedTabs),
                    destinationGroupId,
                    /* indexInGroup= */ primaryTabIndexInGroup,
                    /* notify= */ TabGroupMergeNotificationType.DONT_NOTIFY);
        } else {
            ungroupInteractingBlock();
            List<StripLayoutTab> primaryPartition = isPrimaryPinned ? mPinnedTabs : mUnpinnedTabs;
            int primaryIndexInPartition = primaryPartition.indexOf(mPrimaryInteractingStripTab);
            int leftEdgeIndex = mModel.indexOf(primaryTab);
            int rightEdgeIndex = leftEdgeIndex;

            // Gather tabs before the primary tab (iterating right-to-left).
            for (int i = primaryIndexInPartition - 1; i >= 0; i--) {
                Tab tab = assumeNonNull(mModel.getTabById(primaryPartition.get(i).getTabId()));
                if (mModel.indexOf(tab) < leftEdgeIndex - 1) {
                    mModel.moveTab(tab.getId(), leftEdgeIndex - 1);
                }
                leftEdgeIndex--;
            }

            // Gather tabs after the primary tab (iterating left-to-right).
            for (int i = primaryIndexInPartition + 1; i < primaryPartition.size(); i++) {
                Tab tab = assumeNonNull(mModel.getTabById(primaryPartition.get(i).getTabId()));
                if (mModel.indexOf(tab) > rightEdgeIndex + 1) {
                    mModel.moveTab(tab.getId(), rightEdgeIndex + 1);
                }
                rightEdgeIndex++;
            }
        }

        if (mHasMixedPinState == TriState.TRUE) {
            if (isPrimaryPinned) {
                int firstUnpinnedIndex = mModel.findFirstNonPinnedTabIndex();
                for (StripLayoutTab tab : mUnpinnedTabs) {
                    mModel.moveTab(tab.getTabId(), firstUnpinnedIndex++);
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
        StripLayoutTab anchorTab;
        if (anchorOnPinned) {
            anchorTab = mPinnedTabs.get(mPinnedTabs.size() - 1);
            float anchorX =
                    anchorTab.getIdealX()
                            + anchorTab.getOffsetX()
                            + (rtl ? 0f : getEffectiveTabWidth(/* isPinned= */ true));
            for (StripLayoutTab tab : mUnpinnedTabs) {
                float tabWidth = getEffectiveTabWidth(tab.getIsPinned());
                if (rtl) anchorX -= tabWidth;
                tab.setOffsetX(anchorX - tab.getIdealX());
                if (!rtl) anchorX += tabWidth;
            }
        } else {
            anchorTab = mUnpinnedTabs.get(0);
            float anchorX =
                    anchorTab.getIdealX()
                            + anchorTab.getOffsetX()
                            + (rtl ? getEffectiveTabWidth(anchorTab.getIsPinned()) : 0f);
            for (int i = mPinnedTabs.size() - 1; i >= 0; i--) {
                StripLayoutTab tab = mPinnedTabs.get(i);
                float tabWidth = getEffectiveTabWidth(tab.getIsPinned());
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

        boolean isInGroup = mModel.isTabInTabGroup(edgeTab);
        boolean isAdjInGroup = adjTab != null && mModel.isTabInTabGroup(adjTab);
        boolean areEdgeAndAdjInSameGroup =
                isInGroup
                        && isAdjInGroup
                        && Objects.equals(
                                edgeTab.getTabGroupId(), assumeNonNull(adjTab).getTabGroupId());

        // Case 1: Maybe drag out of group.
        if (isInGroup && !areEdgeAndAdjInSameGroup) {
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

        if (interactingGroupTitle.isCollapsed()) {
            // Case 3a: Maybe drag past collapsed group.
            if (Math.abs(offset) <= getGroupSwapThreshold(interactingGroupTitle)) return false;
            moveAdjacentGroupPastBlock(interactingGroupTitle, adjTab.getIsPinned(), towardEnd);
            animateViewSliding(interactingGroupTitle);
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

    /** Ungroups all tabs currently in the interacting block. */
    private void ungroupInteractingBlock() {
        List<Tab> tabsToUngroup = new ArrayList<>();
        for (StripLayoutTab slt : mInteractingTabs) {
            Tab tab = mModel.getTabById(slt.getTabId());
            assert tab != null;
            if (mModel.isTabInTabGroup(tab)) tabsToUngroup.add(tab);
        }
        mModel.getTabUngrouper()
                .ungroupTabs(tabsToUngroup, /* trailing= */ false, /* allowDialog= */ false, null);
    }

    public void clearReorderStateForTesting() {
        clearReorderState();
    }
}

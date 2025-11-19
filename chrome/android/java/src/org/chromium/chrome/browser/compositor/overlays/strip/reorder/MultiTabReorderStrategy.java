// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.getEffectiveTabWidth;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.graphics.PointF;
import android.view.View;

import org.chromium.base.MathUtils;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.EnsuresNonNull;
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
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType;
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
    @Nullable private Boolean mHasMixedPinState;
    @Nullable private Boolean mIsPrimaryPinned;
    private float mLastScrollOffset;

    /**
     * Constructs a new reorder strategy for handling a multi-tab drag selection.
     *
     * @param reorderDelegate The {@link ReorderDelegate} to handle reorder operations.
     * @param stripUpdateDelegate The {@link StripUpdateDelegate} for strip UI updates.
     * @param animationHost The {@link AnimationHost} for running animations.
     * @param scrollDelegate The {@link ScrollDelegate} for handling strip scrolling.
     * @param model The {@link TabModel} for tab data.
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} for group operations.
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
            TabGroupModelFilter tabGroupModelFilter,
            View containerView,
            ObservableSupplierImpl<@Nullable Token> groupIdToHideSupplier,
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
                tabGroupModelFilter,
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
        mPrimaryInteractingStripTab = (StripLayoutTab) interactingView;
        Tab primaryTab = mModel.getTabById(mPrimaryInteractingStripTab.getTabId());
        if (primaryTab == null) {
            mPrimaryInteractingStripTab = null;
            return;
        }
        TabModelUtils.setIndex(mModel, mModel.indexOf(primaryTab));

        List<Tab> selectedTabs = getSortedSelectedTabs(stripTabs);

        setupReorderState(stripTabs, selectedTabs);

        gatherBlock(primaryTab, selectedTabs);

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
        if (Boolean.TRUE.equals(mHasMixedPinState)) {
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
            if (!Boolean.TRUE.equals(mInReorderModeSupplier.get())) return;
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
                for (StripLayoutTab tab : mInteractingTabs) {
                    if (Boolean.TRUE.equals(mHasMixedPinState)
                            && tab.getIsPinned() != firstTabInBlock.getIsPinned()) {
                        setOffsetXForNonReorderedPartition(firstTabInBlock.getIsPinned());
                        break;
                    } else {
                        tab.setOffsetX(offset);
                    }
                }
            }
            if (lastTabIndex == stripTabs.length - 1) {
                offset =
                        isRtl
                                ? Math.max(-lastTabInBlock.getTrailingMargin(), offset)
                                : Math.min(lastTabInBlock.getTrailingMargin(), offset);
                for (int i = mInteractingTabs.size() - 1; i >= 0; i--) {
                    StripLayoutTab tab = mInteractingTabs.get(i);
                    if (Boolean.TRUE.equals(mHasMixedPinState)
                            && tab.getIsPinned() != lastTabInBlock.getIsPinned()) {
                        setOffsetXForNonReorderedPartition(lastTabInBlock.getIsPinned());
                        break;
                    } else {
                        tab.setOffsetX(offset);
                    }
                }
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
                    for (StripLayoutTab tab : tabsToReorder) {
                        tab.setOffsetX(offset);
                    }
                    setOffsetXForNonReorderedPartition(tabToReorder.getIsPinned());
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

    private boolean isPinnedReordering(float deltaX) {
        boolean rtl = LocalizationUtils.isLayoutRtl();
        StripLayoutTab firstUnpinnedTab = mUnpinnedTabs.get(0);
        Tab tab = mTabGroupModelFilter.getTabModel().getTabById(firstUnpinnedTab.getTabId());
        assumeNonNull(tab);
        if (mTabGroupModelFilter.isTabInTabGroup(tab)) return false;

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

        mPrimaryInteractingStripTab = (StripLayoutTab) reorderingView;
        Tab primaryTab = mModel.getTabById(mPrimaryInteractingStripTab.getTabId());
        if (primaryTab == null) {
            mPrimaryInteractingStripTab = null;
            return;
        }
        TabModelUtils.setIndex(mModel, mModel.indexOf(primaryTab));

        List<Tab> selectedTabs = getSortedSelectedTabs(stripTabs);

        setupReorderState(stripTabs, selectedTabs);

        gatherBlock(primaryTab, selectedTabs);

        // Fake a successful reorder in the target direction.
        float offset = MathUtils.flipSignIf(Float.MAX_VALUE, toLeft);
        List<StripLayoutTab> tabsToReorder =
                reorderingView instanceof StripLayoutTab tab && tab.getIsPinned()
                        ? mPinnedTabs
                        : mUnpinnedTabs;
        reorderBlockIfThresholdReached(stripViews, groupTitles, stripTabs, offset, tabsToReorder);

        // Animate the reordering view and ensure it's foregrounded.
        for (StripLayoutTab tab : mInteractingTabs) {
            tabDelegate.setIsTabNonDragReordering(tab, /* isNonDragReordering= */ true);
            tab.setIsForegrounded(/* isForegrounded= */ true);
            animateViewSliding(tab);
        }
        animateViewSliding(
                reorderingView,
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        for (StripLayoutTab tab : mInteractingTabs) {
                            tabDelegate.setIsTabNonDragReordering(
                                    tab, /* isNonDragReordering= */ false);
                            tab.setIsForegrounded(/* isForegrounded= */ false);
                        }
                        clearReorderState();
                    }
                });
    }

    @Override
    public void stopReorderMode(StripLayoutView[] stripViews, StripLayoutGroupTitle[] groupTitles) {
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

    @EnsuresNonNull({"mHasMixedPinState", "mIsPrimaryPinned"})
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
        mIsPrimaryPinned = Objects.requireNonNull(mPrimaryInteractingStripTab).getIsPinned();
        mHasMixedPinState = mPinnedTabs.size() > 0 && mUnpinnedTabs.size() > 0;
    }

    private void clearReorderState() {
        mInteractingTabs.clear();
        mInteractingTabIds.clear();
        mPinnedTabs.clear();
        mUnpinnedTabs.clear();
        mIsPrimaryPinned = null;
        mHasMixedPinState = null;
        mPrimaryInteractingStripTab = null;
        mLastScrollOffset = 0f;
    }

    private void gatherBlock(Tab primaryTab, List<Tab> selectedTabs) {
        boolean isPrimaryTabInGroup = mTabGroupModelFilter.isTabInTabGroup(primaryTab);
        boolean notAllTabsInPrimaryGroupAreSelected = false;
        List<Tab> tabsInGroup = mTabGroupModelFilter.getTabsInGroup(primaryTab.getTabGroupId());
        for (Tab tab : tabsInGroup) {
            if (!mInteractingTabIds.contains(tab.getId())) {
                notAllTabsInPrimaryGroupAreSelected = true;
                break;
            }
        }

        if (isPrimaryTabInGroup && notAllTabsInPrimaryGroupAreSelected) {
            Token destinationGroupId = primaryTab.getTabGroupId();
            assert destinationGroupId != null;
            int primaryTabIndexInGroup = tabsInGroup.indexOf(primaryTab);
            mTabGroupModelFilter.mergeListOfTabsToGroup(
                    convertStripTabsToTabs(mUnpinnedTabs),
                    primaryTab,
                    /* indexInGroup= */ primaryTabIndexInGroup,
                    /* notify= */ MergeNotificationType.DONT_NOTIFY);
            if (Boolean.TRUE.equals(mHasMixedPinState)) {
                for (StripLayoutTab tab : mPinnedTabs) {
                    mModel.moveTab(tab.getTabId(), mModel.findFirstNonPinnedTabIndex() - 1);
                }
            }
        } else {
            ungroupInteractingBlock();
            int primaryTabModelIndex = mModel.indexOf(primaryTab);
            int primaryTabIndexInSelection = selectedTabs.indexOf(primaryTab);
            if (Boolean.TRUE.equals(mIsPrimaryPinned)) {
                primaryTabIndexInSelection = mPinnedTabs.indexOf(mPrimaryInteractingStripTab);
            } else {
                primaryTabIndexInSelection = mUnpinnedTabs.indexOf(mPrimaryInteractingStripTab);
            }

            int targetGatherIndex = primaryTabModelIndex - primaryTabIndexInSelection;
            int firstUnpinnedIndex = mModel.findFirstNonPinnedTabIndex();
            for (int i = 0; i <= selectedTabs.size() - 1; i++) {
                Tab tab = selectedTabs.get(i);
                if (tab.getIsPinned() == Boolean.TRUE.equals(mIsPrimaryPinned)) {
                    // Gather as normal if has the same pin state.
                    int currentTabModelIndex = mModel.indexOf(tab);
                    if (currentTabModelIndex > targetGatherIndex) {
                        targetGatherIndex++;
                    } else if (currentTabModelIndex == targetGatherIndex) {
                        continue;
                    }
                    mModel.moveTab(tab.getId(), targetGatherIndex);
                } else {
                    // Tabs whose pin state differs from the primary can move only to the last
                    // pinned or first unpinned slot, any remaining distance is carried by
                    // `offsetX`.
                    if (tab.getIsPinned()) {
                        mModel.moveTab(tab.getId(), mModel.findFirstNonPinnedTabIndex() - 1);
                    } else {
                        mModel.moveTab(tab.getId(), firstUnpinnedIndex++);
                    }
                }
            }
        }
        setOffsetXForNonReorderedPartition(Boolean.TRUE.equals(mIsPrimaryPinned));
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
        if (!Boolean.TRUE.equals(mHasMixedPinState)) return;

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
        Tab primaryTab = mModel.getTabById(mPrimaryInteractingStripTab.getTabId());
        if (primaryTab == null) return false;

        StripLayoutTab reorderEdgeTab =
                towardEnd ? tabsToReorder.get(tabsToReorder.size() - 1) : tabsToReorder.get(0);
        Tab edgeTab = mModel.getTabById(reorderEdgeTab.getTabId());
        if (reorderEdgeTab == null || edgeTab == null) return false;
        int edgeTabIndexInStrip =
                StripLayoutUtils.findIndexForTab(stripTabs, reorderEdgeTab.getTabId());

        int adjTabIndex = towardEnd ? edgeTabIndexInStrip + 1 : edgeTabIndexInStrip - 1;
        Tab adjTab = mModel.getTabAt(adjTabIndex);

        // If pinned state is different, then adjTab is ineligible for reordering.
        if (adjTab != null && adjTab.getIsPinned() != reorderEdgeTab.getIsPinned()) adjTab = null;

        boolean isInGroup = mTabGroupModelFilter.isTabInTabGroup(edgeTab);
        boolean mayDragInOrOutOfGroup =
                adjTab == null
                        ? isInGroup
                        : StripLayoutUtils.notRelatedAndEitherTabInGroup(
                                mTabGroupModelFilter, edgeTab, adjTab);

        // Return early if not reordering or removing from group.
        if (adjTab != null && mInteractingTabIds.contains(adjTab.getId())) return false;
        if (adjTab == null && !isInGroup) return false;

        // Not interacting with tab groups.
        if (!mayDragInOrOutOfGroup) {
            if (adjTab == null
                    || Math.abs(offset)
                            <= getTabSwapThreshold(/* isPinned= */ adjTab.getIsPinned())) {
                return false;
            }
            moveAdjacentTabPastBlock(adjTab, towardEnd);
            var adjStripLayoutTab = StripLayoutUtils.findTabById(stripTabs, adjTab.getId());
            animateViewSliding(assumeNonNull(adjStripLayoutTab));
            return true;
        }

        // Maybe drag out of group.
        if (isInGroup) {
            StripLayoutGroupTitle interactingGroupTitle =
                    StripLayoutUtils.findGroupTitle(groupTitles, edgeTab.getTabGroupId());
            assumeNonNull(interactingGroupTitle);
            float threshold = getDragOutThreshold(interactingGroupTitle, towardEnd);
            if (Math.abs(offset) <= threshold) return false;
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

        StripLayoutGroupTitle interactingGroupTitle =
                StripLayoutUtils.findGroupTitle(groupTitles, assumeNonNull(adjTab).getTabGroupId());

        if (interactingGroupTitle == null) return false;

        if (interactingGroupTitle.isCollapsed()) {
            // Maybe drag past collapsed group.
            float threshold = getGroupSwapThreshold(interactingGroupTitle);
            if (Math.abs(offset) <= threshold) return false;

            moveAdjacentGroupPastBlock(interactingGroupTitle, adjTab.getIsPinned(), towardEnd);
            animateViewSliding(interactingGroupTitle);
            return true;
        } else {
            // Maybe merge to group.
            if (Math.abs(offset) <= getDragInThreshold()) return false;
            mergeBlockIntoGroup(adjTab, interactingGroupTitle, towardEnd);
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
        Tab firstTabInAdjGroup =
                mTabGroupModelFilter.getTabsInGroup(adjGroupTitle.getTabGroupId()).get(0);
        mTabGroupModelFilter.moveRelatedTabs(firstTabInAdjGroup.getId(), destinationIndex);
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
        int destinationIndex = TabModel.INVALID_TAB_INDEX;
        List<StripLayoutTab> block = isPinned ? mPinnedTabs : mUnpinnedTabs;
        if (block.isEmpty()) {
            return destinationIndex;
        }
        if (towardEnd) {
            // The block is moving toward the end, so move the adjacent item to the start of the
            // block.
            StripLayoutTab firstTabInBlock = block.get(0);
            destinationIndex = mModel.indexOf(mModel.getTabById(firstTabInBlock.getTabId()));
        } else {
            // The block is moving toward the start, so move the adjacent item to the end of the
            // block.
            StripLayoutTab lastTabInBlock = block.get(block.size() - 1);
            destinationIndex = mModel.indexOf(mModel.getTabById(lastTabInBlock.getTabId()));
        }
        return destinationIndex;
    }

    /**
     * Merges all tabs in the interacting block into an adjacent, expanded tab group.
     *
     * @param adjTab The adjacent tab, which is part of the destination group.
     * @param adjTitle The title view of the destination group.
     * @param towardEnd True if the drag is toward the end of the strip (RTL: left, LTR: right),
     *     which determines if the block is merged to the front or back of the group.
     */
    private void mergeBlockIntoGroup(
            Tab adjTab, StripLayoutGroupTitle adjTitle, boolean towardEnd) {
        RecordUserAction.record("MobileToolbarReorderTab.TabsAddedToGroup");
        mTabGroupModelFilter.mergeListOfTabsToGroup(
                convertStripTabsToTabs(mUnpinnedTabs),
                adjTab,
                /* indexInGroup= */ towardEnd ? 0 : null,
                /* notify= */ MergeNotificationType.DONT_NOTIFY);
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
            if (mTabGroupModelFilter.isTabInTabGroup(tab)) tabsToUngroup.add(tab);
        }
        mTabGroupModelFilter
                .getTabUngrouper()
                .ungroupTabs(tabsToUngroup, /* trailing= */ false, /* allowDialog= */ false, null);
    }

    public void clearReorderStateForTesting() {
        clearReorderState();
    }
}

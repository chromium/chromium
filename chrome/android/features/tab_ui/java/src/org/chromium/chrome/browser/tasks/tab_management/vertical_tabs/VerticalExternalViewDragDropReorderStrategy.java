// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.graphics.Rect;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.MathUtils;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.dragdrop.ChromeTabDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabGroupDropDataAndroid;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabDragHandlerBase;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Objects;
import java.util.function.Supplier;

/**
 * Strategy that computes drop target position, geometry snapping, and target model indexing for
 * non-originating vertical tabstrip drag-and-drop operations.
 */
@NullMarked
public class VerticalExternalViewDragDropReorderStrategy {

    /** Model containing computed drop target information and geometry coordinates. */
    public static class DropTargetResult {
        @IntDef({TargetType.NONE, TargetType.MAIN_LIST, TargetType.PINNED_GRID})
        @Retention(RetentionPolicy.SOURCE)
        public @interface TargetType {
            int NONE = 0;
            int MAIN_LIST = 1;
            int PINNED_GRID = 2;
        }

        public final @TargetType int targetType;
        public final int destTabIndex;
        public final int destGroupTabId;
        public final boolean isPinned;
        public final boolean isZeroPinnedState;
        public final boolean isZeroNormalTabsState;
        public final RecyclerView.@Nullable ViewHolder targetViewHolder;
        public final int adapterPosition;
        public final boolean insertBefore;
        public final boolean isGroupTopOrBottomBoundary;
        public final Rect anchorBounds;

        public DropTargetResult(
                @TargetType int targetType,
                int destTabIndex,
                int destGroupTabId,
                boolean isPinned,
                boolean isZeroPinnedState,
                boolean isZeroNormalTabsState,
                RecyclerView.@Nullable ViewHolder targetViewHolder,
                int adapterPosition,
                boolean insertBefore,
                boolean isGroupTopOrBottomBoundary,
                Rect anchorBounds) {
            this.targetType = targetType;
            this.destTabIndex = destTabIndex;
            this.destGroupTabId = destGroupTabId;
            this.isPinned = isPinned;
            this.isZeroPinnedState = isZeroPinnedState;
            this.isZeroNormalTabsState = isZeroNormalTabsState;
            this.targetViewHolder = targetViewHolder;
            this.adapterPosition = adapterPosition;
            this.insertBefore = insertBefore;
            this.isGroupTopOrBottomBoundary = isGroupTopOrBottomBoundary;
            this.anchorBounds = anchorBounds;
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (this == obj) return true;
            if (!(obj instanceof DropTargetResult that)) return false;
            return targetType == that.targetType
                    && destTabIndex == that.destTabIndex
                    && destGroupTabId == that.destGroupTabId
                    && isPinned == that.isPinned
                    && isZeroPinnedState == that.isZeroPinnedState
                    && isZeroNormalTabsState == that.isZeroNormalTabsState
                    && adapterPosition == that.adapterPosition
                    && insertBefore == that.insertBefore
                    && isGroupTopOrBottomBoundary == that.isGroupTopOrBottomBoundary
                    && Objects.equals(targetViewHolder, that.targetViewHolder)
                    && Objects.equals(anchorBounds, that.anchorBounds);
        }

        @Override
        public int hashCode() {
            return Objects.hash(
                    targetType,
                    destTabIndex,
                    destGroupTabId,
                    isPinned,
                    isZeroPinnedState,
                    isZeroNormalTabsState,
                    targetViewHolder,
                    adapterPosition,
                    insertBefore,
                    isGroupTopOrBottomBoundary,
                    anchorBounds);
        }

        @Override
        public String toString() {
            return "DropTargetResult{"
                    + "targetType="
                    + targetType
                    + ", destTabIndex="
                    + destTabIndex
                    + ", destGroupTabId="
                    + destGroupTabId
                    + ", isPinned="
                    + isPinned
                    + ", isZeroPinnedState="
                    + isZeroPinnedState
                    + ", isZeroNormalTabsState="
                    + isZeroNormalTabsState
                    + ", targetViewHolder="
                    + targetViewHolder
                    + ", adapterPosition="
                    + adapterPosition
                    + ", insertBefore="
                    + insertBefore
                    + ", isGroupTopOrBottomBoundary="
                    + isGroupTopOrBottomBoundary
                    + ", anchorBounds="
                    + anchorBounds
                    + '}';
        }
    }

    private final Supplier<@Nullable TabModel> mTabModelSupplier;
    private final TabListModel mModelList;
    private final RecyclerView mRecyclerView;
    private final @Nullable RecyclerView mPinnedTabsRecyclerView;

    private final int[] mTempSrcLoc = new int[2];
    private final int[] mTempDestLoc = new int[2];
    private final float[] mTempRvCoords = new float[2];

    private @Nullable DropTargetResult mLastDropTargetResult;

    /**
     * @param tabModelSupplier Supplier for the destination window's current {@link TabModel}.
     * @param modelList The {@link TabListModel} for regular tabs.
     * @param recyclerView The main vertical tab list {@link RecyclerView}.
     * @param pinnedTabsRecyclerView The pinned tabs grid {@link RecyclerView}, if available.
     */
    public VerticalExternalViewDragDropReorderStrategy(
            Supplier<@Nullable TabModel> tabModelSupplier,
            TabListModel modelList,
            RecyclerView recyclerView,
            @Nullable RecyclerView pinnedTabsRecyclerView) {
        mTabModelSupplier = tabModelSupplier;
        mModelList = modelList;
        mRecyclerView = recyclerView;
        mPinnedTabsRecyclerView = pinnedTabsRecyclerView;
    }

    /** Clears the current drop target result. */
    public void clear() {
        mLastDropTargetResult = null;
    }

    /** Returns the last computed {@link DropTargetResult}, or null if cleared or invalid. */
    public @Nullable DropTargetResult getLastDropTargetResult() {
        return mLastDropTargetResult;
    }

    /**
     * Calculates the drop target location based on the hovered view and cursor coordinates.
     *
     * @param targetView The view receiving the drag event.
     * @param xPx X coordinate relative to {@code targetView}.
     * @param yPx Y coordinate relative to {@code targetView}.
     * @return The computed {@link DropTargetResult}, or null if the drop is invalid or rejected.
     */
    public @Nullable DropTargetResult calculateDropTarget(View targetView, float xPx, float yPx) {
        boolean isGroupDrag = false;
        boolean isPinnedDrag = false;

        // Query global drag state to determine dragged item type.
        DragDropGlobalState globalState = TabDragHandlerBase.getDragDropGlobalState(null);
        if (globalState != null) {
            Object dropData = globalState.getData();
            if (dropData instanceof ChromeTabGroupDropDataAndroid) {
                isGroupDrag = true;
            } else if (dropData instanceof ChromeTabDropDataAndroid tabDropData) {
                isPinnedDrag = tabDropData.tab != null && tabDropData.tab.getIsPinned();
            }
        }

        return calculateDropTarget(targetView, xPx, yPx, isGroupDrag, isPinnedDrag);
    }

    /**
     * Calculates the drop target location given explicit drag type flags.
     *
     * @param targetView The view receiving the drag event.
     * @param xPx X coordinate relative to {@code targetView}.
     * @param yPx Y coordinate relative to {@code targetView}.
     * @param isGroupDrag Whether a tab group is being dragged.
     * @param isPinnedDrag Whether a pinned tab is being dragged.
     * @return The computed {@link DropTargetResult}, or null if rejected.
     */
    @VisibleForTesting
    public @Nullable DropTargetResult calculateDropTarget(
            View targetView, float xPx, float yPx, boolean isGroupDrag, boolean isPinnedDrag) {
        TabModel tabModel = mTabModelSupplier.get();
        if (tabModel == null) {
            mLastDropTargetResult = null;
            return null;
        }

        int pinnedCount = tabModel.getPinnedTabsCount();
        boolean isOverPinnedGrid = isTargetViewInPinnedGrid(targetView);

        DropTargetResult result = null;
        if (isPinnedDrag) {
            if (pinnedCount == 0) {
                // Zero-pinned state in destination window:
                result = createZeroPinnedDropTarget();
            } else if (!isOverPinnedGrid) {
                // Pinned tab dragged over regular list when pinned tabs exist -> reject
                result = null;
            } else {
                // Pinned tab dragged over pinned grid
                mapCoordinatesToView(targetView, mPinnedTabsRecyclerView, xPx, yPx, mTempRvCoords);
                result =
                        calculatePinnedGridDropTarget(mTempRvCoords[0], mTempRvCoords[1], tabModel);
            }
        } else {
            // Regular tab or tab group drag
            if (isOverPinnedGrid) {
                if (isZeroNormalTabsState(tabModel)) {
                    result = createEmptyMainListDropTarget(tabModel);
                } else {
                    // Regular tab or tab group dragged over pinned grid -> reject
                    result = null;
                }
            } else {
                mapCoordinatesToView(targetView, mRecyclerView, xPx, yPx, mTempRvCoords);
                if (isGroupDrag) {
                    result =
                            calculateGroupDragMainListDropTarget(
                                    mTempRvCoords[0], mTempRvCoords[1], tabModel);
                } else {
                    result =
                            calculateSingleTabMainListDropTarget(
                                    mTempRvCoords[0], mTempRvCoords[1], tabModel);
                }
            }
        }

        mLastDropTargetResult = result;
        return result;
    }

    private boolean isTargetViewInPinnedGrid(View targetView) {
        if (mPinnedTabsRecyclerView == null) return false;
        if (targetView == mPinnedTabsRecyclerView) return true;
        View parent = targetView;
        while (parent != null && parent.getParent() instanceof View parentView) {
            if (parentView == mPinnedTabsRecyclerView) return true;
            parent = parentView;
        }
        return false;
    }

    private void mapCoordinatesToView(
            View sourceView, @Nullable View destView, float srcX, float srcY, float[] outCoords) {
        if (destView == null || sourceView == destView) {
            outCoords[0] = srcX;
            outCoords[1] = srcY;
            return;
        }
        sourceView.getLocationOnScreen(mTempSrcLoc);
        destView.getLocationOnScreen(mTempDestLoc);
        outCoords[0] = srcX + mTempSrcLoc[0] - mTempDestLoc[0];
        outCoords[1] = srcY + mTempSrcLoc[1] - mTempDestLoc[1];
    }

    private static float getViewCenterX(View view) {
        return view.getLeft() + view.getWidth() / 2.0f;
    }

    private static float getViewCenterY(View view) {
        return view.getTop() + view.getHeight() / 2.0f;
    }

    private int getAdapterPosition(
            RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder, View child) {
        int pos = viewHolder.getBindingAdapterPosition();
        if (pos == RecyclerView.NO_POSITION) {
            pos = recyclerView.getChildAdapterPosition(child);
        }
        if (pos == RecyclerView.NO_POSITION) {
            pos = recyclerView.getChildLayoutPosition(child);
        }
        if (pos == RecyclerView.NO_POSITION
                && viewHolder instanceof SimpleRecyclerViewAdapter.ViewHolder simpleVh
                && simpleVh.model != null) {
            for (int i = 0; i < mModelList.size(); i++) {
                if (mModelList.get(i).model == simpleVh.model) {
                    return i;
                }
            }
        }
        return pos;
    }

    private static class TabGroupRange {
        public final int firstIndex;
        public final int lastIndex;
        public final int representativeTabId;

        TabGroupRange(int firstIndex, int lastIndex, int representativeTabId) {
            this.firstIndex = firstIndex;
            this.lastIndex = lastIndex;
            this.representativeTabId = representativeTabId;
        }
    }

    private static TabGroupRange getTabGroupRange(
            TabModel tabModel,
            @Nullable Token groupId,
            int fallbackTabId,
            int firstNonPinnedIndex) {
        if (groupId != null) {
            List<Tab> groupTabs = tabModel.getTabsInGroup(groupId);
            if (!groupTabs.isEmpty()) {
                int firstIndex = TabGroupUtils.getFirstTabModelIndexForList(tabModel, groupTabs);
                int lastIndex = TabGroupUtils.getLastTabModelIndexForList(tabModel, groupTabs);
                int repTabId = groupTabs.get(0).getId();
                return new TabGroupRange(firstIndex, lastIndex, repTabId);
            }
        }

        int repTabId =
                fallbackTabId != Tab.INVALID_TAB_ID ? fallbackTabId : TabList.INVALID_TAB_INDEX;
        return new TabGroupRange(firstNonPinnedIndex, firstNonPinnedIndex, repTabId);
    }

    private static @Nullable Token getGroupId(PropertyModel model) {
        if (TabProperties.isTabGroupHeader(model)) {
            return model.get(TabProperties.TAB_GROUP_HEADER_ID);
        } else if (TabProperties.isTabInGroup(model)) {
            return model.get(TabProperties.TAB_GROUP_ID);
        }
        return null;
    }

    private static Rect getViewBounds(View view) {
        return new Rect(view.getLeft(), view.getTop(), view.getRight(), view.getBottom());
    }

    private static Rect createZeroHeightBounds(View view) {
        return new Rect(0, 0, view.getWidth(), 0);
    }

    private static boolean isBeforeHorizontalCenter(float localX, View view, boolean isRtl) {
        float childCenterX = getViewCenterX(view);
        return isRtl ? localX > childCenterX : localX < childCenterX;
    }

    private View findClosestChildInGrid(float localX, float localY, int childCount) {
        assert mPinnedTabsRecyclerView != null && childCount > 0;
        double minDistanceSq = Double.MAX_VALUE;
        View closestChild = mPinnedTabsRecyclerView.getChildAt(0);
        for (int i = 0; i < childCount; i++) {
            View cv = mPinnedTabsRecyclerView.getChildAt(i);
            float dx = localX - getViewCenterX(cv);
            float dy = localY - getViewCenterY(cv);
            double distSq = dx * dx + dy * dy;
            if (distSq < minDistanceSq) {
                minDistanceSq = distSq;
                closestChild = cv;
            }
        }
        return closestChild;
    }

    /**
     * Defensive fallback to resolve an approximate TabModel index when the hovered item's tab
     * cannot be found in {@code tabModel} (e.g. during a mid-drag tab closure/race condition or in
     * unit test environments).
     */
    private int computeFallbackModelIndex(
            TabModel tabModel, int adapterPos, int firstNonPinnedIndex) {
        // 1. Search backwards for the closest preceding item resolvable in tabModel.
        for (int i = adapterPos - 1; i >= 0; i--) {
            if (!mModelList.isValidIndex(i)) continue;
            PropertyModel model = mModelList.get(i).model;
            Token groupId = getGroupId(model);
            if (groupId != null) {
                List<Tab> groupTabs = tabModel.getTabsInGroup(groupId);
                if (!groupTabs.isEmpty()) {
                    return TabGroupUtils.getLastTabModelIndexForList(tabModel, groupTabs) + 1;
                }
            } else {
                int tabId = TabProperties.getTabId(model);
                Tab tab = tabModel.getTabById(tabId);
                if (tab != null) {
                    int idx = tabModel.indexOf(tab);
                    if (idx != TabModel.INVALID_TAB_INDEX) {
                        return idx + 1;
                    }
                }
            }
        }

        // 2. Search forwards for the closest following item resolvable in tabModel.
        for (int i = adapterPos + 1; i < mModelList.size(); i++) {
            if (!mModelList.isValidIndex(i)) continue;
            PropertyModel model = mModelList.get(i).model;
            Token groupId = getGroupId(model);
            if (groupId != null) {
                List<Tab> groupTabs = tabModel.getTabsInGroup(groupId);
                if (!groupTabs.isEmpty()) {
                    return TabGroupUtils.getFirstTabModelIndexForList(tabModel, groupTabs);
                }
            } else {
                int tabId = TabProperties.getTabId(model);
                Tab tab = tabModel.getTabById(tabId);
                if (tab != null) {
                    int idx = tabModel.indexOf(tab);
                    if (idx != TabModel.INVALID_TAB_INDEX) {
                        return idx;
                    }
                }
            }
        }

        // 3. If no surrounding tabs are resolvable, default to first non-pinned index.
        return firstNonPinnedIndex + adapterPos;
    }

    private DropTargetResult calculateStandaloneTabDropTarget(
            TabModel tabModel,
            RecyclerView.ViewHolder vh,
            int adapterPos,
            int targetTabId,
            boolean isTopHalf,
            Rect anchorBounds,
            int firstNonPinnedIndex,
            int tabCount) {
        Tab targetTab = tabModel.getTabById(targetTabId);
        int modelIndex =
                targetTab != null ? tabModel.indexOf(targetTab) : TabModel.INVALID_TAB_INDEX;
        if (modelIndex == TabModel.INVALID_TAB_INDEX) {
            modelIndex = computeFallbackModelIndex(tabModel, adapterPos, firstNonPinnedIndex);
        }

        int destTabIndex = isTopHalf ? modelIndex : modelIndex + 1;
        destTabIndex = MathUtils.clamp(destTabIndex, firstNonPinnedIndex, tabCount);

        return new DropTargetResult(
                DropTargetResult.TargetType.MAIN_LIST,
                destTabIndex,
                /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                /* isPinned= */ false,
                /* isZeroPinnedState= */ false,
                /* isZeroNormalTabsState= */ false,
                vh,
                adapterPos,
                /* insertBefore= */ isTopHalf,
                /* isGroupTopOrBottomBoundary= */ false,
                anchorBounds);
    }

    private static boolean isZeroNormalTabsState(TabModel tabModel) {
        return tabModel.getPinnedTabsCount() > 0
                && tabModel.getPinnedTabsCount() == tabModel.getCount();
    }

    private DropTargetResult createZeroPinnedDropTarget() {
        return new DropTargetResult(
                DropTargetResult.TargetType.MAIN_LIST,
                /* destTabIndex= */ 0,
                /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                /* isPinned= */ true,
                /* isZeroPinnedState= */ true,
                /* isZeroNormalTabsState= */ false,
                /* targetViewHolder= */ null,
                /* adapterPosition= */ 0,
                /* insertBefore= */ true,
                /* isGroupTopOrBottomBoundary= */ false,
                createZeroHeightBounds(mRecyclerView));
    }

    private DropTargetResult createEmptyMainListDropTarget(TabModel tabModel) {
        int firstNonPinnedIndex = tabModel.findFirstNonPinnedTabIndex();
        return new DropTargetResult(
                DropTargetResult.TargetType.MAIN_LIST,
                /* destTabIndex= */ firstNonPinnedIndex,
                /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                /* isPinned= */ false,
                /* isZeroPinnedState= */ false,
                /* isZeroNormalTabsState= */ isZeroNormalTabsState(tabModel),
                /* targetViewHolder= */ null,
                /* adapterPosition= */ 0,
                /* insertBefore= */ true,
                /* isGroupTopOrBottomBoundary= */ false,
                createZeroHeightBounds(mRecyclerView));
    }

    private View findVerticalChildUnderOrClosest(
            RecyclerView recyclerView, float localX, float localY) {
        View child = recyclerView.findChildViewUnder(localX, localY);
        if (child != null) return child;

        int childCount = recyclerView.getChildCount();
        assert childCount > 0;
        View firstChild = recyclerView.getChildAt(0);
        View lastChild = recyclerView.getChildAt(childCount - 1);
        if (localY < firstChild.getTop()) {
            return firstChild;
        } else if (localY > lastChild.getBottom()) {
            return lastChild;
        }

        View closestChild = firstChild;
        int minDiff = Integer.MAX_VALUE;
        for (int i = 0; i < childCount; i++) {
            View cv = recyclerView.getChildAt(i);
            int cy = (int) getViewCenterY(cv);
            int diff = Math.abs((int) localY - cy);
            if (diff < minDiff) {
                minDiff = diff;
                closestChild = cv;
            }
        }
        return closestChild;
    }

    private @Nullable DropTargetResult calculatePinnedGridDropTarget(
            float localX, float localY, TabModel tabModel) {
        if (mPinnedTabsRecyclerView == null) return null;

        int pinnedCount = tabModel.getPinnedTabsCount();
        if (pinnedCount == 0) {
            return createZeroPinnedDropTarget();
        }

        int childCount = mPinnedTabsRecyclerView.getChildCount();
        if (childCount == 0) {
            return new DropTargetResult(
                    DropTargetResult.TargetType.PINNED_GRID,
                    /* destTabIndex= */ 0,
                    /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                    /* isPinned= */ true,
                    /* isZeroPinnedState= */ false,
                    /* isZeroNormalTabsState= */ false,
                    /* targetViewHolder= */ null,
                    /* adapterPosition= */ 0,
                    /* insertBefore= */ true,
                    /* isGroupTopOrBottomBoundary= */ false,
                    createZeroHeightBounds(mPinnedTabsRecyclerView));
        }

        View child = mPinnedTabsRecyclerView.findChildViewUnder(localX, localY);
        RecyclerView.ViewHolder vh;
        int adapterPos;
        boolean insertBefore;
        View anchorView;

        boolean isRtl = LocalizationUtils.isLayoutRtl();

        if (child != null) {
            vh = mPinnedTabsRecyclerView.getChildViewHolder(child);
            adapterPos =
                    MathUtils.clamp(
                            getAdapterPosition(mPinnedTabsRecyclerView, vh, child),
                            0,
                            pinnedCount - 1);
            anchorView = child;
            insertBefore = isBeforeHorizontalCenter(localX, child, isRtl);
        } else {
            View firstChild = mPinnedTabsRecyclerView.getChildAt(0);
            View lastChild = mPinnedTabsRecyclerView.getChildAt(childCount - 1);

            if (localY < firstChild.getTop()) {
                vh = mPinnedTabsRecyclerView.getChildViewHolder(firstChild);
                adapterPos = 0;
                insertBefore = true;
                anchorView = firstChild;
            } else if (localY > lastChild.getBottom()) {
                vh = mPinnedTabsRecyclerView.getChildViewHolder(lastChild);
                adapterPos = pinnedCount - 1;
                insertBefore = false;
                anchorView = lastChild;
            } else {
                View closestChild = findClosestChildInGrid(localX, localY, childCount);
                vh = mPinnedTabsRecyclerView.getChildViewHolder(closestChild);
                adapterPos =
                        MathUtils.clamp(
                                getAdapterPosition(mPinnedTabsRecyclerView, vh, closestChild),
                                0,
                                pinnedCount - 1);
                anchorView = closestChild;
                insertBefore = isBeforeHorizontalCenter(localX, closestChild, isRtl);
            }
        }

        int destTabIndex = insertBefore ? adapterPos : adapterPos + 1;
        destTabIndex = MathUtils.clamp(destTabIndex, 0, pinnedCount);

        return new DropTargetResult(
                DropTargetResult.TargetType.PINNED_GRID,
                destTabIndex,
                /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                /* isPinned= */ true,
                /* isZeroPinnedState= */ false,
                /* isZeroNormalTabsState= */ false,
                vh,
                adapterPos,
                insertBefore,
                /* isGroupTopOrBottomBoundary= */ false,
                getViewBounds(anchorView));
    }

    private @Nullable DropTargetResult calculateSingleTabMainListDropTarget(
            float localX, float localY, TabModel tabModel) {
        if (mRecyclerView.getChildCount() == 0
                || mModelList.size() == 0
                || isZeroNormalTabsState(tabModel)) {
            return createEmptyMainListDropTarget(tabModel);
        }

        View child = findVerticalChildUnderOrClosest(mRecyclerView, localX, localY);
        RecyclerView.ViewHolder vh = mRecyclerView.getChildViewHolder(child);
        int adapterPos =
                MathUtils.clamp(
                        getAdapterPosition(mRecyclerView, vh, child), 0, mModelList.size() - 1);

        PropertyModel model = mModelList.get(adapterPos).model;
        int targetTabId = TabProperties.getTabId(model);
        boolean isGroupHeader = TabProperties.isTabGroupHeader(model);
        boolean isCollapsed = isGroupHeader && TabProperties.isTabGroupCollapsed(model);
        boolean isChildTab = TabProperties.isTabInGroup(model);
        Token groupId = getGroupId(model);

        float childCenterY = getViewCenterY(child);
        boolean isTopHalf = localY < childCenterY;
        Rect anchorBounds = getViewBounds(child);

        int firstNonPinnedIndex = tabModel.findFirstNonPinnedTabIndex();
        int tabCount = tabModel.getCount();

        // Sub-rule 1: Over Collapsed Tab Group (Atomic rule: NEVER merges into collapsed group)
        if (isGroupHeader && isCollapsed) {
            TabGroupRange range =
                    getTabGroupRange(tabModel, groupId, targetTabId, firstNonPinnedIndex);
            boolean insertBefore = isTopHalf;
            int destTabIndex = insertBefore ? range.firstIndex : range.lastIndex + 1;
            destTabIndex = MathUtils.clamp(destTabIndex, firstNonPinnedIndex, tabCount);

            return new DropTargetResult(
                    DropTargetResult.TargetType.MAIN_LIST,
                    destTabIndex,
                    /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                    /* isPinned= */ false,
                    /* isZeroPinnedState= */ false,
                    /* isZeroNormalTabsState= */ false,
                    vh,
                    adapterPos,
                    insertBefore,
                    /* isGroupTopOrBottomBoundary= */ true,
                    anchorBounds);
        }

        // Sub-rule 2: Over Expanded Tab Group
        // 2a. Over Group Header -> top half: standalone above group; bottom half: 1st child inside
        // group
        if (isGroupHeader && !isCollapsed) {
            TabGroupRange range =
                    getTabGroupRange(tabModel, groupId, targetTabId, firstNonPinnedIndex);
            int destTabIndex = MathUtils.clamp(range.firstIndex, firstNonPinnedIndex, tabCount);
            boolean insertBefore = isTopHalf;
            int destGroupTabId =
                    insertBefore ? TabList.INVALID_TAB_INDEX : range.representativeTabId;
            boolean isGroupTopOrBottomBoundary = insertBefore;

            return new DropTargetResult(
                    DropTargetResult.TargetType.MAIN_LIST,
                    destTabIndex,
                    destGroupTabId,
                    /* isPinned= */ false,
                    /* isZeroPinnedState= */ false,
                    /* isZeroNormalTabsState= */ false,
                    vh,
                    adapterPos,
                    insertBefore,
                    isGroupTopOrBottomBoundary,
                    anchorBounds);
        }

        // 2b. Over Child Tab in Expanded Group -> top half / bottom half sets insertion slot within
        // group, or standalone slot below group if past lower threshold on last child
        if (isChildTab) {
            TabGroupRange range =
                    getTabGroupRange(tabModel, groupId, targetTabId, firstNonPinnedIndex);
            Tab targetTab = tabModel.getTabById(targetTabId);
            int modelIndex =
                    targetTab != null ? tabModel.indexOf(targetTab) : TabModel.INVALID_TAB_INDEX;
            if (modelIndex == TabModel.INVALID_TAB_INDEX) {
                modelIndex = range.firstIndex;
            }

            boolean insertBefore = isTopHalf;
            boolean isLastChildInGroup = (modelIndex == range.lastIndex);
            boolean isBelowGroupThreshold =
                    isLastChildInGroup
                            && !insertBefore
                            && (localY >= child.getTop() + 0.75f * child.getHeight()
                                    || localY > child.getBottom());

            int destTabIndex;
            int destGroupTabId;
            boolean isGroupTopOrBottomBoundary;
            if (isBelowGroupThreshold) {
                destTabIndex = MathUtils.clamp(range.lastIndex + 1, firstNonPinnedIndex, tabCount);
                destGroupTabId = TabList.INVALID_TAB_INDEX;
                isGroupTopOrBottomBoundary = true;
            } else {
                destTabIndex = insertBefore ? modelIndex : modelIndex + 1;
                destTabIndex = MathUtils.clamp(destTabIndex, firstNonPinnedIndex, tabCount);
                destGroupTabId = range.representativeTabId;
                isGroupTopOrBottomBoundary = false;
            }

            return new DropTargetResult(
                    DropTargetResult.TargetType.MAIN_LIST,
                    destTabIndex,
                    destGroupTabId,
                    /* isPinned= */ false,
                    /* isZeroPinnedState= */ false,
                    /* isZeroNormalTabsState= */ false,
                    vh,
                    adapterPos,
                    insertBefore,
                    isGroupTopOrBottomBoundary,
                    anchorBounds);
        }

        // Sub-rule 3: Over Standalone Tab
        return calculateStandaloneTabDropTarget(
                tabModel,
                vh,
                adapterPos,
                targetTabId,
                isTopHalf,
                anchorBounds,
                firstNonPinnedIndex,
                tabCount);
    }

    private @Nullable DropTargetResult calculateGroupDragMainListDropTarget(
            float localX, float localY, TabModel tabModel) {
        if (mRecyclerView.getChildCount() == 0
                || mModelList.size() == 0
                || isZeroNormalTabsState(tabModel)) {
            return createEmptyMainListDropTarget(tabModel);
        }

        View child = findVerticalChildUnderOrClosest(mRecyclerView, localX, localY);
        RecyclerView.ViewHolder vh = mRecyclerView.getChildViewHolder(child);
        int adapterPos =
                MathUtils.clamp(
                        getAdapterPosition(mRecyclerView, vh, child), 0, mModelList.size() - 1);

        PropertyModel model = mModelList.get(adapterPos).model;
        int targetTabId = TabProperties.getTabId(model);
        Token targetGroupId = getGroupId(model);

        int firstNonPinnedIndex = tabModel.findFirstNonPinnedTabIndex();
        int tabCount = tabModel.getCount();

        // Target is part of a Tab Group -> snap to top or bottom boundary of entire group
        if (targetGroupId != null) {
            TabGroupRange range =
                    getTabGroupRange(tabModel, targetGroupId, targetTabId, firstNonPinnedIndex);

            // Compute envelope from top of header to bottom of last visible child
            int envelopeTop = Integer.MAX_VALUE;
            int envelopeBottom = Integer.MIN_VALUE;
            RecyclerView.ViewHolder headerVh = null;
            RecyclerView.ViewHolder lastChildVh = null;
            int headerAdapterPos = -1;
            int maxChildAdapterPos = -1;

            for (int i = 0; i < mRecyclerView.getChildCount(); i++) {
                View v = mRecyclerView.getChildAt(i);
                RecyclerView.ViewHolder childVh = mRecyclerView.getChildViewHolder(v);
                int pos = getAdapterPosition(mRecyclerView, childVh, v);
                if (pos == RecyclerView.NO_POSITION || !mModelList.isValidIndex(pos)) continue;

                PropertyModel childModel = mModelList.get(pos).model;
                Token gid = getGroupId(childModel);
                if (Objects.equals(gid, targetGroupId)) {
                    if (v.getTop() < envelopeTop) {
                        envelopeTop = v.getTop();
                    }
                    if (v.getBottom() > envelopeBottom) {
                        envelopeBottom = v.getBottom();
                    }
                    if (TabProperties.isTabGroupHeader(childModel)) {
                        headerVh = childVh;
                        headerAdapterPos = pos;
                    }
                    if (pos > maxChildAdapterPos) {
                        maxChildAdapterPos = pos;
                        lastChildVh = childVh;
                    }
                }
            }

            if (envelopeTop == Integer.MAX_VALUE) {
                envelopeTop = child.getTop();
                envelopeBottom = child.getBottom();
                headerVh = vh;
                lastChildVh = vh;
            }

            if (headerVh == null) headerVh = vh;
            if (lastChildVh == null) lastChildVh = vh;

            float envelopeCenterY = (envelopeTop + envelopeBottom) / 2.0f;
            boolean isCloserToTop = localY < envelopeCenterY;

            RecyclerView.ViewHolder targetVh = isCloserToTop ? headerVh : lastChildVh;
            int targetPos = isCloserToTop ? headerAdapterPos : maxChildAdapterPos;
            int destModelIndex = isCloserToTop ? range.firstIndex : range.lastIndex + 1;
            int destTabIndex = MathUtils.clamp(destModelIndex, firstNonPinnedIndex, tabCount);

            return new DropTargetResult(
                    DropTargetResult.TargetType.MAIN_LIST,
                    destTabIndex,
                    /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                    /* isPinned= */ false,
                    /* isZeroPinnedState= */ false,
                    /* isZeroNormalTabsState= */ false,
                    targetVh,
                    targetPos >= 0 ? targetPos : adapterPos,
                    /* insertBefore= */ isCloserToTop,
                    /* isGroupTopOrBottomBoundary= */ true,
                    getViewBounds(targetVh.itemView));
        }

        // Target is Standalone Tab
        boolean isTopHalf = localY < getViewCenterY(child);
        Rect anchorBounds = getViewBounds(child);
        return calculateStandaloneTabDropTarget(
                tabModel,
                vh,
                adapterPos,
                targetTabId,
                isTopHalf,
                anchorBounds,
                firstNonPinnedIndex,
                tabCount);
    }
}

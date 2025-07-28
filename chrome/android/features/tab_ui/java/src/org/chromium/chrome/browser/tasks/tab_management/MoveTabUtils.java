// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.List;
import java.util.Objects;

/**
 * Utils for calculating the proper index for APIs that require calling {@link TabModel#moveTab(int,
 * int)} and {@link TabGroupModelFilter#moveRelatedTabs}.
 */
@NullMarked
public final class MoveTabUtils {
    /**
     * Moves the specified tab to the requested index. If the index is not a valid move operation,
     * it will calculate a valid index.
     *
     * <p>If the tab is inside a tab group and the requestedIndex is outside the tab group, it will
     * insert the tab towards the boundary of the tab group following the direction of the intended
     * move.
     *
     * <p>If it is an ungrouped tab and the requestedIndex is inside a tab group, it will be placed
     * adjacent to the entire group. This may result in a no-op if the current index of the tab is a
     * valid index.
     *
     * @param tabModel The current {@link TabModel}.
     * @param filter The current {@link TabGroupModelFilter}.
     * @param tab The {@link Tab} to move.
     * @param requestedIndex The requested index to move {@code tab} to.
     */
    public static void moveSingleTab(
            TabModel tabModel, TabGroupModelFilter filter, Tab tab, int requestedIndex) {
        int curIndex = tabModel.indexOf(tab);
        requestedIndex = Math.max(requestedIndex, TabList.INVALID_TAB_INDEX);

        // No-op if same index or requested an invalid index;
        if (curIndex == TabList.INVALID_TAB_INDEX
                || requestedIndex == TabList.INVALID_TAB_INDEX
                || curIndex == requestedIndex) {
            return;
        }

        @Nullable Token tabGroupId = tab.getTabGroupId();
        @Nullable Tab destinationTab = tabModel.getTabAt(requestedIndex);
        @Nullable Token destinationTabGroupId =
                (destinationTab != null) ? destinationTab.getTabGroupId() : null;

        final int validIndex;
        if (Objects.equals(tabGroupId, destinationTabGroupId)) {
            // Case 1: Simple move.
            // This handles moves between two ungrouped tabs, or moves within the same tab group.
            validIndex = requestedIndex;
        } else if (tabGroupId != null) {
            // Case 2: A grouped tab is attempting to leave its group.
            // This handles moves from GroupA -> GroupB and GroupA -> SingleTab.
            validIndex =
                    getClosestValidIndexInGroup(
                            tabModel, filter, tabGroupId, curIndex < requestedIndex);
        } else {
            // Case 3: An ungrouped tab is attempting to enter a group.
            // This handles moves from SingleTab -> Group.

            // This block is only reached if tabGroupId is null. The first if condition has already
            // handled the case where destinationTabGroupId is also null. Hence,
            // destinationTabGroupId is guaranteed to be non-null here.
            assumeNonNull(destinationTabGroupId);

            validIndex =
                    getClosestValidIndexAdjacentToGroup(
                            tabModel, filter, destinationTabGroupId, curIndex, requestedIndex);
        }

        // No-op if invalid index.
        if (validIndex == TabList.INVALID_TAB_INDEX || curIndex == validIndex) {
            return;
        }

        tabModel.moveTab(tab.getId(), validIndex);
    }

    /**
     * Moves the specified group to the requested index. If the index is not a valid move operation,
     * it will calculate a valid index.
     *
     * <p>{@param requestedIndex} is not the true destination index; it doesn't take into account
     * the size of the moving tab group, and it will be modified to be a valid index (see below).
     *
     * <p>If the requested index is inside the source tab group, it will be a no-op since the tab
     * group is already in that index range.
     *
     * <p>If the requested index is a single tab, it will insert the tab group in that position.
     *
     * <p>If the requestedIndex is inside a tab group, it will insert the tab group adjacent to the
     * destination group. This may result in a no-op if the source tab group is already adjacent to
     * the destination group.
     *
     * <p>"Insert" rounds away from the current location of the tab group. That is, if the tab group
     * is being "inserted" to location i, if the tab group is moving left, the tab group will be
     * inserted before the current contents of location i. If the tab group is moving right, the tab
     * group will be inserted after the current contents of location i.
     *
     * <p>Taking a concrete example, if you have tabs D E [A B C], a requested index of 1 gives
     * D [A B C] E and a requested index of 0 gives [A B C] D E. With a start state of [A B C] D E,
     * a requested index of 3 (the current index of D) will result in the state D [A B C] E. [A B C]
     * actually ends up at index 1, but that calling this method with a requested index of 1 would
     * be a no-op because [A B C] already occupies index 1. If, because we wanted to move the tab
     * group to the right of index D, we requested index 4 (current index of D plus 1), the tab
     * group would actually move to after E (D E [A B C]).
     *
     * @param tabModel The current {@link TabModel}.
     * @param filter The current {@link TabGroupModelFilter}.
     * @param tabGroupId The tab group id.
     * @param requestedIndex The requested index to move {@code tab} to.
     */
    public static void moveTabGroup(
            TabModel tabModel, TabGroupModelFilter filter, Token tabGroupId, int requestedIndex) {
        List<Tab> sourceTabsInGroup = filter.getTabsInGroup(tabGroupId);
        requestedIndex = Math.max(requestedIndex, TabList.INVALID_TAB_INDEX);
        if (sourceTabsInGroup.isEmpty() || requestedIndex == TabList.INVALID_TAB_INDEX) return;

        @Nullable Tab destinationTab = tabModel.getTabAt(requestedIndex);
        @Nullable Token destinationTabGroupId =
                (destinationTab != null) ? destinationTab.getTabGroupId() : null;

        // No-op if destination is invalid or moving to the same tab group id.
        if (destinationTab == null || tabGroupId.equals(destinationTabGroupId)) return;

        int sourceFirstIndex =
                TabGroupUtils.getFirstTabModelIndexForList(tabModel, sourceTabsInGroup);

        final int validIndex;
        final int pivotIndex;
        if (destinationTabGroupId == null) {
            // Simple move because the destination is an ungrouped tab. The requested index is a
            // valid index.
            validIndex = requestedIndex;
            // Pivot index doesn't matter when it is a valid index.
            pivotIndex = sourceFirstIndex;
        } else {
            // Find the tab group boundary closest to the requested destination to detect cases
            // where the source tab group might already be adjacent to the destination tab group.
            int sourceLastIndex =
                    TabGroupUtils.getLastTabModelIndexForList(tabModel, sourceTabsInGroup);
            boolean isMovingToHigherIndex = requestedIndex > sourceFirstIndex;
            pivotIndex = isMovingToHigherIndex ? sourceLastIndex : sourceFirstIndex;

            validIndex =
                    getClosestValidIndexAdjacentToGroup(
                            tabModel, filter, destinationTabGroupId, pivotIndex, requestedIndex);
        }

        if (validIndex == TabList.INVALID_TAB_INDEX || validIndex == pivotIndex) {
            return;
        }

        @TabId int firstTabId = sourceTabsInGroup.get(0).getId();
        filter.moveRelatedTabs(firstTabId, validIndex);
    }

    private static int getClosestValidIndexInGroup(
            TabModel tabModel,
            TabGroupModelFilter filter,
            Token tabGroupId,
            boolean isMovingToHigherIndex) {
        List<Tab> tabsInGroup = filter.getTabsInGroup(tabGroupId);
        if (isMovingToHigherIndex) {
            return TabGroupUtils.getLastTabModelIndexForList(tabModel, tabsInGroup);
        } else {
            return TabGroupUtils.getFirstTabModelIndexForList(tabModel, tabsInGroup);
        }
    }

    private static int getClosestValidIndexAdjacentToGroup(
            TabModel tabModel,
            TabGroupModelFilter filter,
            Token tabGroupId,
            int curIndex,
            int requestedIndexInsideGroup) {
        List<Tab> tabsInGroup = filter.getTabsInGroup(tabGroupId);
        assert !tabsInGroup.isEmpty();

        if (tabsInGroup.size() == 1) {
            // There is only one tab in the group, so we can just return the requested index as
            // there is no risk of breaking the contiguous property of the group.
            return requestedIndexInsideGroup;
        }

        int firstIndexOfGroup = TabGroupUtils.getFirstTabModelIndexForList(tabModel, tabsInGroup);
        int lastIndexOfGroup = TabGroupUtils.getLastTabModelIndexForList(tabModel, tabsInGroup);
        int firstDelta = requestedIndexInsideGroup - firstIndexOfGroup;
        int lastDelta = lastIndexOfGroup - requestedIndexInsideGroup;

        boolean keepInFront =
                firstDelta < lastDelta || (firstDelta == lastDelta && curIndex < firstIndexOfGroup);
        if (keepInFront) {
            // The new first index of the group will be one earlier than before if the tab is coming
            // from a lower index.
            return curIndex < firstIndexOfGroup ? firstIndexOfGroup - 1 : firstIndexOfGroup;
        } else {
            // Target the last index of the group. If the group would not be shifted forward add one
            // to ensure the tab comes after the group.
            return curIndex < lastIndexOfGroup ? lastIndexOfGroup : lastIndexOfGroup + 1;
        }
    }
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
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
     * @param curIndex The current index of the {@code tab}.
     * @param requestedIndex The requested index to move {@code tab} to.
     */
    public static void moveSingleTab(
            TabModel tabModel,
            TabGroupModelFilter filter,
            Tab tab,
            int curIndex,
            int requestedIndex) {
        // No-op if same index or requested a negative index.
        if (curIndex == requestedIndex || curIndex < 0) return;

        @Nullable Token tabGroupId = tab.getTabGroupId();
        @Nullable Tab destinationTab = tabModel.getTabAt(requestedIndex);
        @Nullable Token destinationTabGroupId =
                (destinationTab != null) ? destinationTab.getTabGroupId() : null;
        int validIndex;

        if ((tabGroupId == null && destinationTabGroupId == null)
                || Objects.equals(tabGroupId, destinationTabGroupId)) {
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
            int requestedIndex) {
        List<Tab> tabsInGroup = filter.getTabsInGroup(tabGroupId);

        assert !tabsInGroup.isEmpty();
        int firstIndexOfGroup = TabGroupUtils.getFirstTabModelIndexForList(tabModel, tabsInGroup);
        if (tabsInGroup.size() == 1) {
            // There is only one tab in the group, so we can just return the requested index as
            // there is no risk of breaking the contiguous property of the group.
            return requestedIndex;
        }

        int lastIndexOfGroup = TabGroupUtils.getLastTabModelIndexForList(tabModel, tabsInGroup);
        int firstDelta = requestedIndex - firstIndexOfGroup;
        int lastDelta = lastIndexOfGroup - requestedIndex;

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

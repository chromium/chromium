// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.Objects;

/** Helper utilities for reordering tabs and tab groups in nested tab list layout. */
@NullMarked
public class NestedTabReorderUtils {
    private NestedTabReorderUtils() {}

    /**
     * Attempts to ungroup a non-solitary child tab when moving across group boundaries or onto its
     * own group header.
     *
     * @param tabModel The active {@link TabModel}.
     * @param currentTabId The ID of the tab being moved.
     * @param currentGroupId The group ID of the tab being moved.
     * @param destGroupId The group ID of the target item.
     * @param isDestGroupHeader Whether the destination item is a tab group header.
     * @param distance The directional move distance (positive = moving down / trailing).
     * @return Whether the child tab was ungrouped.
     */
    public static boolean tryUngroupChildTab(
            TabModel tabModel,
            int currentTabId,
            @Nullable Token currentGroupId,
            @Nullable Token destGroupId,
            boolean isDestGroupHeader,
            int distance) {
        if (!Objects.equals(currentGroupId, destGroupId)
                || (isDestGroupHeader && Objects.equals(currentGroupId, destGroupId))) {
            boolean trailing = distance > 0;
            Tab currentTab = tabModel.getTabById(currentTabId);
            if (currentTab != null) {
                ungroupTab(tabModel, currentTab, trailing);
            }
            return true;
        }
        return false;
    }

    /**
     * Intercepts swaps between a standalone tab and a tab group to merge the tab into the group.
     *
     * @param tabModel The active {@link TabModel}.
     * @param currentTabId The ID of the standalone tab.
     * @param destinationTabId The ID of the destination tab.
     * @param destGroupId The group ID of the target group.
     * @param isDestGroupHeader Whether the destination item is a tab group header.
     * @param destinationModel The {@link PropertyModel} of the destination item.
     * @param distance The directional move distance (positive = dragging down).
     * @return Whether the standalone tab was merged into the group.
     */
    public static boolean tryMergeStandaloneTab(
            TabModel tabModel,
            int currentTabId,
            int destinationTabId,
            @Nullable Token destGroupId,
            boolean isDestGroupHeader,
            @Nullable PropertyModel destinationModel,
            int distance) {
        // Intercept swaps between a standalone tab and a tab group.
        if (destGroupId != null) {
            boolean isDestGroupCollapsed =
                    isDestGroupHeader
                            && destinationModel != null
                            && Boolean.TRUE.equals(
                                    destinationModel.get(TabProperties.IS_COLLAPSED));

            if (!isDestGroupCollapsed) {
                boolean isDraggingDown = distance > 0;

                Tab currentTab = tabModel.getTabById(currentTabId);
                Tab destinationTab = tabModel.getTabById(destinationTabId);

                if (currentTab != null && destinationTab != null) {
                    // Handle grouping when a standalone tab intersects any part of a group.
                    Integer indexInGroup = 0;
                    if (!isDestGroupHeader) {
                        List<Tab> destRelatedTabs = tabModel.getRelatedTabList(destinationTabId);
                        if (destRelatedTabs != null) {
                            boolean isDraggingUp = distance < 0;
                            boolean isTargetLowestTab =
                                    destRelatedTabs.get(destRelatedTabs.size() - 1).getId()
                                            == destinationTabId;
                            if (isDraggingUp && isTargetLowestTab) {
                                indexInGroup = null;
                            } else {
                                indexInGroup = destRelatedTabs.indexOf(destinationTab);
                                if (isDraggingDown) {
                                    indexInGroup++;
                                }
                            }
                        }
                    }
                    tabModel.mergeListOfTabsToGroup(
                            List.of(currentTab),
                            destinationTab,
                            indexInGroup,
                            TabGroupMergeNotificationType.NOTIFY_ALWAYS);
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * Calculates the destination index in the {@link TabModel} for moving a tab or tab group.
     *
     * @param tabModel The active {@link TabModel}.
     * @param currentTabId The ID of the tab being moved.
     * @param destinationTabId The ID of the target tab.
     * @param isGroup Whether the item being moved is a group header or solitary child.
     * @param isStandaloneTab Whether the item being moved is a standalone tab.
     * @param destGroupId The group ID of the target destination, if any.
     * @param distance The directional move distance.
     * @return The calculated destination index, or {@link TabModel#INVALID_TAB_INDEX}.
     */
    public static int calculateDestinationIndex(
            TabModel tabModel,
            int currentTabId,
            int destinationTabId,
            boolean isGroup,
            boolean isStandaloneTab,
            @Nullable Token destGroupId,
            int distance) {
        boolean isTraversingGroup = isGroup || (isStandaloneTab && destGroupId != null);
        int destinationIndex;
        if (isTraversingGroup) {
            // Tab groups should maintain the boundaries of target tab groups
            // so they do not split other groups during drags.
            List<Tab> destinationTabGroup = tabModel.getRelatedTabList(destinationTabId);
            destinationIndex =
                    distance >= 0
                            ? TabGroupUtils.getLastTabModelIndexForList(
                                    tabModel, destinationTabGroup)
                            : TabGroupUtils.getFirstTabModelIndexForList(
                                    tabModel, destinationTabGroup);
        } else {
            //  - Child tabs should reorder inside tab groups.
            //  - Standalone tabs use this logic too, but only when not intersecting with a group.
            Tab destinationTab = tabModel.getTabById(destinationTabId);
            destinationIndex =
                    destinationTab != null
                            ? tabModel.indexOf(destinationTab)
                            : TabModel.INVALID_TAB_INDEX;
        }

        if (destinationIndex == TabModel.INVALID_TAB_INDEX) return TabModel.INVALID_TAB_INDEX;

        return adjustIndexBasedOnPinning(tabModel, currentTabId, destinationIndex);
    }

    /**
     * Performs basic list reordering by updating the {@link TabModel} immediately.
     *
     * <p>- Group headers use moveRelatedTabs() to fire didMoveTabGroup(), which TabListMediator
     * observes to update top-level UI rows.
     *
     * <p>- Child tabs use moveTab() because they move within their group, firing
     * didMoveWithinGroup() which TabListMediator observes.
     *
     * <p>- Standalone tabs use moveTab() since they are single elements.
     *
     * @param tabModel The active {@link TabModel}.
     * @param currentTabId The ID of the tab being moved.
     * @param destinationIndex The target index in the tab model.
     * @param isGroup Whether the item being moved is a group header or solitary child.
     */
    public static void moveTabOrGroup(
            TabModel tabModel, int currentTabId, int destinationIndex, boolean isGroup) {
        if (isGroup) {
            tabModel.moveRelatedTabs(currentTabId, destinationIndex);
        } else {
            tabModel.moveTab(currentTabId, destinationIndex);
        }
    }

    /**
     * Reorders an item at {@code fromIndex} to {@code toIndex} within the vertical tab list.
     *
     * @param tabModel The active {@link TabModel}.
     * @param modelList The {@link TabListModel} for the vertical tab list.
     * @param fromIndex The adapter index of the item being moved.
     * @param toIndex The adapter index of the target position.
     * @return Whether the reorder operation was successfully executed.
     */
    public static boolean reorderItem(
            TabModel tabModel, TabListModel modelList, int fromIndex, int toIndex) {
        if (fromIndex < 0 || fromIndex >= modelList.size()) return false;
        if (toIndex < 0 || toIndex >= modelList.size()) return false;
        if (fromIndex == toIndex) return false;

        ListItem fromItem = modelList.get(fromIndex);
        ListItem toItem = modelList.get(toIndex);
        if (fromItem.model == null || toItem.model == null) return false;

        PropertyModel fromModel = fromItem.model;
        PropertyModel toModel = toItem.model;

        int currentTabId = getRepresentativeTabId(tabModel, fromItem);
        int destinationTabId = getRepresentativeTabId(tabModel, toItem);
        if (currentTabId == Tab.INVALID_TAB_ID || destinationTabId == Tab.INVALID_TAB_ID) {
            return false;
        }

        boolean isGroupHeader = isTabGroupHeader(fromItem);
        Token currentGroupId = getTabGroupId(fromModel);
        boolean isStandaloneTab = !isGroupHeader && currentGroupId == null;
        boolean isSolitaryChild = !isGroupHeader && isSolitaryChild(tabModel, fromModel);
        boolean isGroup = isGroupHeader || isSolitaryChild;

        Token destGroupId = getTabGroupId(toModel);
        boolean isDestGroupHeader = isTabGroupHeader(toItem);

        int distance = toIndex - fromIndex;

        if (!isStandaloneTab && !isGroup) {
            // This is a non-solitary child tab.
            if (tryUngroupChildTab(
                    tabModel,
                    currentTabId,
                    currentGroupId,
                    destGroupId,
                    isDestGroupHeader,
                    distance)) {
                return true;
            }
        }

        if (isStandaloneTab) {
            // Intercept swaps between a standalone tab and a tab group.
            if (tryMergeStandaloneTab(
                    tabModel,
                    currentTabId,
                    destinationTabId,
                    destGroupId,
                    isDestGroupHeader,
                    toModel,
                    distance)) {
                return true;
            }
        }

        int destinationIndex =
                calculateDestinationIndex(
                        tabModel,
                        currentTabId,
                        destinationTabId,
                        isGroup,
                        isStandaloneTab,
                        destGroupId,
                        distance);

        if (destinationIndex == TabModel.INVALID_TAB_INDEX) return false;

        moveTabOrGroup(tabModel, currentTabId, destinationIndex, isGroup);
        return true;
    }

    /**
     * Reorders the tab or tab group at {@code pos} in {@code modelList} in the given direction (up
     * / previous vs down / next).
     *
     * @param tabModel The active {@link TabModel}.
     * @param modelList The {@link TabListModel} for the vertical tab list.
     * @param pos The adapter position of the item to reorder.
     * @param toPrevious Whether to move the item up (previous) or down (next).
     * @return Whether the reorder operation was successfully executed.
     */
    public static boolean reorderItemInDirection(
            TabModel tabModel, TabListModel modelList, int pos, boolean toPrevious) {
        if (pos < 0 || pos >= modelList.size()) return false;
        if (toPrevious && pos == 0) return false;

        ListItem item = modelList.get(pos);
        if (item.model == null) return false;

        int currentTabId = getRepresentativeTabId(tabModel, item);
        if (currentTabId == Tab.INVALID_TAB_ID) return false;

        boolean isGroupHeader = isTabGroupHeader(item);
        boolean isSolitaryChild = isSolitaryChild(tabModel, item.model);
        boolean isGroup = isGroupHeader || isSolitaryChild;

        if (isGroup) {
            return reorderTabGroupByAnchorTabId(tabModel, currentTabId, toPrevious);
        }

        // If at the end of the list, a child tab can still move down to ungroup out of the group.
        if (!toPrevious && pos == modelList.size() - 1) {
            if (isChildTab(item)) {
                Tab currentTab = tabModel.getTabById(currentTabId);
                if (currentTab != null) {
                    ungroupTab(tabModel, currentTab, /* trailing= */ true);
                    return true;
                }
            }
            return false;
        }

        int targetPos = toPrevious ? pos - 1 : pos + 1;
        return reorderItem(tabModel, modelList, pos, targetPos);
    }

    // =============================================================================================
    // Context Menu Reorder Entry Points
    // =============================================================================================

    /**
     * Reorders an entire tab group in the given direction (up/previous or down/next).
     *
     * @param tabModel The active {@link TabModel}.
     * @param groupId The {@link Token} ID of the tab group.
     * @param toPrevious Whether to move to previous (up) or next (down).
     * @return Whether the group was successfully reordered.
     */
    public static boolean reorderTabGroup(
            @Nullable TabModel tabModel, Token groupId, boolean toPrevious) {
        if (tabModel == null) return false;
        List<Tab> tabs = tabModel.getTabsInGroup(groupId);
        if (tabs == null || tabs.isEmpty()) return false;
        return reorderTabGroupByAnchorTabId(tabModel, tabs.get(0).getId(), toPrevious);
    }

    /**
     * Reorders a single tab or pinned tab in the given direction (up/previous or down/next).
     *
     * @param tabModel The active {@link TabModel}.
     * @param pinnedTabsModelList The {@link TabListModel} containing pinned tabs.
     * @param modelList The {@link TabListModel} containing unpinned tabs.
     * @param tabId The ID of the tab being reordered.
     * @param toPrevious Whether to move to previous (up) or next (down).
     * @return Whether the tab was successfully reordered.
     */
    public static boolean reorderTabById(
            @Nullable TabModel tabModel,
            TabListModel pinnedTabsModelList,
            TabListModel modelList,
            int tabId,
            boolean toPrevious) {
        if (tabModel == null) return false;
        int pinnedIndex = pinnedTabsModelList.indexFromTabId(tabId);
        if (pinnedIndex != TabModel.INVALID_TAB_INDEX) {
            return reorderItemInDirection(tabModel, pinnedTabsModelList, pinnedIndex, toPrevious);
        }
        int index = modelList.indexFromTabId(tabId);
        if (index != TabModel.INVALID_TAB_INDEX) {
            return reorderItemInDirection(tabModel, modelList, index, toPrevious);
        }
        return false;
    }

    /** Returns the {@link Token} tab group ID from the given {@link PropertyModel}, if any. */
    public static @Nullable Token getTabGroupId(@Nullable PropertyModel model) {
        if (model == null) return null;
        Token headerId = model.get(TabProperties.TAB_GROUP_HEADER_ID);
        if (headerId != null) return headerId;
        return model.get(TabProperties.TAB_GROUP_ID);
    }

    /** Ungroups the given tab in the tab model. */
    public static void ungroupTab(TabModel tabModel, Tab tab, boolean trailing) {
        tabModel.getTabUngrouper().ungroupTabs(List.of(tab), trailing, false);
    }

    /**
     * Determines whether the given item model represents a child tab that is the only tab in its
     * group.
     */
    public static boolean isSolitaryChild(
            @Nullable TabModel tabModel, @Nullable PropertyModel model) {
        if (tabModel == null || model == null || TabProperties.isTabGroupHeader(model)) {
            return false;
        }
        Token groupId = getTabGroupId(model);
        if (groupId != null) {
            int tabId = getTabId(model);
            if (tabId != Tab.INVALID_TAB_ID) {
                List<Tab> relatedTabs = tabModel.getRelatedTabList(tabId);
                return relatedTabs != null && relatedTabs.size() == 1;
            }
        }
        return false;
    }

    // =============================================================================================
    // Private Helpers
    // =============================================================================================

    /**
     * Helper to reorder a tab group before or after an adjacent group/item.
     *
     * @param tabModel The active {@link TabModel}.
     * @param tabId The anchor tab ID of the group.
     * @param toPrevious Whether to move forward (up / previous) or backward (down / next).
     * @return Whether the group was successfully reordered.
     */
    private static boolean reorderTabGroupByAnchorTabId(
            TabModel tabModel, @TabId int tabId, boolean toPrevious) {
        List<Tab> currentGroup = tabModel.getRelatedTabList(tabId);
        int adjacentIndex;
        if (toPrevious) {
            adjacentIndex = TabGroupUtils.getFirstTabModelIndexForList(tabModel, currentGroup) - 1;
        } else {
            adjacentIndex = TabGroupUtils.getLastTabModelIndexForList(tabModel, currentGroup) + 1;
        }
        Tab adjacentTab = tabModel.getTabAt(adjacentIndex);
        if (adjacentTab == null || adjacentTab.getIsPinned()) return false;

        List<Tab> adjacentGroup = tabModel.getRelatedTabList(adjacentTab.getId());
        int newIndex;
        if (toPrevious) {
            newIndex = TabGroupUtils.getFirstTabModelIndexForList(tabModel, adjacentGroup);
        } else {
            newIndex = TabGroupUtils.getLastTabModelIndexForList(tabModel, adjacentGroup);
        }

        tabModel.moveRelatedTabs(tabId, newIndex);
        return true;
    }

    /** Returns whether the given {@link ListItem} represents a tab group header card. */
    private static boolean isTabGroupHeader(ListItem item) {
        if (item.type == TabProperties.UiType.TAB_GROUP) return true;
        if (item.model != null) {
            return TabProperties.isTabGroupHeader(item.model)
                    || item.model.get(TabProperties.TAB_GROUP_HEADER_ID) != null;
        }
        return false;
    }

    /** Returns whether the given {@link ListItem} represents a nested child tab inside a group. */
    private static boolean isChildTab(ListItem item) {
        if (isTabGroupHeader(item)) return false;
        if (item.model != null) {
            return item.model.containsKey(TabProperties.TAB_GROUP_ID)
                    && item.model.get(TabProperties.TAB_GROUP_ID) != null;
        }
        return false;
    }

    /** Returns the tab ID from the given {@link PropertyModel}, or {@link Tab#INVALID_TAB_ID}. */
    private static int getTabId(PropertyModel model) {
        return model.containsKey(TabProperties.TAB_ID)
                ? model.get(TabProperties.TAB_ID)
                : Tab.INVALID_TAB_ID;
    }

    /**
     * Returns a representative tab ID for the given {@link ListItem}, resolving from group ID if
     * necessary for group headers.
     */
    private static int getRepresentativeTabId(TabModel tabModel, ListItem item) {
        if (item.model == null) return Tab.INVALID_TAB_ID;
        int tabId = getTabId(item.model);
        if (tabId != Tab.INVALID_TAB_ID) return tabId;

        Token groupId = getTabGroupId(item.model);
        if (groupId != null) {
            List<Tab> tabs = tabModel.getTabsInGroup(groupId);
            if (tabs != null && !tabs.isEmpty()) {
                return tabs.get(0).getId();
            }
        }
        return Tab.INVALID_TAB_ID;
    }

    /** Adjusts the destination index to respect pinned tab boundaries. */
    private static int adjustIndexBasedOnPinning(TabModel tabModel, int fromTabId, int newIndex) {
        Tab fromTab = tabModel.getTabById(fromTabId);
        if (fromTab != null) {
            int lastPinnedIndex = tabModel.findFirstNonPinnedTabIndex() - 1;
            if (fromTab.getIsPinned()) {
                if (newIndex > lastPinnedIndex) {
                    newIndex = lastPinnedIndex;
                }
            } else {
                int firstNonPinnedIndex = tabModel.findFirstNonPinnedTabIndex();
                if (newIndex < firstNonPinnedIndex) {
                    newIndex = firstNonPinnedIndex;
                }
            }
        }
        return newIndex;
    }
}

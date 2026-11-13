// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.text.TextUtils;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabRestoreEntryId;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosedTab;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.GroupData;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Restores the tab group membership of tabs that were brought back by undoing a tab closure.
 *
 * <p>Whole hiding synced tab groups are reopened from {@link TabGroupSyncService} with their
 * pre-close collapsed state and moved to their adjusted earliest pre-close {@link
 * ClosedTab#modelIndex} before native {@code TabRestoreService} restore runs ({@link
 * #reopenHiddenGroups}), and then repositioned in {@link #apply} if native {@code
 * TabRestoreService} also restored tabs for the closure.
 *
 * <p>For tabs restored from {@code TabRestoreService} (which come back ungrouped), {@link #apply}
 * regroups them using the {@link UndoClosureSnapshot} captured at close time:
 *
 * <ul>
 *   <li>If the group still exists in the model under its original {@link Token} (Case A), or was
 *       reopened in the model under a new local {@link Token} in {@link TabGroupSyncService} (Case
 *       B), each restored tab is merged back into it at its {@link ClosedTab#indexInGroup} (while
 *       tabs of a group currently open in another window are left ungrouped in {@code tabModel}).
 *   <li>If the group is currently hidden in {@link TabGroupSyncService} (Case C), it is reopened,
 *       its collapsed state is restored, and the restored tabs are merged into the reopened group
 *       (falling back to recreation if reopening does not place the group in {@code tabModel}).
 *   <li>Otherwise (Case D: unsynced or deleted synced group), the group is recreated with its
 *       original {@link Token} and visual metadata.
 * </ul>
 */
@NullMarked
final class UndoGroupRestorer {
    private UndoGroupRestorer() {}

    /**
     * Reopens whole hiding synced tab groups from {@code snapshot} and moves each group to its
     * earliest pre-close {@link ClosedTab#modelIndex} before native {@code TabRestoreService}
     * restore runs so their tabs are present at their pre-close positions in {@code tabModel} when
     * native restore computes insertion indices.
     *
     * @param tabModel The model to reopen hidden groups into.
     * @param syncService The {@link TabGroupSyncService} for the model's profile, or {@code null}
     *     if unavailable.
     * @param uiActionHandler Handler used to reopen the saved tab group in the current window, or
     *     {@code null} if unavailable.
     * @param snapshot The snapshot captured when the tabs were closed.
     * @return An array of length {@code snapshot.tabs.size()} containing the reopened tab IDs at
     *     the corresponding slots of {@code snapshot.tabs}, and {@link Tab#INVALID_TAB_ID} at all
     *     other slots.
     */
    static int[] reopenHiddenGroups(
            TabModel tabModel,
            @Nullable TabGroupSyncService syncService,
            @Nullable TabGroupUiActionHandler uiActionHandler,
            UndoClosureSnapshot snapshot) {
        int tabCount = snapshot.tabs.size();
        int[] restoredIds = new int[tabCount];
        Arrays.fill(restoredIds, Tab.INVALID_TAB_ID);
        if (syncService == null
                || uiActionHandler == null
                || !snapshot.hasReopenableHiddenGroups()) {
            return restoredIds;
        }

        // Multiple entries in snapshot.tabs belong to the same closed group; track processed
        // group Tokens so each whole hiding group is reopened at most once.
        Set<Token> processedGroups = new HashSet<>();
        for (int i = 0; i < tabCount; i++) {
            ClosedTab closedTab = snapshot.tabs.get(i);
            Token groupId = closedTab.groupId;
            if (groupId == null || !processedGroups.add(groupId)) continue;

            GroupData groupData = snapshot.groups.get(groupId);
            if (groupData == null
                    || !groupData.isReopenableHiddenGroup()
                    || groupData.savedTabGroupSyncId == null) {
                continue;
            }

            String syncId = groupData.savedTabGroupSyncId;
            SavedTabGroup savedGroup = syncService.getGroup(syncId);
            if (savedGroup == null || savedGroup.localId != null) continue;

            // Reopening via TabGroupSyncService creates new tabs under a fresh local Token at the
            // end of tabModel.
            uiActionHandler.openTabGroup(syncId);
            Token newGroupId = findLocalGroupIdInModel(tabModel, syncService, syncId);
            if (newGroupId == null) continue;

            // Pair each newly created tab in the reopened group with its corresponding ClosedTab
            // slot in snapshot.tabs (both are ordered by in-group position).
            List<Tab> groupTabs = tabModel.getTabsInGroup(newGroupId);
            int tabIndexInGroup = 0;
            for (int j = i; j < tabCount && tabIndexInGroup < groupTabs.size(); j++) {
                ClosedTab candidate = snapshot.tabs.get(j);
                if (groupId.equals(candidate.groupId)) {
                    restoredIds[j] = groupTabs.get(tabIndexInGroup++).getId();
                }
            }
            // Move the reopened group to its adjusted earliest pre-close index now so its tabs
            // occupy their pre-close positions before native TabRestoreService restore computes
            // insertion indices for any remaining tabs.
            int targetIndex = findTargetGroupModelIndex(groupId, restoredIds, snapshot);
            moveGroupToIndex(tabModel, newGroupId, targetIndex);
            tabModel.setTabGroupCollapsed(newGroupId, groupData.collapsed, /* animate= */ false);
        }
        return restoredIds;
    }

    /**
     * Positions reopened hidden groups and regroups the tabs restored by an undo of a tab closure.
     *
     * @param tabModel The model the tabs were restored into.
     * @param syncService The {@link TabGroupSyncService} for the model's profile, or {@code null}
     *     if unavailable.
     * @param uiActionHandler Handler used to reopen a hidden saved tab group in the current window,
     *     or {@code null} if unavailable.
     * @param restoredIds The ids of the restored tabs, parallel to {@code snapshot.tabs}, with
     *     {@link Tab#INVALID_TAB_ID} for tabs that were not restored.
     * @param snapshot The snapshot captured when the tabs were closed.
     */
    static void apply(
            TabModel tabModel,
            @Nullable TabGroupSyncService syncService,
            @Nullable TabGroupUiActionHandler uiActionHandler,
            int[] restoredIds,
            UndoClosureSnapshot snapshot) {
        assert restoredIds.length == snapshot.tabs.size();

        Map<Token, List<RestoredTab>> restoredTabsByGroup = new LinkedHashMap<>();
        for (int i = 0; i < restoredIds.length; i++) {
            ClosedTab closedTab = snapshot.tabs.get(i);
            Token groupId = closedTab.groupId;
            if (groupId == null) continue;
            // Grouped tabs cannot be pinned; skip malformed entries rather than unpinning them.
            if (closedTab.pinned) continue;
            @TabId int restoredId = restoredIds[i];
            if (restoredId == Tab.INVALID_TAB_ID) continue;

            Tab tab = tabModel.getTabById(restoredId);
            if (tab == null) continue;

            restoredTabsByGroup
                    .computeIfAbsent(groupId, k -> new ArrayList<>())
                    .add(new RestoredTab(tab, closedTab));
        }

        for (Map.Entry<Token, List<RestoredTab>> entry : restoredTabsByGroup.entrySet()) {
            Token groupId = entry.getKey();
            List<RestoredTab> restoredTabs = entry.getValue();

            GroupData groupData = snapshot.groups.get(groupId);
            if (groupData != null && groupData.isHiding) {
                if (snapshot.entryId != TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID) {
                    positionReopenedHiddenGroup(
                            tabModel, syncService, groupId, groupData, restoredIds, snapshot);
                }
                continue;
            }

            // Sorting by ascending indexInGroup (falling back to model index) makes each insert
            // into a live group exact and preserves in-group order when recreating a group.
            restoredTabs.sort(
                    Comparator.comparingInt(
                                    (RestoredTab t) ->
                                            t.mClosedTab.indexInGroup == TabList.INVALID_TAB_INDEX
                                                    ? Integer.MAX_VALUE
                                                    : t.mClosedTab.indexInGroup)
                            .thenComparingInt(t -> tabModel.indexOf(t.mTab)));

            if (tabModel.containsTabGroup(groupId)) {
                mergeIntoLiveGroup(
                        tabModel, groupId, restoredTabs, tabModel.getTabGroupCollapsed(groupId));
                continue;
            }

            if (syncService == null || groupData == null || groupData.savedTabGroupSyncId == null) {
                recreateGroup(tabModel, groupId, restoredTabs, groupData);
                continue;
            }

            String syncId = groupData.savedTabGroupSyncId;
            SavedTabGroup savedGroup = syncService.getGroup(syncId);
            if (savedGroup == null) {
                recreateGroup(tabModel, groupId, restoredTabs, groupData);
                continue;
            }

            LocalTabGroupId localId = savedGroup.localId;
            if (localId != null) {
                Token localGroupId = localId.tabGroupId;
                if (tabModel.containsTabGroup(localGroupId)) {
                    mergeIntoLiveGroup(
                            tabModel,
                            localGroupId,
                            restoredTabs,
                            tabModel.getTabGroupCollapsed(localGroupId));
                }
                continue;
            }

            if (uiActionHandler != null) {
                uiActionHandler.openTabGroup(syncId);
                Token reopenedGroupId = findLocalGroupIdInModel(tabModel, syncService, syncId);
                if (reopenedGroupId != null) {
                    mergeIntoLiveGroup(
                            tabModel, reopenedGroupId, restoredTabs, groupData.collapsed);
                    continue;
                }
            }

            recreateGroup(tabModel, groupId, restoredTabs, groupData);
        }
    }

    private static void positionReopenedHiddenGroup(
            TabModel tabModel,
            @Nullable TabGroupSyncService syncService,
            Token groupId,
            GroupData groupData,
            int[] restoredIds,
            UndoClosureSnapshot snapshot) {
        String syncId = groupData.savedTabGroupSyncId;
        if (syncService == null || !groupData.isReopenableHiddenGroup() || syncId == null) return;

        Token newGroupId = findLocalGroupIdInModel(tabModel, syncService, syncId);
        if (newGroupId == null) return;

        int targetIndex = findTargetGroupModelIndex(groupId, restoredIds, snapshot);
        moveGroupToIndex(tabModel, newGroupId, targetIndex);
    }

    private static void moveGroupToIndex(TabModel tabModel, Token groupId, int targetIndex) {
        if (targetIndex == TabList.INVALID_TAB_INDEX) return;

        List<Tab> groupTabs = tabModel.getTabsInGroup(groupId);
        if (groupTabs.isEmpty()) return;

        int currentIndex = tabModel.indexOf(groupTabs.get(0));
        if (currentIndex == TabList.INVALID_TAB_INDEX || currentIndex == targetIndex) return;

        int destinationIndex =
                targetIndex > currentIndex ? targetIndex + groupTabs.size() - 1 : targetIndex;
        tabModel.moveGroupToIndex(groupId, destinationIndex);
    }

    /**
     * Computes the target model index for a reopened hidden group by finding its earliest pre-close
     * {@link ClosedTab#modelIndex} among restored tabs and subtracting the number of unrestored
     * snapshot entries with a smaller non-negative {@code modelIndex}.
     */
    private static int findTargetGroupModelIndex(
            Token groupId, int[] restoredIds, UndoClosureSnapshot snapshot) {
        int firstModelIndex = TabList.INVALID_TAB_INDEX;
        int tabCount = snapshot.tabs.size();
        for (int i = 0; i < tabCount; i++) {
            if (restoredIds[i] == Tab.INVALID_TAB_ID) continue;
            ClosedTab closedTab = snapshot.tabs.get(i);
            if (!groupId.equals(closedTab.groupId)) continue;
            int modelIndex = closedTab.modelIndex;
            if (modelIndex != TabList.INVALID_TAB_INDEX
                    && (firstModelIndex == TabList.INVALID_TAB_INDEX
                            || modelIndex < firstModelIndex)) {
                firstModelIndex = modelIndex;
            }
        }
        if (firstModelIndex == TabList.INVALID_TAB_INDEX) {
            return TabList.INVALID_TAB_INDEX;
        }
        int unrestoredBeforeCount = 0;
        for (int k = 0; k < tabCount; k++) {
            if (restoredIds[k] != Tab.INVALID_TAB_ID) continue;
            int modelIndex = snapshot.tabs.get(k).modelIndex;
            if (modelIndex >= 0 && modelIndex < firstModelIndex) {
                unrestoredBeforeCount++;
            }
        }
        return Math.max(0, firstModelIndex - unrestoredBeforeCount);
    }

    private static @Nullable Token findLocalGroupIdInModel(
            TabModel tabModel, TabGroupSyncService syncService, String syncId) {
        SavedTabGroup savedGroup = syncService.getGroup(syncId);
        if (savedGroup == null) return null;

        LocalTabGroupId localId = savedGroup.localId;
        if (localId == null) return null;

        Token localGroupId = localId.tabGroupId;
        return tabModel.containsTabGroup(localGroupId) ? localGroupId : null;
    }

    private static void mergeIntoLiveGroup(
            TabModel tabModel,
            Token groupId,
            List<RestoredTab> restoredTabs,
            boolean shouldCollapse) {
        for (RestoredTab restoredTab : restoredTabs) {
            int indexInGroup = restoredTab.mClosedTab.indexInGroup;
            tabModel.mergeListOfTabsToExistingGroup(
                    Collections.singletonList(restoredTab.mTab),
                    groupId,
                    indexInGroup == TabList.INVALID_TAB_INDEX ? null : indexInGroup,
                    TabGroupMergeNotificationType.DONT_NOTIFY);
        }
        if (tabModel.getTabGroupCollapsed(groupId) != shouldCollapse) {
            tabModel.setTabGroupCollapsed(groupId, shouldCollapse, /* animate= */ false);
        }
    }

    private static void recreateGroup(
            TabModel tabModel,
            Token groupId,
            List<RestoredTab> restoredTabs,
            @Nullable GroupData groupData) {
        List<Tab> tabs = new ArrayList<>(restoredTabs.size());
        for (RestoredTab restoredTab : restoredTabs) {
            tabs.add(restoredTab.mTab);
        }
        // Recreating the group fires the normal group-created observers, so a group that was synced
        // and deleted at close comes back as a new saved tab group with the same local Token. No
        // merge notification or undo snackbar is shown.
        tabModel.createTabGroupForTabGroupSync(tabs, groupId);

        if (groupData == null) return;

        String title = groupData.title;
        if (!TextUtils.isEmpty(title)) {
            tabModel.setTabGroupTitle(groupId, title);
        }
        if (groupData.color != TabGroupColorUtils.INVALID_COLOR_ID) {
            tabModel.setTabGroupColor(groupId, groupData.color);
        }
        if (groupData.collapsed) {
            tabModel.setTabGroupCollapsed(groupId, /* isCollapsed= */ true, /* animate= */ false);
        }
    }

    /** A restored tab paired with its closed-tab entry in the snapshot. */
    private static final class RestoredTab {
        final Tab mTab;
        final ClosedTab mClosedTab;

        RestoredTab(Tab tab, ClosedTab closedTab) {
            this.mTab = tab;
            this.mClosedTab = closedTab;
        }
    }
}

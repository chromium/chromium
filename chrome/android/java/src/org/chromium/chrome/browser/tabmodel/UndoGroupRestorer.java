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
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosedTab;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.GroupData;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Restores the tab group membership of tabs that were brought back by undoing a tab closure.
 *
 * <p>Undo restores every tab ungrouped, so this class regroups the restored tabs using the {@link
 * UndoClosureSnapshot} captured at close time. Only tabs that were actually restored are
 * considered, and groups are processed in the order in which their first restored tab appears in
 * {@link UndoClosureSnapshot#tabs}; a group with no restored tabs is left untouched. For each
 * group:
 *
 * <ul>
 *   <li>If the group was hiding, its tabs are left ungrouped.
 *   <li>If the group still exists in the model, i.e. only some of its tabs were closed, each
 *       restored tab is merged back into it at its {@link ClosedTab#indexInGroup}, in ascending
 *       {@link ClosedTab#indexInGroup} order (unknown positions appended in model order). The
 *       group's collapsed state from before the merge is then re-applied, as merging expands the
 *       group.
 *   <li>Otherwise, the group is recreated with its original {@link Token} from the restored tabs in
 *       ascending {@link ClosedTab#indexInGroup} order (unknown positions in model order), and the
 *       title (if non-empty), color (if valid) and collapsed state (if collapsed) captured in the
 *       snapshot are applied.
 * </ul>
 */
@NullMarked
final class UndoGroupRestorer {
    private UndoGroupRestorer() {}

    /**
     * Regroups the tabs restored by an undo of a tab closure.
     *
     * @param tabModel The model the tabs were restored into.
     * @param restoredIds The ids of the restored tabs, parallel to {@code snapshot.tabs}, with
     *     {@link Tab#INVALID_TAB_ID} for tabs that were not restored.
     * @param snapshot The snapshot captured when the tabs were closed.
     */
    static void apply(TabModel tabModel, int[] restoredIds, UndoClosureSnapshot snapshot) {
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
            if (groupData != null && groupData.isHiding) continue;

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
                mergeIntoLiveGroup(tabModel, groupId, restoredTabs);
            } else {
                recreateGroup(tabModel, groupId, restoredTabs, groupData);
            }
        }
    }

    private static void mergeIntoLiveGroup(
            TabModel tabModel, Token groupId, List<RestoredTab> restoredTabs) {
        boolean wasCollapsed = tabModel.getTabGroupCollapsed(groupId);
        for (RestoredTab restoredTab : restoredTabs) {
            int indexInGroup = restoredTab.mClosedTab.indexInGroup;
            tabModel.mergeListOfTabsToExistingGroup(
                    Collections.singletonList(restoredTab.mTab),
                    groupId,
                    indexInGroup == TabList.INVALID_TAB_INDEX ? null : indexInGroup,
                    TabGroupMergeNotificationType.DONT_NOTIFY);
        }
        if (tabModel.getTabGroupCollapsed(groupId) != wasCollapsed) {
            tabModel.setTabGroupCollapsed(groupId, wasCollapsed, /* animate= */ false);
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

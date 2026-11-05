// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.undo;

import static org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.forSavedGroups;
import static org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.forTabModel;

import org.chromium.base.Token;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabRestoreEntryId;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosedTab;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.GroupData;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Static factory methods that capture an {@link UndoClosureSnapshot} at close time. */
@NullMarked
public final class UndoClosureSnapshotFactory {
    private UndoClosureSnapshotFactory() {}

    /**
     * Captures a snapshot from the given tab model at close time. Evaluates the comprehensive
     * model's lazy group identifiers only when a closed tab is grouped.
     *
     * @param tabModel The model from which to capture state.
     * @param tabs The tabs being closed.
     * @param isAllTabs Whether all tabs in the model are being closed.
     * @param isTabGroupSyncEnabled Whether tab group sync is enabled for the model's profile.
     * @param closingSource The UI surface from which the tab closure originated.
     * @return The immutable snapshot representing the closure state.
     */
    public static UndoClosureSnapshot captureFromTabModel(
            TabModel tabModel,
            List<Tab> tabs,
            boolean isAllTabs,
            boolean isTabGroupSyncEnabled,
            @TabClosingSource int closingSource) {
        List<ClosedTab> closedTabs = new ArrayList<>(tabs.size());
        Map<Token, GroupData> groups = new HashMap<>();

        LazyOneshotSupplier<Set<Token>> lazyGroupIds =
                tabModel.getLazyAllTabGroupIds(tabs, /* includePendingClosures= */ true);

        Set<Token> closedGroupIds = new HashSet<>();
        for (Tab tab : tabs) {
            int id = tab.getId();
            String title = tab.getTitle();
            Token groupId = tab.getTabGroupId();
            closedTabs.add(
                    new ClosedTab(
                            id,
                            title,
                            groupId,
                            /* indexInGroup= */ TabList.INVALID_TAB_INDEX,
                            /* modelIndex= */ TabList.INVALID_TAB_INDEX,
                            tab.getIsPinned(),
                            /* wasSelected= */ false));
            if (groupId != null && closedGroupIds.add(groupId)) {
                String groupTitle = tabModel.getTabGroupTitle(groupId);
                int groupColor = tabModel.getTabGroupColorWithFallback(groupId);
                boolean groupCollapsed = tabModel.getTabGroupCollapsed(groupId);
                boolean groupIsHiding = tabModel.isTabGroupHiding(groupId);

                boolean survivesInModel = false;
                Set<Token> lazySet = lazyGroupIds.get();
                if (lazySet != null) {
                    survivesInModel = lazySet.contains(groupId);
                }

                groups.put(
                        groupId,
                        new GroupData(
                                groupTitle,
                                groupColor,
                                groupCollapsed,
                                groupIsHiding,
                                survivesInModel));
            }
        }

        return forTabModel(
                closedTabs,
                groups,
                isTabGroupSyncEnabled,
                isAllTabs,
                /* entryId= */ TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID,
                closingSource,
                /* undoRunnable= */ null);
    }

    /**
     * Captures a snapshot for saved tab groups at close time.
     *
     * @param tabs The tabs being closed.
     * @param syncIds The sync identifiers of the saved tab groups being closed.
     * @param tabGroupSyncService The service used to look up saved tab group details.
     * @return The immutable snapshot representing the closure state.
     */
    public static UndoClosureSnapshot captureSavedGroups(
            List<Tab> tabs,
            List<String> syncIds,
            @Nullable TabGroupSyncService tabGroupSyncService) {
        List<ClosedTab> closedTabs = new ArrayList<>(tabs.size());
        for (Tab tab : tabs) {
            closedTabs.add(
                    new ClosedTab(
                            tab.getId(),
                            tab.getTitle(),
                            /* groupId= */ null,
                            /* indexInGroup= */ TabList.INVALID_TAB_INDEX,
                            /* modelIndex= */ TabList.INVALID_TAB_INDEX,
                            tab.getIsPinned(),
                            /* wasSelected= */ false));
        }

        String savedGroupTitle = null;
        int savedGroupTabCount = 0;
        if (syncIds.size() == 1 && tabGroupSyncService != null) {
            SavedTabGroup group = tabGroupSyncService.getGroup(syncIds.get(0));
            if (group != null) {
                savedGroupTitle = group.title != null ? group.title : "";
                savedGroupTabCount = group.savedTabs.size();
            }
        }

        return forSavedGroups(closedTabs, syncIds, savedGroupTitle, savedGroupTabCount);
    }
}

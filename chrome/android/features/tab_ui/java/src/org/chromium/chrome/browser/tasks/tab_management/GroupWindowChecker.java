// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

/** For tab group lists to interact with {@link TabGroupSyncService} and multiple windows. */
@NullMarked
public class GroupWindowChecker {
    private final @Nullable TabGroupSyncService mSyncService;
    private final TabGroupModelFilter mFilter;

    /**
     * @param syncService The service to use for accessing synced tab groups.
     * @param filter Used for accessing tab information.
     */
    public GroupWindowChecker(
            @Nullable TabGroupSyncService syncService, TabGroupModelFilter filter) {
        mSyncService = syncService;
        mFilter = filter;
    }

    /**
     * Returns a sorted list of {@link SavedTabGroup}s.
     *
     * <p>The list includes all synced tab groups, except those that are currently open in other
     * windows. The list is sorted using the provided comparator.
     *
     * @param comparator Used for sorting the list.
     */
    public List<SavedTabGroup> getSortedGroupList(Comparator<SavedTabGroup> comparator) {
        List<SavedTabGroup> groupList = new ArrayList<>();
        if (mSyncService == null) return groupList;

        for (String syncGroupId : mSyncService.getAllGroupIds()) {
            SavedTabGroup savedTabGroup = mSyncService.getGroup(syncGroupId);
            assert savedTabGroup != null && !savedTabGroup.savedTabs.isEmpty();

            // To simplify interactions, do not include any groups currently open in other windows.
            if (getState(savedTabGroup) != GroupWindowState.IN_ANOTHER) {
                groupList.add(savedTabGroup);
            }
        }
        groupList.sort(comparator);
        return groupList;
    }

    /** Returns the {@link GroupWindowState} of the given {@link SavedTabGroup}. */
    public @GroupWindowState int getState(SavedTabGroup savedTabGroup) {
        if (savedTabGroup.localId == null) {
            return GroupWindowState.HIDDEN;
        }

        Token groupId = savedTabGroup.localId.tabGroupId;
        boolean isFullyClosing = true;
        int rootId = Tab.INVALID_TAB_ID;

        TabList tabList = mFilter.getTabModel().getComprehensiveModel();
        for (int i = 0; i < tabList.getCount(); i++) {
            Tab tab = tabList.getTabAt(i);
            if (groupId.equals(tab.getTabGroupId())) {
                rootId = tab.getRootId();
                isFullyClosing &= tab.isClosing();
            }
        }
        if (rootId == Tab.INVALID_TAB_ID) return GroupWindowState.IN_ANOTHER;

        // If the group is only partially closing no special case is required since we still have to
        // do all the IN_CURRENT work and returning to the tab group via the dialog will work.
        return isFullyClosing ? GroupWindowState.IN_CURRENT_CLOSING : GroupWindowState.IN_CURRENT;
    }
}

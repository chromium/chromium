// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import android.util.Pair;

import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupColorUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.tab_group_sync.ClosingSource;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * Helper class to create a {@link SavedTabGroup} based on a local tab group. It's a wrapper around
 * {@link TabGroupSyncService} to help with invoking mutation methods.
 */
public class RemoteTabGroupMutationHelper {
    private static final String TAG = "TG.RemoteMutation";
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final TabGroupSyncService mTabGroupSyncService;

    /**
     * Constructor.
     *
     * @param tabGroupModelFilter The local tab model.
     * @param tabGroupSyncService The sync backend.
     */
    public RemoteTabGroupMutationHelper(
            TabGroupModelFilter tabGroupModelFilter, TabGroupSyncService tabGroupSyncService) {
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupSyncService = tabGroupSyncService;
    }

    /**
     * Creates a remote tab group corresponding to the given local tab group.
     *
     * @param groupId The ID of the local tab group.
     */
    public void createRemoteTabGroup(LocalTabGroupId groupId) {
        LogUtils.log(TAG, "createRemoteTabGroup, groupId = " + groupId.tabGroupId);
        // Create an empty group and set visuals. This will create a mapping in native as well.
        mTabGroupSyncService.createGroup(groupId);
        updateVisualData(groupId);

        // Add tabs to the group.
        int rootId = TabGroupSyncUtils.getRootId(mTabGroupModelFilter, groupId);
        List<Tab> tabs = mTabGroupModelFilter.getRelatedTabListForRootId(rootId);
        for (int position = 0; position < tabs.size(); position++) {
            addTab(groupId, tabs.get(position), position);
        }
    }

    /**
     * Called to update the visual data of a remote tab group. Uses default values, if title or
     * color are still unset for the local tab group.
     *
     * @param groupId The ID the local tab group.
     */
    public void updateVisualData(LocalTabGroupId groupId) {
        int rootId = TabGroupSyncUtils.getRootId(mTabGroupModelFilter, groupId);
        String title = mTabGroupModelFilter.getTabGroupTitle(rootId);
        if (title == null) title = new String();

        int color = mTabGroupModelFilter.getTabGroupColor(rootId);
        if (color == TabGroupColorUtils.INVALID_COLOR_ID) color = TabGroupColorId.GREY;

        mTabGroupSyncService.updateVisualData(groupId, title, color);
    }

    /**
     * Removes a tab group from sync.
     *
     * @param groupId The local tab group ID.
     */
    public void removeGroup(LocalTabGroupId groupId) {
        mTabGroupSyncService.removeGroup(groupId);
    }

    public void addTab(LocalTabGroupId tabGroupId, Tab tab, int position) {
        Pair<GURL, String> urlAndTitle =
                TabGroupSyncUtils.getFilteredUrlAndTitle(tab.getUrl(), tab.getTitle());
        mTabGroupSyncService.addTab(
                tabGroupId, tab.getId(), urlAndTitle.second, urlAndTitle.first, position);
    }

    public void moveTab(LocalTabGroupId tabGroupId, int tabId, int newPosition) {
        mTabGroupSyncService.moveTab(tabGroupId, tabId, newPosition);
    }

    public void removeTab(LocalTabGroupId tabGroupId, int tabId) {
        mTabGroupSyncService.removeTab(tabGroupId, tabId);
    }

    /**
     * Updates tab ID mappings for a particular group.
     *
     * @param localGroupId The local ID of the tab group.
     */
    public void updateTabIdMappingsOnStartup(LocalTabGroupId localGroupId) {
        LogUtils.log(TAG, "updateTabIdMappingsOnStartup, localGroupId = " + localGroupId);
        // Update tab ID mapping for tabs in the group.
        SavedTabGroup group = mTabGroupSyncService.getGroup(localGroupId);
        if (group == null) return;

        int rootId = TabGroupSyncUtils.getRootId(mTabGroupModelFilter, localGroupId);
        List<Tab> tabs = mTabGroupModelFilter.getRelatedTabListForRootId(rootId);
        // We just reconciled local state with sync. The tabs should match.
        assert tabs.size() == group.savedTabs.size()
                : "Local tab count doesn't match with remote : local #"
                        + tabs.size()
                        + " vs remote #"
                        + group.savedTabs.size();
        for (int i = 0; i < group.savedTabs.size() && i < tabs.size(); i++) {
            SavedTabGroupTab savedTab = group.savedTabs.get(i);
            mTabGroupSyncService.updateLocalTabId(
                    localGroupId, savedTab.syncId, tabs.get(i).getId());
        }
    }

    /**
     * Handle a tab group being closed.
     *
     * @param groupId The group ID being closed.
     * @param wasHiding Whether the group is hiding instead of being deleted.
     */
    public void handleCommittedTabGroupClosure(LocalTabGroupId groupId, boolean wasHiding) {
        int closingSource =
                wasHiding ? ClosingSource.CLOSED_BY_USER : ClosingSource.DELETED_BY_USER;
        mTabGroupSyncService.removeLocalTabGroupMapping(groupId, closingSource);
        if (!wasHiding) {
            // When deleting drop the group from sync entirely.
            removeGroup(groupId);
            RecordUserAction.record("TabGroups.Sync.LocalDeleted");
        } else {
            RecordUserAction.record("TabGroups.Sync.LocalHidden");
        }
    }

    /**
     * Handle tab closure and notifies sync. Note, tab groups that are closed as part of close
     * group, or close all tabs, or close multiple tabs shouldn't be removed from sync. However,
     * individual tab closures should be treated as tab removal from they synced group. This is done
     * by checking if the tabs being closed contains an entire group.
     */
    public void handleMultipleTabClosure(List<Tab> tabs) {
        LogUtils.log(TAG, "handleMultipleTabClosure, tabs# " + tabs.size());
        // Filter out tabs that weren't in a group.
        List<Tab> tabsInGroups =
                tabs.stream()
                        .filter(tab -> tab.getTabGroupId() != null)
                        .collect(Collectors.toList());

        LazyOneshotSupplier<Set<Token>> tabGroupIdsInComprehensiveModel =
                mTabGroupModelFilter.getLazyAllTabGroupIdsInComprehensiveModel(tabs);
        for (Tab tab : tabsInGroups) {
            Token tabGroupId = tab.getTabGroupId();
            if (mTabGroupModelFilter.isTabGroupHiding(tabGroupId)
                    && !tabGroupIdsInComprehensiveModel.get().contains(tabGroupId)) {
                continue;
            }

            // Remaining tabs will be in a tab group, but the closure event is either:
            // 1. A subset of tabs in the group.
            // 2. The group itself is to be deleted from sync so removing the tabs is ok.
            mTabGroupSyncService.removeTab(TabGroupSyncUtils.getLocalTabGroupId(tab), tab.getId());
        }
    }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupColorUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * Helper class to create a {@link SavedTabGroup} based on a local tab group. It's a wrapper around
 * {@link TabGroupSyncService} to help with invoking mutation methods.
 */
public class RemoteTabGroupMutationHelper {
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
     * @param groupId The root ID of the local tab group.
     */
    public void createRemoteTabGroup(int groupId) {
        // Create an empty group and set visuals.
        String syncId = mTabGroupSyncService.createGroup(new LocalTabGroupId(groupId));
        updateVisualData(groupId);

        // Add tabs to the group.
        List<Tab> tabs = mTabGroupModelFilter.getRelatedTabList(groupId);
        for (int position = 0; position < tabs.size(); position++) {
            addTab(groupId, tabs.get(position), position);
        }

        // Persist sync ID mapping for the tab group in shared preference.
        mapTabGroupId(groupId, syncId);
    }

    /**
     * Called to update the visual data of a remote tab group. Uses default values, if title or
     * color are still unset for the local tab group.
     *
     * @param groupId The root ID of the local tab group.
     */
    public void updateVisualData(int groupId) {
        String title = mTabGroupModelFilter.getTabGroupTitle(groupId);
        if (title == null) title = new String();

        int color = mTabGroupModelFilter.getTabGroupColor(groupId);
        if (color == TabGroupColorUtils.INVALID_COLOR_ID) color = TabGroupColorId.GREY;

        mTabGroupSyncService.updateVisualData(new LocalTabGroupId(groupId), title, color);
    }

    /**
     * Removes a tab group from sync.
     *
     * @param groupId The local tab group ID.
     */
    public void removeGroup(int groupId) {
        mTabGroupSyncService.removeGroup(new LocalTabGroupId(groupId));
    }

    /**
     * Updates the tab group ID mapping when the root ID of the group changes.
     *
     * @param oldGroupId The old local tab group ID.
     * @param newGroupId The new local tab group ID.
     */
    public void onLocalGroupIdChanged(int oldGroupId, int newGroupId) {
        unmapTabGroupId(oldGroupId);
        SavedTabGroup group = mTabGroupSyncService.getGroup(new LocalTabGroupId(oldGroupId));
        if (group == null) return;
        mapTabGroupId(newGroupId, group.syncId);
    }

    public void addTab(int tabGroupId, Tab tab, int position) {
        mTabGroupSyncService.addTab(
                new LocalTabGroupId(tabGroupId),
                tab.getId(),
                tab.getTitle(),
                tab.getUrl(),
                position);
    }

    public void updateTab(int tabGroupId, Tab tab, int position) {
        mTabGroupSyncService.updateTab(
                new LocalTabGroupId(tabGroupId),
                tab.getId(),
                tab.getTitle(),
                tab.getUrl(),
                position);
    }

    public void removeTab(int tabGroupId, int tabId) {
        mTabGroupSyncService.removeTab(new LocalTabGroupId(tabGroupId), tabId);
    }

    /**
     * Updates ID mappings for the tab group ID and optionally tab IDs for a particular group in
     * {@link TabGroupSyncService}. Doesn't update the mapping in the prefs as it's already stored.
     *
     * @param syncGroupId The sync ID of the tab group.
     * @param localGroupId The local ID of the tab group.
     * @param updateTabIds Whether or not the tab IDs should also be updated.
     */
    public void updateIdMappingForGroupOnStartup(
            String syncGroupId, int localGroupId, boolean updateTabIds) {
        // Update tab group ID mapping.
        mTabGroupSyncService.updateLocalTabGroupMapping(
                syncGroupId, new LocalTabGroupId(localGroupId));
        if (!updateTabIds) return;

        // Update tab ID mapping for tabs in the group.
        SavedTabGroup group = mTabGroupSyncService.getGroup(new LocalTabGroupId(localGroupId));
        List<Integer> tabIds = mTabGroupModelFilter.getRelatedTabIds(localGroupId);
        for (int i = 0; i < group.savedTabs.size() && i < tabIds.size(); i++) {
            SavedTabGroupTab savedTab = group.savedTabs.get(i);
            mTabGroupSyncService.updateLocalTabId(
                    new LocalTabGroupId(localGroupId), savedTab.syncId, tabIds.get(i));
        }
    }

    /** Adds mapping for a tab group ID to the service and persistence. */
    public void mapTabGroupId(int groupId, String syncId) {
        mTabGroupSyncService.updateLocalTabGroupMapping(syncId, new LocalTabGroupId(groupId));
        mTabGroupModelFilter.setTabGroupSyncId(groupId, syncId);
    }

    /** Removes mapping for a tab group ID from service and persistence. */
    public void unmapTabGroupId(int groupId) {
        mTabGroupSyncService.removeLocalTabGroupMapping(new LocalTabGroupId(groupId));
        mTabGroupModelFilter.setTabGroupSyncId(groupId, null);
    }

    /**
     * Handle tab closure and notifies sync. Note, tab groups that are closed as part of close
     * group, or close all tabs, or close multiple tabs shouldn't be removed from sync. However,
     * individual tab closures should be treated as tab removal from they synced group. This is done
     * by checking if the tabs being closed contains an entire group.
     */
    public void handleMultipleTabClosure(List<Tab> tabs) {
        // Filter out tabs that weren't in a group.
        List<Tab> tabsInGroups =
                tabs.stream()
                        .filter(tab -> tab.getTabGroupId() != null)
                        .collect(Collectors.toList());

        // Find out tab groups being closed. Don't remove them from sync, only drop the local
        // mapping from shared prefs and the service.
        Set<Integer> groupsClosing = findCompleteGroups(tabsInGroups);
        for (int groupId : groupsClosing) {
            unmapTabGroupId(groupId);
        }

        // The rest of the tabs are the ones that are being removed from their groups. Remove them
        // from sync.
        Set<Tab> tabsToRemove = findTabsNotInCompleteGroups(tabsInGroups, groupsClosing);
        for (Tab tab : tabsToRemove) {
            mTabGroupSyncService.removeTab(new LocalTabGroupId(tab.getRootId()), tab.getId());
        }
    }

    private Set<Integer> findCompleteGroups(List<Tab> tabs) {
        Set<Integer> tabIds = tabs.stream().map(Tab::getId).collect(Collectors.toSet());

        Set<Integer> completeGroups = new HashSet<>();
        for (Tab tab : tabs) {
            if (completeGroups.contains(tab.getRootId())) continue;

            List<Tab> relatedTabs =
                    mTabGroupModelFilter.getRelatedTabListForRootId(tab.getRootId());
            if (areAllTabsInGroup(relatedTabs, tabIds)) {
                completeGroups.add(tab.getRootId());
            }
        }

        return completeGroups;
    }

    private static Set<Tab> findTabsNotInCompleteGroups(
            List<Tab> tabs, Set<Integer> completeGroups) {
        return tabs.stream()
                .filter(tab -> !completeGroups.contains(tab.getRootId()))
                .collect(Collectors.toSet());
    }

    private static boolean areAllTabsInGroup(List<Tab> tabs, Set<Integer> tabIdsInGroups) {
        return tabs.stream().allMatch(tab -> tabIdsInGroups.contains(tab.getId()));
    }
}

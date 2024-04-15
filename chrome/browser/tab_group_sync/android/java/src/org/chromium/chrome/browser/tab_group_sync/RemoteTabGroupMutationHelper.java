// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupColorUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.List;

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
        String syncId = mTabGroupSyncService.createGroup(groupId);
        updateVisualData(groupId);

        // Add tabs to the group.
        List<Tab> tabs = mTabGroupModelFilter.getRelatedTabList(groupId);
        for (int position = 0; position < tabs.size(); position++) {
            addTab(groupId, tabs.get(position), position);
        }

        // Persist sync ID mapping for the tab group in shared preference.
        mTabGroupModelFilter.setTabGroupSyncId(groupId, syncId);
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

        mTabGroupSyncService.updateVisualData(groupId, title, color);
    }

    public void addTab(int tabGroupId, Tab tab, int position) {
        mTabGroupSyncService.addTab(
                tabGroupId, tab.getId(), tab.getTitle(), tab.getUrl(), position);
    }

    public void updateTab(int tabGroupId, Tab tab, int position) {
        mTabGroupSyncService.updateTab(
                tabGroupId, tab.getId(), tab.getTitle(), tab.getUrl(), position);
    }

    public void removeTab(int tabGroupId, int tabId) {
        mTabGroupSyncService.removeTab(tabGroupId, tabId);
    }
}

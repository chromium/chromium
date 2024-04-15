// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncController.TabCreationDelegate;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Helper class to create, modify, overwrite local tab groups in response to sync updates and
 * startup.
 */
public final class LocalTabGroupMutationHelper {
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final TabGroupSyncService mTabGroupSyncService;
    private final TabCreationDelegate mTabCreationDelegate;
    private final NavigationTracker mNavigationTracker;

    /** Constructor. */
    public LocalTabGroupMutationHelper(
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupSyncService tabGroupSyncService,
            TabCreationDelegate tabCreationDelegate,
            NavigationTracker navigationTracker) {
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupSyncService = tabGroupSyncService;
        mTabCreationDelegate = tabCreationDelegate;
        mNavigationTracker = navigationTracker;
    }

    /**
     * Called in response to a tab group being added from sync that didn't exist locally. This will
     * create the group locally, update its visuals, add new tabs with desired URLs, update the
     * mapping in the service.
     */
    public void createNewTabGroup(SavedTabGroup tabGroup) {
        // If the incoming tab group is empty, don't add it. Wait for another update that has at
        // least one tab.
        if (tabGroup.savedTabs.isEmpty()) return;

        // For tracking IDs of the tabs to be created.
        Map<String, Integer> tabIdMappings = new HashMap<>();

        // We need to create a local tab group matching the remote one.
        // First create new tabs and append them to the end of tab model.
        int position = getTabModel().getCount();
        List<Tab> tabs = new ArrayList<>();
        for (SavedTabGroupTab savedTab : tabGroup.savedTabs) {
            tabs.add(mTabCreationDelegate.createBackgroundTab(savedTab.url, null, position++));
            tabIdMappings.put(savedTab.syncId, tabs.get(tabs.size() - 1).getId());
        }

        // Create a new tab group and add the tabs just created. Group ID is the ID of the first new
        // tab.
        int groupId = tabs.get(0).getId();
        updateTabGroupVisuals(tabGroup, groupId);
        mTabGroupModelFilter.mergeListOfTabsToGroup(
                tabs, tabs.get(0), /* isSameGroup= */ true, /* notify= */ false);

        // Notify sync backend about IDs of the newly created group and tabs.
        mTabGroupSyncService.updateLocalTabGroupId(tabGroup.syncId, groupId);
        for (String syncTabId : tabIdMappings.keySet()) {
            mTabGroupSyncService.updateLocalTabId(groupId, syncTabId, tabIdMappings.get(syncTabId));
        }

        // Update shared prefs about tab group mapping.
        mTabGroupModelFilter.setTabGroupSyncId(groupId, tabGroup.syncId);
    }

    /**
     * Called in response to a tab group being updated from sync that is already mapped in memory.
     * It will try to match the local tabs to the sync ones based on their IDs. Removes any tabs
     * that don't have mapping in sync already. Updates the URLs and positions of the tabs to match
     * sync. Creates new tabs for new incoming sync tabs.
     */
    public void updateTabGroup(SavedTabGroup tabGroup) {
        // We got the updated tab group from sync. We need to update the local one to match.
        // First close any extra tabs that aren't in sync.
        closeLocalTabsNotInSync(tabGroup);

        List<Tab> tabs = mTabGroupModelFilter.getRelatedTabListForRootId(tabGroup.localId);
        if (tabs.isEmpty()) {
            return;
        }

        // Update the remaining tabs. If the tab is already there, ensure its URL is up-to-date.
        // If the tab doesn't exist yet, create a new one.
        // TODO(b/333721527): This is assuming that the tabs are continuous and the first tab in
        // the related tab list has the lowest index. Verify if this is correct.
        int groupStartIndex = TabModelUtils.getTabIndexById(getTabModel(), tabs.get(0).getId());
        Tab parent = tabs.get(0);
        for (int i = 0; i < tabGroup.savedTabs.size(); i++) {
            SavedTabGroupTab savedTab = tabGroup.savedTabs.get(i);
            Tab localTab = getLocalTab(savedTab.localId);
            int desiredTabIndex = groupStartIndex + i;
            if (localTab != null) {
                maybeNavigateToUrl(localTab, savedTab.url);
            } else {
                localTab =
                        mTabCreationDelegate.createBackgroundTab(
                                savedTab.url, parent, desiredTabIndex);

                mTabGroupModelFilter.mergeTabsToGroup(
                        /* sourceTabId= */ localTab.getId(),
                        /* destinationTabId= */ tabGroup.localId);
                mTabGroupSyncService.updateLocalTabId(
                        tabGroup.localId, savedTab.syncId, localTab.getId());
            }

            // Move tab if required.
            getTabModel().moveTab(localTab.getId(), desiredTabIndex);
        }

        updateTabGroupVisuals(tabGroup, tabGroup.localId);
    }

    private void closeLocalTabsNotInSync(SavedTabGroup savedTabGroup) {
        List<Tab> tabs = findLocalTabsNotInSync(savedTabGroup);
        getTabModel().closeMultipleTabs(tabs, /* canUndo= */ false);
    }

    private List<Tab> findLocalTabsNotInSync(SavedTabGroup savedTabGroup) {
        Set<Integer> savedTabIds = new HashSet<>();
        for (SavedTabGroupTab savedTab : savedTabGroup.savedTabs) {
            if (savedTab.localId == null) continue;
            savedTabIds.add(savedTab.localId);
        }

        List<Tab> tabsNotInSync = new ArrayList<>();
        for (Tab localTab :
                mTabGroupModelFilter.getRelatedTabListForRootId(savedTabGroup.localId)) {
            if (!savedTabIds.contains(localTab.getId())) {
                tabsNotInSync.add(localTab);
            }
        }

        return tabsNotInSync;
    }

    private void maybeNavigateToUrl(Tab tab, GURL url) {
        // If the tab is already at the correct URL, don't do anything.
        if (url.equals(tab.getUrl())) return;

        // Set the new URL on the tab. But defer the navigation until the tab becomes active.
        // TODO(b/333721527): Implement lazy loading.
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        mNavigationTracker.setNavigationWasFromSync(loadUrlParams.getNavigationHandleUserData());
        tab.loadUrl(loadUrlParams);
    }

    private Tab getLocalTab(Integer tabId) {
        return tabId == null ? null : TabModelUtils.getTabById(getTabModel(), tabId);
    }

    private void updateTabGroupVisuals(SavedTabGroup tabGroup, int rootId) {
        mTabGroupModelFilter.setTabGroupTitle(rootId, tabGroup.title);
        mTabGroupModelFilter.setTabGroupColor(rootId, tabGroup.color);
    }

    private TabModel getTabModel() {
        return mTabGroupModelFilter.getTabModel();
    }
}

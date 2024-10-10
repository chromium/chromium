// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncController.TabCreationDelegate;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.tab_group_sync.ClosingSource;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.OpeningSource;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
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
public class LocalTabGroupMutationHelper {
    private static final String TAG = "TG.LocalMutation";
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final TabGroupSyncService mTabGroupSyncService;
    private final TabCreationDelegate mTabCreationDelegate;

    /** Constructor. */
    public LocalTabGroupMutationHelper(
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupSyncService tabGroupSyncService,
            TabCreationDelegate tabCreationDelegate) {
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupSyncService = tabGroupSyncService;
        mTabCreationDelegate = tabCreationDelegate;
    }

    /**
     * Called in response to a tab group being added from sync that didn't exist locally. This will
     * create the group locally, update its visuals, add new tabs with desired URLs, update the
     * mapping in the service.
     */
    public void createNewTabGroup(SavedTabGroup tabGroup, @OpeningSource int openingSource) {
        LogUtils.log(TAG, "createNewTabGroup " + tabGroup);
        // We ensure in native that the observers are notified only after the group has received at
        // least one tab.
        assert !tabGroup.savedTabs.isEmpty();

        // For tracking IDs of the tabs to be created.
        Map<String, Integer> tabIdMappings = new HashMap<>();

        // We need to create a local tab group matching the remote one.
        // First create new tabs and append them to the end of tab model.
        int position = getTabModel().getCount();
        List<Tab> tabs = new ArrayList<>();
        for (SavedTabGroupTab savedTab : tabGroup.savedTabs) {
            Tab newTab =
                    mTabCreationDelegate.createBackgroundTab(
                            savedTab.url, savedTab.title, /* parent= */ null, position++);
            tabs.add(newTab);
            tabIdMappings.put(savedTab.syncId, newTab.getId());
            RecordUserAction.record("TabGroups.Sync.CreatedNewTab");
        }

        // Create a new tab group and add the tabs just created. Group ID is the ID of the first new
        // tab.
        Tab rootTab = tabs.get(0);
        int rootId = rootTab.getId();
        updateTabGroupVisuals(tabGroup, rootId);
        if (tabs.size() == 1) {
            mTabGroupModelFilter.createSingleTabGroup(rootTab, /* notify= */ false);
        } else {
            mTabGroupModelFilter.mergeListOfTabsToGroup(tabs, rootTab, /* notify= */ false);
        }
        // Remote group should start collapsed. Do this after the merge to avoid auto expand.
        mTabGroupModelFilter.setTabGroupCollapsed(rootId, true);

        // Notify sync backend about IDs of the newly created group and tabs.
        LocalTabGroupId localTabGroupId =
                TabGroupSyncUtils.getLocalTabGroupId(mTabGroupModelFilter, rootId);
        assert localTabGroupId != null : "Local tab group ID is null after creating a group!";
        mTabGroupSyncService.updateLocalTabGroupMapping(
                tabGroup.syncId, localTabGroupId, openingSource);
        for (String syncTabId : tabIdMappings.keySet()) {
            mTabGroupSyncService.updateLocalTabId(
                    localTabGroupId, syncTabId, tabIdMappings.get(syncTabId));
        }
    }

    /**
     * Called in response to a tab group being updated from sync that is already mapped in memory.
     * It will try to match the local tabs to the sync ones based on their IDs. Removes any tabs
     * that don't have mapping in sync already. Updates the URLs and positions of the tabs to match
     * sync. Creates new tabs for new incoming sync tabs.
     */
    public void updateTabGroup(SavedTabGroup tabGroup) {
        LogUtils.log(TAG, "updateTabGroup ");
        reconcileGroup(tabGroup, /* isOnStartup= */ false);
    }

    /**
     * Called on startup to force update local state to sync. It will apply the sync tab URLs to
     * their local counterparts in order of their position in the group. Creates new tabs if the
     * local group has less tabs than the synced one.
     */
    public void reconcileGroupOnStartup(SavedTabGroup tabGroup) {
        LogUtils.log(TAG, "reconcileGroupOnStartup ");
        reconcileGroup(tabGroup, /* isOnStartup= */ true);
    }

    /**
     * Called to reconcile a local tab group with its synced counterpart. Called both on startup and
     * on subsequent sync updates. Depending on {@code isOnStartup}, it matches the tab by position
     * or by ID. Closes any tabs that are extra, and creates new ones when needed. Navigates the
     * tabs if they aren't on the correct URL already.
     *
     * <p>TODO(b/322856551): Persist tab ID mapping along with the group ID mapping. After that we
     * won't need to run a different routine on startup.
     */
    private void reconcileGroup(SavedTabGroup tabGroup, boolean isOnStartup) {
        LogUtils.log(TAG, "reconcileGroup " + tabGroup);
        assert tabGroup.localId != null;

        int rootId = TabGroupSyncUtils.getRootId(mTabGroupModelFilter, tabGroup.localId);
        List<Tab> tabs = mTabGroupModelFilter.getRelatedTabListForRootId(rootId);
        assert !tabs.isEmpty();
        if (tabs.isEmpty()) {
            LogUtils.log(TAG, "Found no tabs in the local group");
            return;
        }

        // We want to reconcile the local group with the synced group.
        // The algorithm is different depending on whether we are running this on startup or for a
        // subsequent sync update.
        // For subsequent sync updates, we need to close any extra tabs that aren't in sync.
        List<Tab> tabsToClose = new ArrayList<>();
        if (isOnStartup) {
            if (tabs.size() > tabGroup.savedTabs.size()) {
                tabsToClose = tabs.subList(tabGroup.savedTabs.size(), tabs.size());
            }
        } else {
            tabsToClose = findLocalTabsNotInSyncPostStartup(tabGroup);
        }

        // Update the remaining tabs. If the tab is already there, ensure its URL is up-to-date.
        // If the tab doesn't exist yet, create a new one.
        // Note, root ID might have changed due to the close operations. Query it again.
        rootId = TabGroupSyncUtils.getRootId(mTabGroupModelFilter, tabGroup.localId);
        tabs = mTabGroupModelFilter.getRelatedTabListForRootId(rootId);
        int groupStartIndex = TabModelUtils.getTabIndexById(getTabModel(), tabs.get(0).getId());
        Tab parent = tabs.get(0);
        boolean wasCollapsed = mTabGroupModelFilter.getTabGroupCollapsed(rootId);
        for (int i = 0; i < tabGroup.savedTabs.size(); i++) {
            SavedTabGroupTab savedTab = tabGroup.savedTabs.get(i);
            int desiredTabModelIndex = groupStartIndex + i;
            Tab localTab = null;
            // Find the tab by position on startup, or by tab ID on subsequent update.
            if (isOnStartup) {
                localTab = i < tabs.size() ? tabs.get(i) : null;
            } else {
                localTab = getLocalTabInGroup(savedTab.localId, rootId);
            }

            // If the tab exists, navigate to the desired URL. Otherwise, create a new tab.
            if (localTab != null) {
                maybeNavigateToUrl(localTab, savedTab.url, savedTab.title);
            } else {
                localTab =
                        createTabAndAddToGroup(
                                savedTab.url, savedTab.title, desiredTabModelIndex, parent, rootId);
                mTabGroupSyncService.updateLocalTabId(
                        tabGroup.localId, savedTab.syncId, localTab.getId());
            }

            // Move tab if required.
            getTabModel().moveTab(localTab.getId(), desiredTabModelIndex);
        }

        if (!tabsToClose.isEmpty()) {
            getTabModel()
                    .closeTabs(TabClosureParams.closeTabs(tabsToClose).allowUndo(false).build());
        }
        updateTabGroupVisuals(tabGroup, rootId);
        // TODO(crbug.com/346406221): This currently causes the layout strip to flicker as events
        // still escape the filter and kick off animations. Rework somehow to avoid.
        mTabGroupModelFilter.setTabGroupCollapsed(rootId, wasCollapsed);
    }

    /** Helper method to create a tab with a given URL and add it to the tab group. */
    private Tab createTabAndAddToGroup(
            GURL url, String title, int desiredTabModelIndex, Tab parentTab, int rootId) {
        Tab newTab =
                mTabCreationDelegate.createBackgroundTab(
                        url, title, parentTab, desiredTabModelIndex);
        RecordUserAction.record("TabGroups.Sync.CreatedNewTab");

        List<Tab> tabsToMerge = new ArrayList<>();
        tabsToMerge.add(newTab);
        mTabGroupModelFilter.mergeListOfTabsToGroup(
                tabsToMerge, getTabModel().getTabById(rootId), /* notify= */ false);
        return newTab;
    }

    /**
     * Called to close a tab group in response to a tab group being removed from sync or sync being
     * disabled due to sign out or turning off sync. For non-primary windows, it could be invoked
     * during the next startup of the window. This function is responsible for notifying sync that
     * the group has been closed and drop the mapping.
     *
     * @param tabGroupId The local ID of the tab group.
     */
    public void closeTabGroup(LocalTabGroupId tabGroupId, @ClosingSource int closingSource) {
        LogUtils.log(TAG, "closeTabGroup " + tabGroupId);
        int rootId = TabGroupSyncUtils.getRootId(mTabGroupModelFilter, tabGroupId);
        assert rootId != Tab.INVALID_TAB_ID;

        // Close the tabs.
        List<Tab> tabs = mTabGroupModelFilter.getRelatedTabListForRootId(rootId);
        getTabModel().closeTabs(TabClosureParams.closeTabs(tabs).allowUndo(false).build());

        // Remove mapping from service. Collect metrics before that.
        mTabGroupSyncService.removeLocalTabGroupMapping(tabGroupId, closingSource);
    }

    private List<Tab> findLocalTabsNotInSyncPostStartup(SavedTabGroup savedTabGroup) {
        assert savedTabGroup.localId != null;

        // We have been through startup reconcile earlier, so the tabs should have IDs mapped
        // already.
        // Find the ones that are not in sync. These are the ones that should be closed.
        Set<Integer> savedTabIds = new HashSet<>();
        for (SavedTabGroupTab savedTab : savedTabGroup.savedTabs) {
            if (savedTab.localId == null) continue;
            savedTabIds.add(savedTab.localId);
        }

        List<Tab> tabsNotInSync = new ArrayList<>();
        int rootId = TabGroupSyncUtils.getRootId(mTabGroupModelFilter, savedTabGroup.localId);
        for (Tab localTab : mTabGroupModelFilter.getRelatedTabListForRootId(rootId)) {
            if (!savedTabIds.contains(localTab.getId())) {
                tabsNotInSync.add(localTab);
            }
        }

        return tabsNotInSync;
    }

    private void maybeNavigateToUrl(Tab tab, GURL url, String title) {
        GURL localUrl = tab.getUrl();
        GURL syncUrl = url;

        // If the tab is already at the correct URL, don't do anything.
        if (localUrl.equals(syncUrl)) return;

        // If the tab has a non-syncable URL, don't override it if sync is trying to override it
        // with a default override. We allow local state to differ from sync in this case,
        // especially since we want to honor the local URL after restarts.
        boolean isLocalUrlSyncable = TabGroupSyncUtils.isSavableUrl(localUrl);
        if (!isLocalUrlSyncable && syncUrl.equals(TabGroupSyncUtils.UNSAVEABLE_URL_OVERRIDE)) {
            return;
        }

        if (TabGroupSyncUtils.isUrlInTabRedirectChain(tab, url)) {
            return;
        }

        boolean isCurrentTab =
                getTabModel().getCurrentTabSupplier().get() != null
                        && getTabModel().getCurrentTabSupplier().get().getId() == tab.getId();
        mTabCreationDelegate.navigateToUrl(tab, syncUrl, title, isCurrentTab);
    }

    private Tab getLocalTabInGroup(Integer tabId, int rootId) {
        Tab tab = tabId == null ? null : getTabModel().getTabById(tabId);
        // Check if the tab is still attached to the same root ID. If not, it belongs to another
        // group. Don't touch it and rather create a new one in subsequent step.
        return tab != null && tab.getRootId() == rootId ? tab : null;
    }

    private void updateTabGroupVisuals(SavedTabGroup tabGroup, int rootId) {
        mTabGroupModelFilter.setTabGroupTitle(rootId, tabGroup.title);
        mTabGroupModelFilter.setTabGroupColor(rootId, tabGroup.color);
    }

    private TabModel getTabModel() {
        return mTabGroupModelFilter.getTabModel();
    }
}

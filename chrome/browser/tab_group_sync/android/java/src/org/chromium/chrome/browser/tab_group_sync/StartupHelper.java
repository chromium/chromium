// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import android.text.TextUtils;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Handles startup flow. Invoked when both the {@link TabModel} and {@link TabGroupSyncService} have
 * been initialized. Creates in-memory mapping of the local and sync tab group IDs. Additionally, if
 * there are local only tab groups, adds them to sync.
 */
public class StartupHelper {
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final TabGroupSyncService mTabGroupSyncService;
    private final RemoteTabGroupMutationHelper mRemoteTabGroupMutationHelper;

    /**
     * Constructor.
     *
     * @param tabGroupModelFilter The local tab group model.
     * @param tabGroupSyncService The sync back end.
     * @param remoteTabGroupMutationHelper Helper to mutate remote tab groups based on local state.
     */
    public StartupHelper(
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupSyncService tabGroupSyncService,
            RemoteTabGroupMutationHelper remoteTabGroupMutationHelper) {
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupSyncService = tabGroupSyncService;
        mRemoteTabGroupMutationHelper = remoteTabGroupMutationHelper;
    }

    /**
     * Does startup routine. Few things:
     *
     * <ol>
     *   <li>Populate the in-memory mapping of the local tab group ID with sync ID. This will be
     *       stored in {@link TabGroupSyncService}.
     *   <li>For the first time migration, or if we signed in from a non-signed in state, we will
     *       have groups that don't exist in sync yet. This function will add these groups to sync.
     * </ol>
     */
    public void initializeTabGroupSync() {
        // Read mappings from pref and build an in-memory map.
        Map<String, Integer> idMappingsFromPref = readTabGroupIdMappingsFromPref();

        // Notify TabGroupSyncService about all the relevant local tab group IDs.
        notifyServiceOfIdMapping(idMappingsFromPref);

        // Do a one-time migration to local groups not in sync. Though unusual, it can happen
        // because of one of the following:
        // 1. They were created in pre-tabgroup-sync era.
        // 2. A crash happened after tab group creation but before notifying sync service.
        // In either case, create sync group counterparts for these groups.
        createRemoteTabGroupForNewGroups();
    }

    /** Reads all tab group ID mappings from prefs and returns a map of sync ID -> local ID. */
    private Map<String, Integer> readTabGroupIdMappingsFromPref() {
        // Find all local tab group IDs.
        Set<Integer> localTabGroups = new HashSet<>();
        for (int i = 0; i < getTabModel().getCount(); i++) {
            Tab tab = getTabModel().getTabAt(i);
            if (tab.getTabGroupId() == null) continue;
            localTabGroups.add(tab.getRootId());
        }

        // Read sync ID for the local groups and build a map.
        Map<String, Integer> idMappingsFromPref = new HashMap<>();
        for (int tabGroupId : localTabGroups) {
            String syncId = mTabGroupModelFilter.getTabGroupSyncId(tabGroupId);
            if (TextUtils.isEmpty(syncId)) continue;
            idMappingsFromPref.put(syncId, tabGroupId);
        }

        return idMappingsFromPref;
    }

    /**
     * For each group that has synced before, updates the local ID in the mapping held by {@link
     * TabGroupSyncService}.
     */
    private void notifyServiceOfIdMapping(Map<String, Integer> idMappingsFromPref) {
        String[] syncGroupIds = mTabGroupSyncService.getAllGroupIds();
        for (String syncGroupId : syncGroupIds) {
            Integer localGroupId = idMappingsFromPref.get(syncGroupId);
            if (localGroupId == null) continue;

            mTabGroupSyncService.updateLocalTabGroupId(syncGroupId, localGroupId);
        }
    }

    /**
     * If there are new groups that have never gotten a chance to get synced, this method will
     * create their sync counterparts.
     */
    private void createRemoteTabGroupForNewGroups() {
        Set<Integer> localTabGroups = new HashSet<>();
        for (int i = 0; i < getTabModel().getCount(); i++) {
            Tab tab = getTabModel().getTabAt(i);
            if (tab.getTabGroupId() == null) continue;
            localTabGroups.add(tab.getRootId());
        }

        for (int tabGroupId : localTabGroups) {
            String syncId = mTabGroupModelFilter.getTabGroupSyncId(tabGroupId);
            if (!TextUtils.isEmpty(syncId)) continue;

            mRemoteTabGroupMutationHelper.createRemoteTabGroup(tabGroupId);
        }
    }

    private TabModel getTabModel() {
        return mTabGroupModelFilter.getTabModel();
    }
}

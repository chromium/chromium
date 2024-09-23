// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.tab_group_sync.ClosingSource;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.HashSet;
import java.util.Set;

/**
 * Handles startup flow. Invoked when both the {@link TabModel} and {@link TabGroupSyncService} have
 * been initialized. Primarily reconciles remote group updates / deletions with the local model and
 * local group additions to remote. Also initializes tab ID mappings for the session.
 */
public class StartupHelper {
    private static final String TAG = "TG.StartupHelper";
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final TabGroupSyncService mTabGroupSyncService;
    private final LocalTabGroupMutationHelper mLocalTabGroupMutationHelper;
    private final RemoteTabGroupMutationHelper mRemoteTabGroupMutationHelper;

    /**
     * Constructor.
     *
     * @param tabGroupModelFilter The local tab group model.
     * @param tabGroupSyncService The sync back end.
     * @param localTabGroupMutationHelper Helper to mutate local tab groups based on remote state.
     * @param remoteTabGroupMutationHelper Helper to mutate remote tab groups based on local state.
     */
    public StartupHelper(
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupSyncService tabGroupSyncService,
            LocalTabGroupMutationHelper localTabGroupMutationHelper,
            RemoteTabGroupMutationHelper remoteTabGroupMutationHelper) {
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupSyncService = tabGroupSyncService;
        mLocalTabGroupMutationHelper = localTabGroupMutationHelper;
        mRemoteTabGroupMutationHelper = remoteTabGroupMutationHelper;
    }

    /**
     * The startup routine that is executed in order:
     *
     * <ol>
     *   <li>Delete any tab groups from tab model that were deleted from sync. It could happen in
     *       multi-window situations where the deletion event was received when the window wasn't
     *       alive.
     *   <li>Add any tab group to sync that doesn't exist yet in sync. This is meant to handle when
     *       tab group sync feature is turned on for the first time or after a rollback.
     *   <li>Reconcile local state to be same as sync. We could have lost a update event from sync
     *       while the window wasn't running.
     *   <li>Populate tab ID mapping for {@link TabGroupSyncService}. We only persist tab group ID
     *       mapping in storage. Tab IDs are mapped on startup.
     * </ol>
     */
    public void initializeTabGroupSync() {
        LogUtils.log(TAG, "initializeTabGroupSync");
        // First close the groups that were deleted remotely when the activity was not running.
        closeDeletedGroupsFromTabModel();

        // Add local groups that are not in sync. This can happen if:
        // 1. The group was created before tab group sync feature was enabled. More prevalent
        // if we are restoring a window created long back in history.
        // 2. A crash happened after group creation so that we couldn't write it to sync.
        createRemoteTabGroupForNewGroups();

        // Force update the local groups to be exactly same as sync. This accounts for any missing
        // updates from sync when the current window wasn't alive.
        reconcileGroupsToSync();

        // Connect the tab IDs to their sync counterpart. This is an in-memory mapping maintained
        // by {@link TabGroupSyncService}.
        updateTabIdMappings();
    }

    private void closeDeletedGroupsFromTabModel() {
        LogUtils.log(TAG, "closeDeletedGroupsFromTabModel");
        for (LocalTabGroupId tabGroupId : mTabGroupSyncService.getDeletedGroupIds()) {
            if (!TabGroupSyncUtils.isInCurrentWindow(mTabGroupModelFilter, tabGroupId)) continue;

            mLocalTabGroupMutationHelper.closeTabGroup(
                    tabGroupId, ClosingSource.CLEANED_UP_ON_STARTUP);
        }
    }

    /**
     * If there are new groups that have never gotten a chance to get synced, this method will
     * create their sync counterparts.
     */
    private void createRemoteTabGroupForNewGroups() {
        LogUtils.log(TAG, "createRemoteTabGroupForNewGroups");
        for (LocalTabGroupId tabGroupId : getLocalTabGroupIds()) {
            // Skip if the group is already added to sync.
            SavedTabGroup savedTabGroup = mTabGroupSyncService.getGroup(tabGroupId);
            if (savedTabGroup != null) continue;

            // Skip if the group is ineligible for syncing, e.g. hasn't been accessed in recent
            // times.
            if (!TabGroupSyncUtils.isTabGroupEligibleForSyncing(tabGroupId, mTabGroupModelFilter)) {
                LogUtils.log(TAG, "Skipping the tab group as it is too old");
                return;
            }

            mRemoteTabGroupMutationHelper.createRemoteTabGroup(tabGroupId);
        }
    }

    private void reconcileGroupsToSync() {
        LogUtils.log(TAG, "reconcileGroupsToSync");
        for (LocalTabGroupId tabGroupId : getLocalTabGroupIds()) {
            SavedTabGroup savedTabGroup = mTabGroupSyncService.getGroup(tabGroupId);
            // At this point every tab group should be already in sync unless they are too old.
            if (savedTabGroup == null) continue;
            mLocalTabGroupMutationHelper.reconcileGroupOnStartup(savedTabGroup);
        }
    }

    /**
     * For each group that has synced before, updates the tab ID mappings held by {@link
     * TabGroupSyncService}.
     */
    private void updateTabIdMappings() {
        LogUtils.log(TAG, "updateTabIdMappings");
        for (LocalTabGroupId tabGroupId : getLocalTabGroupIds()) {
            mRemoteTabGroupMutationHelper.updateTabIdMappingsOnStartup(tabGroupId);
        }
    }

    private Set<LocalTabGroupId> getLocalTabGroupIds() {
        Set<LocalTabGroupId> localTabGroups = new HashSet<>();
        for (int i = 0; i < getTabModel().getCount(); i++) {
            Tab tab = getTabModel().getTabAt(i);
            if (tab.getTabGroupId() == null) continue;
            localTabGroups.add(TabGroupSyncUtils.getLocalTabGroupId(tab));
        }
        return localTabGroups;
    }

    private TabModel getTabModel() {
        return mTabGroupModelFilter.getTabModel();
    }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.tab_group_sync.ClosingSource;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Handles startup flow. Invoked when both the {@link TabModel} and {@link TabGroupSyncService} have
 * been initialized. Primarily reconciles remote group updates / deletions with the local model and
 * local group additions to remote. Also initializes tab ID mappings for the session.
 */
@NullMarked
public class StartupHelper {
    private static final String TAG = "TG.StartupHelper";
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final TabGroupSyncService mTabGroupSyncService;
    private final LocalTabGroupMutationHelper mLocalTabGroupMutationHelper;
    private final RemoteTabGroupMutationHelper mRemoteTabGroupMutationHelper;
    private final PrefService mPrefService;

    /**
     * Constructor.
     *
     * @param tabGroupModelFilter The local tab group model.
     * @param tabGroupSyncService The sync back end.
     * @param localTabGroupMutationHelper Helper to mutate local tab groups based on remote state.
     * @param remoteTabGroupMutationHelper Helper to mutate remote tab groups based on local state.
     * @param prefService Pref service for checking tab group sync migration status in past.
     */
    public StartupHelper(
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupSyncService tabGroupSyncService,
            LocalTabGroupMutationHelper localTabGroupMutationHelper,
            RemoteTabGroupMutationHelper remoteTabGroupMutationHelper,
            PrefService prefService) {
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupSyncService = tabGroupSyncService;
        mLocalTabGroupMutationHelper = localTabGroupMutationHelper;
        mRemoteTabGroupMutationHelper = remoteTabGroupMutationHelper;
        mPrefService = prefService;
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

        // Handle any local tab groups that don't exist in sync. They will be either closed or added
        // back to sync depending on whether this is the first time the sync feature is being
        // enabled.
        handleUnsavedLocalTabGroups();

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
     * If there are unsaved local groups that don't have a corresponding sync entry, resolve them
     * now.
     */
    private void handleUnsavedLocalTabGroups() {
        LogUtils.log(TAG, "handleUnsavedLocalTabGroups");

        // Find the local tab groups that don't have a corresponding saved tab group.
        List<LocalTabGroupId> tabGroupsNotKnownToSync = new ArrayList<>();
        for (LocalTabGroupId tabGroupId : getLocalTabGroupIds()) {
            SavedTabGroup savedTabGroup = mTabGroupSyncService.getGroup(tabGroupId);
            if (savedTabGroup == null) {
                tabGroupsNotKnownToSync.add(tabGroupId);
            }
        }

        boolean didSyncTabGroupsInLastSession =
                mPrefService.getBoolean(Pref.DID_SYNC_TAB_GROUPS_IN_LAST_SESSION);
        mPrefService.setBoolean(Pref.DID_SYNC_TAB_GROUPS_IN_LAST_SESSION, true);
        for (LocalTabGroupId tabGroupId : tabGroupsNotKnownToSync) {
            if (didSyncTabGroupsInLastSession) {
                // This is an unexpected local tab group as all the groups should have been saved to
                // sync DB already. Close it.
                mLocalTabGroupMutationHelper.closeTabGroup(
                        tabGroupId, ClosingSource.CLEANED_UP_ON_STARTUP);
            } else {
                // This is the first time feature launch for tab group sync. Add the group to sync
                // DB.
                mRemoteTabGroupMutationHelper.createRemoteTabGroup(tabGroupId);
            }
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
            Tab tab = getTabModel().getTabAtChecked(i);
            LocalTabGroupId localTabGroupId = TabGroupSyncUtils.getLocalTabGroupId(tab);
            if (localTabGroupId == null) continue;
            localTabGroups.add(localTabGroupId);
        }
        return localTabGroups;
    }

    private TabModel getTabModel() {
        return mTabGroupModelFilter.getTabModel();
    }
}

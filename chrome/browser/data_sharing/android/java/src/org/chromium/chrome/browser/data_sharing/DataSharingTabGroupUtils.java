// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

/** Utilities related to tab groups in data sharing. */
public class DataSharingTabGroupUtils {
    /** A holder for list of tab groups pending destruction. */
    public static class GroupsPendingDestroy {
        /**
         * The list of synced groups that will be destroyed. This excludes entries with
         * collaboration IDs.
         */
        public final List<LocalTabGroupId> syncedGroupsDestroyed = new ArrayList<>();

        /**
         * The list of synced groups with collaborations that will be destroyed. Entries in this
         * list will not be in syncedGroupsDestroyed.
         */
        public final List<LocalTabGroupId> collaborationGroupsDestroyed = new ArrayList<>();

        /** Returns if there are no groups that would be destroyed. */
        public boolean isEmpty() {
            return syncedGroupsDestroyed.isEmpty() && collaborationGroupsDestroyed.isEmpty();
        }
    }

    /**
     * Returns lists of local tab group IDs in sync that closing or ungrouping the tabs would
     * destroy.
     *
     * @param tabModel The tab model to close or ungroup tabs in.
     * @param tabsToRemove The list of tabs to remove.
     * @return lists of the local tab group IDs that would have collaborations or sync data
     *     destroyed.
     */
    public static @NonNull GroupsPendingDestroy getSyncedGroupsDestroyedByTabRemoval(
            @NonNull TabModel tabModel, @Nullable List<Tab> tabsToRemove) {
        // TODO(crbug.com/345854441): Add feature flag checks.

        GroupsPendingDestroy destroyedGroups = new GroupsPendingDestroy();

        // Collaborations are not possible in incognito branded mode.
        if (tabsToRemove == null || tabsToRemove.isEmpty() || tabModel.isIncognitoBranded()) {
            return destroyedGroups;
        }

        @Nullable
        TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(tabModel.getProfile());
        if (tabGroupSyncService == null) {
            return destroyedGroups;
        }

        for (String syncId : tabGroupSyncService.getAllGroupIds()) {
            SavedTabGroup group = tabGroupSyncService.getGroup(syncId);

            // Tab groups without a local representation won't have local tabs that are being
            // removed and can be skipped.
            if (group.localId == null) continue;

            boolean isCollaboration = !TextUtils.isEmpty(group.collaborationId);
            if (willRemoveAllTabsInGroup(tabModel, group.savedTabs, tabsToRemove)) {
                if (isCollaboration) {
                    destroyedGroups.collaborationGroupsDestroyed.add(group.localId);
                } else {
                    destroyedGroups.syncedGroupsDestroyed.add(group.localId);
                }
            }
        }
        return destroyedGroups;
    }

    /**
     * Returns lists of local tab group IDs in sync that closing the tabs described by the closure
     * params would destroy.
     *
     * @param tabModel The tab model to close tabs in.
     * @param closureParams The params that would be used to close tabs.
     * @return lists of the local tab group IDs that would have collaborations or sync data
     *     destroyed.
     */
    public static @NonNull GroupsPendingDestroy getSyncedGroupsDestroyedByTabClosure(
            @NonNull TabModel tabModel, @NonNull TabClosureParams closureParams) {
        // If tab groups are being hidden then they cannot be destroyed.
        if (closureParams.hideTabGroups) return new GroupsPendingDestroy();

        @Nullable
        List<Tab> tabsToClose =
                closureParams.isAllTabs
                        ? TabModelUtils.convertTabListToListOfTabs(tabModel)
                        : closureParams.tabs;
        return getSyncedGroupsDestroyedByTabRemoval(tabModel, tabsToClose);
    }

    /**
     * Given a set of local tab group IDs create placeholder tabs in those tab groups to protect the
     * collaborations from being deleted.
     *
     * @param tabModel The tab model to close tabs in.
     * @param localTabGroupIds The list of tab group IDs to add tabs to.
     * @return A list of tabs that were created.
     */
    public static @NonNull List<Tab> createPlaceholderTabInGroups(
            TabModel tabModel, @Nullable List<LocalTabGroupId> localTabGroupIds) {
        // This functionality is not supported in incognito mode.
        if (localTabGroupIds == null
                || localTabGroupIds.isEmpty()
                || tabModel.isIncognitoBranded()) {
            return Collections.emptyList();
        }

        Set<Token> tabGroupIds =
                localTabGroupIds.stream()
                        .map(localTabGroupId -> localTabGroupId.tabGroupId)
                        .collect(Collectors.toSet());
        HashMap<Token, Tab> parentTabMap = new HashMap<>();
        for (int i = 0; i < tabModel.getCount(); i++) {
            Tab tab = tabModel.getTabAt(i);
            @Nullable Token tabGroupId = tab.getTabGroupId();

            // The parent tab should be the last tab in the tab group. Tab groups are contiguous.
            if (tabGroupId != null && tabGroupIds.contains(tabGroupId)) {
                parentTabMap.put(tabGroupId, tab);
            } else if (parentTabMap.size() == localTabGroupIds.size()) {
                // Every tab group now has a parent tab and since groups are contiguous we can early
                // exit.
                break;
            }
        }

        TabCreator tabCreator = tabModel.getTabCreator();
        List<Tab> newTabs = new ArrayList<>();
        for (Tab parentTab : parentTabMap.values()) {
            // The tab will automatically be placed immediately after the parent and this launch
            // type ensures the tab is added to the tab group.
            Tab newTab =
                    tabCreator.createNewTab(
                            new LoadUrlParams(UrlConstants.NTP_URL),
                            TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP,
                            parentTab);
            newTabs.add(newTab);
        }
        return newTabs;
    }

    private static boolean willRemoveAllTabsInGroup(
            TabModel tabModel, List<SavedTabGroupTab> savedTabs, List<Tab> tabsToRemove) {
        for (SavedTabGroupTab savedTab : savedTabs) {
            // First check that we have local IDs for the tab. It is possible that we don't if the
            // tab group is open in another window that hasn't been foregrounded yet as the tabs are
            // loaded lazily and so won't be tracked yet. If this happens we won't destroy the
            // collaboration as the tabs cannot be removed.
            if (savedTab.localId == null) {
                return false;
            }

            // If the saved tab has a local id, but it is not in the current tab model it is either
            // currently closing or in another window. In either case, the tabsToRemove belong to
            // the tabModel so we can skip this tab as it is either closing or not relevant.
            int localId = savedTab.localId;
            if (tabModel.getTabById(localId) == null) {
                continue;
            }

            // If any of the tabs in the saved group are missing from the list of tabsToRemove we
            // can assume the collaboration will not be destroyed and early out. This check is
            // technically O(n^2) if every group is a collaboration and all tabs are closing. We
            // could optimize this with sets, but then the average case performance is likely to
            // be worse as realistically very few entries will be shared. We can revisit this if we
            // start seeing ANRs or other issues.
            if (savedTab.localId == null
                    || !tabsToRemove.stream()
                            .filter(tab -> tab.getId() == savedTab.localId)
                            .findFirst()
                            .isPresent()) {
                return false;
            }
        }
        return true;
    }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.LoadUrlParams;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Utilities related to tab groups in data sharing. */
@NullMarked
public class DataSharingTabGroupUtils {
    @IntDef({
        TabPresence.IN_WINDOW,
        TabPresence.IN_WINDOW_CLOSING,
        TabPresence.NOT_IN_WINDOW,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface TabPresence {
        int IN_WINDOW = 0;
        int IN_WINDOW_CLOSING = 1;
        int NOT_IN_WINDOW = 2;
    }

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
    public static GroupsPendingDestroy getSyncedGroupsDestroyedByTabRemoval(
            TabModel tabModel, @Nullable List<Tab> tabsToRemove) {
        GroupsPendingDestroy destroyedGroups = new GroupsPendingDestroy();

        // Collaborations are not possible in incognito branded mode.
        if (tabsToRemove == null || tabsToRemove.isEmpty() || tabModel.isIncognitoBranded()) {
            return destroyedGroups;
        }

        @Nullable TabGroupSyncService tabGroupSyncService =
                TabGroupSyncServiceFactory.getForProfile(assumeNonNull(tabModel.getProfile()));
        if (tabGroupSyncService == null || !tabGroupSyncService.isObservingLocalChanges()) {
            return destroyedGroups;
        }

        String[] syncIds = tabGroupSyncService.getAllGroupIds();
        // This may be null in tests.
        if (syncIds == null) return destroyedGroups;

        for (String syncId : syncIds) {
            SavedTabGroup group = tabGroupSyncService.getGroup(syncId);
            assumeNonNull(group);

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
    public static GroupsPendingDestroy getSyncedGroupsDestroyedByTabClosure(
            TabModel tabModel, TabClosureParams closureParams) {
        // If tab groups are being hidden then they cannot be destroyed.
        if (closureParams.hideTabGroups) return new GroupsPendingDestroy();

        @Nullable List<Tab> tabsToClose =
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
    public static List<Tab> createPlaceholderTabInGroups(
            TabModel tabModel, @Nullable List<LocalTabGroupId> localTabGroupIds) {
        // This functionality is not supported in incognito mode.
        if (localTabGroupIds == null
                || localTabGroupIds.isEmpty()
                || tabModel.isIncognitoBranded()) {
            return Collections.emptyList();
        }

        Set<Token> tabGroupIds = new HashSet<>();
        for (LocalTabGroupId localTabGroupId : localTabGroupIds) {
            tabGroupIds.add(localTabGroupId.tabGroupId);
        }
        HashMap<Token, Tab> parentTabMap = new HashMap<>();
        for (Tab tab : tabModel) {
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
            UrlConstantResolver urlConstantResolver =
                    UrlConstantResolverFactory.getForProfile(tabModel.getProfile());
            // The tab will automatically be placed immediately after the parent and this launch
            // type ensures the tab is added to the tab group.
            Tab newTab =
                    tabCreator.createNewTab(
                            new LoadUrlParams(urlConstantResolver.getNtpUrl()),
                            TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP,
                            parentTab);
            newTabs.add(newTab);
        }
        return newTabs;
    }

    private static boolean willRemoveAllTabsInGroup(
            TabModel tabModel, List<SavedTabGroupTab> savedTabs, List<Tab> tabsToRemove) {
        boolean areAllAlreadyClosing = true;
        for (SavedTabGroupTab savedTab : savedTabs) {
            // First check that we have local IDs for the tab. It is possible that we don't if the
            // tab group is open in another window that hasn't been foregrounded yet as the tabs are
            // loaded lazily and so won't be tracked yet. If this happens we won't destroy the
            // collaboration as the tabs cannot be removed.
            if (savedTab.localId == null) {
                return false;
            }

            // If the saved tab has a local id, but it is not in the current tab model it is either
            // currently closing or in another window.
            int localTabId = savedTab.localId;
            switch (getTabPresence(tabModel, localTabId)) {
                case TabPresence.IN_WINDOW:
                    // Intentional no-op.
                    areAllAlreadyClosing = false;
                    break;
                case TabPresence.IN_WINDOW_CLOSING:
                    // If the tab is closing we should keep checking since all the rest of the tabs
                    // in the group might also be closing as part of tabsToRemove.
                    continue;
                case TabPresence.NOT_IN_WINDOW:
                    // If the tab is just missing from the model entirely we can assume the group is
                    // not present in this window since all tabs in a group must be in one window.
                    return false;
                default:
                    assert false : "Not reached.";
            }

            // If any of the tabs in the saved group are missing from the list of tabsToRemove we
            // can assume the collaboration will not be destroyed and early out. This check is
            // technically O(n^2) if every group is a collaboration and all tabs are closing. We
            // could optimize this with sets, but then the average case performance is likely to
            // be worse as realistically very few entries will be shared. We can revisit this if we
            // start seeing ANRs or other issues.
            if (!tabsToRemoveContains(tabsToRemove, localTabId)) {
                return false;
            }
        }
        // If all the tabs in the group are already closing showing a dialog does not make sense.
        return !areAllAlreadyClosing;
    }

    private static @TabPresence int getTabPresence(TabModel tabModel, int tabId) {
        TabList tabList = tabModel.getComprehensiveModel();
        for (Tab tab : tabList) {
            assumeNonNull(tab);
            if (tab.getId() == tabId) {
                return tab.isClosing() ? TabPresence.IN_WINDOW_CLOSING : TabPresence.IN_WINDOW;
            }
        }
        return TabPresence.NOT_IN_WINDOW;
    }

    private static boolean tabsToRemoveContains(List<Tab> tabsToRemove, int tabId) {
        for (Tab tab : tabsToRemove) {
            if (tab.getId() == tabId) return true;
        }
        return false;
    }

    /**
     * @param collaborationId The sharing ID associated with the group.
     * @param tabGroupSyncService The sync service to get tab group data form.
     * @return The {@link SavedTabGroup} from sync service.
     */
    public @Nullable static SavedTabGroup getTabGroupForCollabIdFromSync(
            String collaborationId, TabGroupSyncService tabGroupSyncService) {
        for (String syncGroupId : tabGroupSyncService.getAllGroupIds()) {
            SavedTabGroup savedTabGroup = tabGroupSyncService.getGroup(syncGroupId);
            assumeNonNull(savedTabGroup);
            assert !savedTabGroup.savedTabs.isEmpty();
            if (savedTabGroup.collaborationId != null
                    && savedTabGroup.collaborationId.equals(collaborationId)) {
                return savedTabGroup;
            }
        }
        return null;
    }

    /**
     * @param context The activity context.
     * @param collaborationId The sharing ID associated with the group.
     * @param tabGroupSyncService The sync service to get tab group data form.
     * @return The title of the tab group.
     */
    public static @Nullable String getTabGroupTitle(
            Context context, String collaborationId, TabGroupSyncService tabGroupSyncService) {
        SavedTabGroup tabGroup =
                getTabGroupForCollabIdFromSync(collaborationId, tabGroupSyncService);
        if (tabGroup == null) {
            return null;
        }
        return TextUtils.isEmpty(tabGroup.title)
                ? TabGroupTitleUtils.getDefaultTitle(context, tabGroup.savedTabs.size())
                : tabGroup.title;
    }
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.StringRes;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils.TabMovedCallback;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/** Helper methods for Tab Group UI components and string resources. */
@NullMarked
public class TabGroupUiUtils {

    /** Returns whether cross-window tab group operations are enabled. */
    public static boolean isCrossWindowTabGroupOperationsEnabled() {
        return ChromeFeatureList.sCrossWindowTabGroupOperations.isEnabled();
    }

    /** Returns whether remote group operations are enabled. */
    public static boolean isRemoteGroupOperationsEnabled() {
        return isCrossWindowTabGroupOperationsEnabled()
                && ChromeFeatureList.sCrossWindowTabGroupOperationsRemoteGroupOperations.getValue();
    }

    /**
     * Returns the string resource ID for the 'add to group' menu item ("Add tab to group" vs "Add
     * tab to new group" vs "Move tab to group").
     *
     * @param currentTabGroupId The tab group ID of the current tab if already in a group, or null.
     * @param hasTabGroups Whether any tab groups exist.
     */
    public static @StringRes int getAddToGroupMenuItemString(
            @Nullable Token currentTabGroupId, boolean hasTabGroups) {
        if (currentTabGroupId != null) {
            return R.string.menu_move_tab_to_group;
        }
        return hasTabGroups ? R.string.menu_add_tab_to_group : R.string.menu_add_tab_to_new_group;
    }

    /**
     * Returns the string title for adding/moving tab(s) to a tab group.
     *
     * @param context The current context.
     * @param currentGroupId The group ID of the current tab group, or null if outside a group.
     * @param tabCount The number of tabs to add or move.
     * @return The string title for the menu item.
     */
    public static String getAddToGroupMenuItemTitle(
            Context context, @Nullable Token currentGroupId, int tabCount) {
        if (currentGroupId != null) {
            return context.getString(R.string.menu_move_tab_to_group);
        }
        return context.getResources()
                .getQuantityString(R.plurals.add_tab_to_group_menu_item, tabCount);
    }

    /**
     * Resolves the {@link GroupWindowInfo} for a group given a local tab group ID or a sync group
     * ID.
     *
     * @param context The current context.
     * @param tabModel The current tab model.
     * @param syncService The tab group sync service, or null.
     * @param groupId The local tab group ID, or null.
     * @param syncGroupId The sync tab group ID, or null.
     * @return The resolved {@link GroupWindowInfo}, or null if neither ID was resolvable.
     */
    public static @Nullable GroupWindowInfo getGroupWindowInfo(
            Context context,
            TabModel tabModel,
            @Nullable TabGroupSyncService syncService,
            @Nullable Token groupId,
            @Nullable String syncGroupId) {
        GroupWindowChecker checker = new GroupWindowChecker(context, syncService, tabModel);
        if (groupId != null) {
            return GroupWindowInfo.forLocalGroup(
                    context, tabModel, groupId, checker.getState(groupId));
        } else if (syncGroupId != null && syncService != null) {
            SavedTabGroup group = syncService.getGroup(syncGroupId);
            if (group != null) {
                return GroupWindowInfo.forSyncedGroup(context, group, checker.getState(group));
            }
        }
        return null;
    }

    private static boolean isRemoteGroup(GroupWindowInfo group) {
        return group.groupWindowState == GroupWindowState.HIDDEN || group.localId == null;
    }

    /**
     * Determines whether tabs can be added to the destination group based on feature flags, group
     * window state, and required service dependencies.
     *
     * @param destinationGroup The target group information.
     * @param syncService The sync service required to resolve restored remote groups.
     * @param uiActionHandler The UI handler required to restore remote groups.
     * @return True if tabs can be added to the destination group, false otherwise.
     */
    public static boolean isValidDestination(
            @Nullable GroupWindowInfo destinationGroup,
            @Nullable TabGroupSyncService syncService,
            @Nullable TabGroupUiActionHandler uiActionHandler) {
        if (destinationGroup == null) {
            return false;
        }
        if (destinationGroup.groupWindowState == GroupWindowState.IN_CURRENT_CLOSING) {
            return false;
        }
        if (destinationGroup.groupWindowState == GroupWindowState.IN_ANOTHER) {
            return isCrossWindowTabGroupOperationsEnabled() && destinationGroup.localId != null;
        }
        if (isRemoteGroup(destinationGroup)) {
            return isRemoteGroupOperationsEnabled()
                    && destinationGroup.syncId != null
                    && syncService != null
                    && uiActionHandler != null;
        }
        return destinationGroup.localId != null;
    }

    /**
     * Adds the given tabs to the destination tab group. Handles local tab group merge within the
     * same window, cross-window move to another window, and restoring remote tab groups.
     *
     * @param sourceTabModel The source {@link TabModel}.
     * @param tabs The list of {@link Tab}s to add to the group.
     * @param destinationGroup The {@link GroupWindowInfo} representing the target tab group.
     * @param syncService The sync service used to look up restored tab groups.
     * @param uiActionHandler The UI action handler used to restore remote tab groups.
     * @param tabMovedCallback Optional callback invoked when tabs are moved.
     * @param bringToFront Whether to bring the destination window to the front if cross-window.
     */
    public static void addTabsToGroup(
            TabModel sourceTabModel,
            List<Tab> tabs,
            @Nullable GroupWindowInfo destinationGroup,
            @Nullable TabGroupSyncService syncService,
            @Nullable TabGroupUiActionHandler uiActionHandler,
            @Nullable TabMovedCallback tabMovedCallback,
            boolean bringToFront) {
        if (tabs == null || tabs.isEmpty()) {
            return;
        }
        if (destinationGroup == null
                || !isValidDestination(destinationGroup, syncService, uiActionHandler)) {
            return;
        }
        Token destinationGroupId = destinationGroup.localId;
        if (destinationGroupId != null && areTabsAlreadyInGroup(tabs, destinationGroupId)) {
            return;
        }
        if (isRemoteGroup(destinationGroup)) {
            assert destinationGroup.syncId != null;
            assert syncService != null;
            assert uiActionHandler != null;

            String syncId = destinationGroup.syncId;
            uiActionHandler.openTabGroup(syncId);
            SavedTabGroup savedGroup = syncService.getGroup(syncId);
            if (savedGroup == null || savedGroup.localId == null) {
                return;
            }
            destinationGroupId = savedGroup.localId.tabGroupId;
        }
        if (destinationGroupId == null || areTabsAlreadyInGroup(tabs, destinationGroupId)) {
            return;
        }

        if (sourceTabModel.tabGroupExists(destinationGroupId)) {
            @TabId int destTabId = sourceTabModel.getGroupLastShownTabId(destinationGroupId);
            TabGroupUtils.mergeTabsToDest(tabs, destTabId, sourceTabModel, tabMovedCallback);
            return;
        }

        if (isCrossWindowTabGroupOperationsEnabled()) {
            TabWindowManager windowManager = TabWindowManagerSingleton.getInstance();
            if (windowManager != null) {
                int windowId = windowManager.findWindowIdForTabGroup(destinationGroupId);
                if (windowId != TabWindowManager.INVALID_WINDOW_ID) {
                    TabModelSelector selector = windowManager.getTabModelSelectorById(windowId);
                    if (selector != null) {
                        TabModel destTabModel = selector.getModel(sourceTabModel.isIncognito());
                        @TabId
                        int destTabId = destTabModel.getGroupLastShownTabId(destinationGroupId);
                        maybeUngroupTabs(sourceTabModel, tabs);
                        MultiInstanceOrchestratorFactory.getInstance()
                                .moveTabsToWindowByIdChecked(
                                        windowId,
                                        tabs,
                                        TabList.INVALID_TAB_INDEX,
                                        destTabId,
                                        bringToFront);
                        if (tabMovedCallback != null) {
                            tabMovedCallback.onTabMoved();
                        }
                    }
                }
            }
        }
    }

    private static void maybeUngroupTabs(TabModel tabModel, List<Tab> tabs) {
        List<Tab> groupedTabs = new ArrayList<>();
        for (Tab tab : tabs) {
            if (tabModel.isTabInTabGroup(tab)) {
                groupedTabs.add(tab);
            }
        }
        if (!groupedTabs.isEmpty()) {
            tabModel.getTabUngrouper()
                    .ungroupTabs(groupedTabs, /* trailing= */ true, /* allowDialog= */ false);
        }
    }

    private static boolean areTabsAlreadyInGroup(List<Tab> tabs, Token destinationGroupId) {
        boolean areTabsAlreadyInGroup = true;
        for (Tab tab : tabs) {
            areTabsAlreadyInGroup &= Objects.equals(destinationGroupId, tab.getTabGroupId());
        }
        return areTabsAlreadyInGroup;
    }
}

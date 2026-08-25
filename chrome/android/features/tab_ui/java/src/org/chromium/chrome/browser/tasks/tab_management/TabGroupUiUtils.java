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

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

/** Helper methods for Tab Group UI components and string resources. */
@NullMarked
public class TabGroupUiUtils {

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
     * Returns the string resource ID for the 'add to group' menu item ("Add tab to group" vs "Add
     * tab to new group" vs "Move tab to group").
     *
     * @param tabModel The current {@link TabModel}.
     * @param currentTabGroupId The tab group ID of the current tab if already in a group, or null.
     */
    public static @StringRes int getAddToGroupMenuItemString(
            @Nullable TabModel tabModel, @Nullable Token currentTabGroupId) {
        if (currentTabGroupId != null) {
            return R.string.menu_move_tab_to_group;
        }
        Collection<TabModelSelector> selectors =
                ChromeFeatureList.sCrossWindowTabGroupOperations.isEnabled()
                        ? TabWindowManagerSingleton.getInstance().getAllTabModelSelectors()
                        : Collections.emptyList();
        return getAddToGroupMenuItemString(
                currentTabGroupId, TabGroupUtils.hasTabGroups(tabModel, selectors));
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
     * Adds the given tabs to the destination tab group. Handles both local tab group merge within
     * the same window and cross-window move to another window.
     *
     * @param sourceTabModel The source {@link TabModel}.
     * @param tabs The list of {@link Tab}s to add to the group.
     * @param destinationGroupId The ID of the target tab group.
     * @param tabMovedCallback Optional callback invoked when tabs are moved.
     * @param bringToFront Whether to bring the destination window to the front if cross-window.
     */
    public static void addTabsToGroup(
            TabModel sourceTabModel,
            List<Tab> tabs,
            Token destinationGroupId,
            @Nullable TabMovedCallback tabMovedCallback,
            boolean bringToFront) {
        if (tabs.isEmpty() || areTabsAlreadyInGroup(tabs, destinationGroupId)) {
            return;
        }

        if (sourceTabModel.tabGroupExists(destinationGroupId)) {
            @TabId int destTabId = sourceTabModel.getGroupLastShownTabId(destinationGroupId);
            TabGroupUtils.mergeTabsToDest(tabs, destTabId, sourceTabModel, tabMovedCallback);
            return;
        }

        if (ChromeFeatureList.sCrossWindowTabGroupOperations.isEnabled()) {
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

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.text.TextUtils;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;

/** Helper class to handle tab groups related utilities. */
@NullMarked
public class TabGroupUtils {
    /**
     * This method gets the selected tab of the group where {@code tab} is in.
     *
     * @param filter The filter that owns the {@code tab}.
     * @param tab The {@link Tab}.
     * @return The selected tab of the group which contains the {@code tab}
     */
    public static Tab getSelectedTabInGroupForTab(TabGroupModelFilter filter, Tab tab) {
        return assumeNonNull(filter.getRepresentativeTabAt(filter.representativeIndexOf(tab)));
    }

    /**
     * This method gets the index in TabModel of the first tab in {@code tabs}.
     *
     * @param tabModel The tabModel that owns the {@code tab}.
     * @param tabs The list of tabs among which we need to find the first tab index.
     * @return The index in TabModel of the first tab in {@code tabs}
     */
    public static int getFirstTabModelIndexForList(TabModel tabModel, List<Tab> tabs) {
        assert tabs != null && tabs.size() != 0;

        return tabModel.indexOf(tabs.get(0));
    }

    /**
     * This method gets the index in TabModel of the last tab in {@code tabs}.
     *
     * @param tabModel The tabModel that owns the {@code tab}.
     * @param tabs The list of tabs among which we need to find the last tab index.
     * @return The index in TabModel of the last tab in {@code tabs}
     */
    public static int getLastTabModelIndexForList(TabModel tabModel, List<Tab> tabs) {
        assert tabs != null && tabs.size() != 0;

        return tabModel.indexOf(tabs.get(tabs.size() - 1));
    }

    /**
     * Opens a new tab in the last position of the tab group and selects it.
     *
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} to act on.
     * @param url The url to load the new tab with.
     * @param parentId The ID of one of the tabs in the tab group.
     * @param type The launch type of the new tab.
     */
    public static void openUrlInGroup(
            TabGroupModelFilter tabGroupModelFilter,
            String url,
            int parentId,
            @TabLaunchType int type) {
        List<Tab> relatedTabs = tabGroupModelFilter.getRelatedTabList(parentId);
        if (relatedTabs.isEmpty()) return;

        Tab lastTab = relatedTabs.get(relatedTabs.size() - 1);
        TabCreator tabCreator = tabGroupModelFilter.getTabModel().getTabCreator();
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        tabCreator.createNewTab(loadUrlParams, type, lastTab);
    }

    /**
     * Regroups the provided list of tabs into a tab group using the given {@link TabGroupMetadata}.
     *
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} to act on.
     * @param tabs The list of tabs to be merged to a group.
     * @param tabGroupMetadata The metadata used to regrouped the tabs.
     * @param shouldApplyCollapse Whether to apply the collapsed state.
     */
    public static void regroupTabs(
            TabGroupModelFilter tabGroupModelFilter,
            List<Tab> tabs,
            TabGroupMetadata tabGroupMetadata,
            boolean shouldApplyCollapse) {
        // 1. Extract tab group properties from the metadata.
        Token tabGroupId = tabGroupMetadata.tabGroupId;
        String tabGroupTitle = tabGroupMetadata.tabGroupTitle;
        boolean tabGroupCollapsed = tabGroupMetadata.tabGroupCollapsed;
        int tabGroupColor = tabGroupMetadata.tabGroupColor;

        // 2. Create the local tab group in the current TabGroupModelFilter.
        tabGroupModelFilter.createTabGroupForTabGroupSync(tabs, tabGroupId);
        int newRootId = tabs.get(0).getRootId();

        // 3. Apply the tab group attributes (color, collapsed state, and title) using the new
        // rootId.
        tabGroupModelFilter.setTabGroupColor(newRootId, tabGroupColor);
        tabGroupModelFilter.setTabGroupTitle(newRootId, tabGroupTitle);
        if (shouldApplyCollapse) {
            tabGroupModelFilter.setTabGroupCollapsed(
                    newRootId, tabGroupCollapsed, /* animate= */ false);
        }
    }

    /**
     * Checks to see if any tab in a list of tabs is in a tab group.
     *
     * @param tabModel The {@link TabModel} that owns the tabs.
     * @param tabs The list of tabs to be checked.
     * @param destGroupId The group id of the destination . If not null, then tabs with the same
     *     group will be allowed.
     */
    public static boolean areAnyTabsPartOfSharedGroup(
            TabModel tabModel, List<Tab> tabs, @Nullable Token destGroupId) {
        Profile profile = tabModel.getProfile();
        if (profile == null
                || profile.isOffTheRecord()
                || !TabGroupSyncFeatures.isTabGroupSyncEnabled(profile)) {
            return false;
        }
        TabGroupSyncService tabGroupSyncService =
                assumeNonNull(TabGroupSyncServiceFactory.getForProfile(profile));

        Set<Token> visitedGroups = new HashSet<>();
        for (Tab tab : tabs) {
            Token groupId = tab.getTabGroupId();
            if (groupId == null
                    || Objects.equals(groupId, destGroupId)
                    || visitedGroups.contains(groupId)) {
                continue;
            }
            visitedGroups.add(groupId);

            LocalTabGroupId localTabGroupId = new LocalTabGroupId(groupId);
            SavedTabGroup savedTabGroup = tabGroupSyncService.getGroup(localTabGroupId);
            if (savedTabGroup == null) continue;

            @Nullable String collaborationId = savedTabGroup.collaborationId;
            if (!TextUtils.isEmpty(collaborationId)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Returns whether the collapsed state should be applied when a group is dropped into another
     * window. If the tab strip is hidden or no tab is selected, this skips collapsing and calls
     * {@link TabModelUtils#setIndex} to ensure a tab in the group is selected.
     *
     * @param tabModel The tab model associated with the dropped group.
     * @param isCollapsed Whether the group was previously collapsed.
     * @param isTabStripVisible Whether the tablet tab strip is visible.
     * @param dropIndex The index representing the group position after drop.
     * @return {@code true} if the collapsed state should be applied, {@code false} otherwise.
     */
    public static boolean shouldApplyCollapsedState(
            TabModel tabModel, boolean isCollapsed, boolean isTabStripVisible, int dropIndex) {
        if (!isCollapsed) return true;
        if (!isTabStripVisible) {
            // If the tab strip is hidden due to window size, skip collapsing and select the first
            // tab to avoid the group appearing lost.
            TabModelUtils.setIndex(tabModel, dropIndex);
            return false;
        } else {
            // Dragging a collapsed tab group to a new window can result in a window with a single
            // collapsed group and no tab in foreground. To prevent this, we skip applying the
            // collapsed state and select the first tab in the dropped group instead.
            Tab selectedTab = TabModelUtils.getCurrentTab(tabModel);
            if (selectedTab == null) {
                TabModelUtils.setIndex(tabModel, /* index= */ 0);
            }
            return selectedTab != null;
        }
    }

    /**
     * Checks to see if the tabs in a list of tabs are a subset of the same tab group.
     *
     * @return The tab group id if they are, null otherwise.
     */
    public static @Nullable Token findSingleTabGroupIfPresent(List<Tab> tabs) {
        @Nullable Token tabGroupId = null;
        for (Tab tab : tabs) {
            if (tabGroupId == null) {
                tabGroupId = tab.getTabGroupId();
            } else if (!Objects.equals(tabGroupId, tab.getTabGroupId())) {
                return null;
            }
        }
        return tabGroupId;
    }
}

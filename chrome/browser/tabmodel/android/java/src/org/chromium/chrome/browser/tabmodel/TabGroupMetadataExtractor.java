// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;

import java.util.LinkedHashMap;
import java.util.List;

/** Utility class for extracting metadata from a list of tabs that form a tab group. */
@NullMarked
public class TabGroupMetadataExtractor {
    /**
     * Extracts the metadata of a given tab group, including tab IDs, URLs, group ID, root ID,
     * color, title, and collapsed state.
     *
     * @param groupedTabs The list of tabs that form the group.
     * @param sourceWindowIndex The index of the window that holds the tab group.
     * @param selectedTabId The selected tab ID of the group.
     * @return A {@link TabGroupMetadata} object containing the metadata of the tab group, or {@code
     *     null} if the provided tab group list is empty.
     */
    public static @Nullable TabGroupMetadata extractTabGroupMetadata(
            List<Tab> groupedTabs, int sourceWindowIndex, int selectedTabId) {
        if (groupedTabs.size() == 0) return null;

        // 1. Collect IDs and URLs for each tab in the group. Check if the selected tab is in the
        // tab group, otherwise default select the first tab in the group after re-parenting to
        // destination window.
        LinkedHashMap<Integer, String> tabIdsToUrls = new LinkedHashMap();
        boolean selectedTabIsInGroup = false;
        // Tabs are stored in reverse to ensure the correct opening order. Because tabs are inserted
        // one-by-one at the same start index in the target window, storing them in their original
        // order would reverse their final order in destination window.
        for (int i = groupedTabs.size() - 1; i >= 0; i--) {
            Tab tab = groupedTabs.get(i);
            if (tab.getId() == selectedTabId) selectedTabIsInGroup = true;
            tabIdsToUrls.put(tab.getId(), tab.getUrl().getSpec());
        }
        if (!selectedTabIsInGroup) selectedTabId = groupedTabs.get(0).getId();

        // 2. Get the first tab as a representative to retrieve the tab group ID and root ID.
        Tab firstTab = groupedTabs.get(0);
        @Nullable Token tabGroupId = firstTab.getTabGroupId();
        if (tabGroupId == null) return null;
        int rootId = firstTab.getRootId();

        // 3. Fetch group-level properties using the root ID.
        int tabGroupColor = TabGroupColorUtils.getTabGroupColor(rootId);
        @Nullable String tabGroupTitle = TabGroupTitleUtils.getTabGroupTitle(rootId);
        boolean tabGroupCollapsed = TabGroupCollapsedUtils.getTabGroupCollapsed(rootId);

        // 4. Create and populate TabGroupMetadata with data gathered above.
        TabGroupMetadata tabGroupMetadata =
                new TabGroupMetadata(
                        rootId,
                        selectedTabId,
                        sourceWindowIndex,
                        tabGroupId,
                        tabIdsToUrls,
                        tabGroupColor,
                        tabGroupTitle,
                        tabGroupCollapsed,
                        firstTab.isIncognitoBranded());
        return tabGroupMetadata;
    }
}

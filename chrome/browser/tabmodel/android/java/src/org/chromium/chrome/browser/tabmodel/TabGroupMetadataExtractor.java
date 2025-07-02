// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.text.TextUtils;

import org.chromium.base.FileUtils;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.external_intents.ExternalNavigationHandler;

import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** Utility class for extracting metadata from a list of tabs that form a tab group. */
@NullMarked
public class TabGroupMetadataExtractor {
    /**
     * Extracts the metadata of a given tab group, including tab IDs, URLs, group ID, color, title,
     * collapsed state and shared state.
     *
     * @param tabGroupModelFilter The tab group model filter to fetch group-level properties.
     * @param groupedTabs The list of tabs that form the group.
     * @param sourceWindowIndex The index of the window that holds the tab group.
     * @param selectedTabId The selected tab ID of the group.
     * @param isGroupShared Whether the tab group is shared with other collaborators.
     * @return A {@link TabGroupMetadata} object containing the metadata of the tab group, or {@code
     *     null} if the provided tab group list is empty.
     */
    public static @Nullable TabGroupMetadata extractTabGroupMetadata(
            TabGroupModelFilter tabGroupModelFilter,
            List<Tab> groupedTabs,
            int sourceWindowIndex,
            int selectedTabId,
            boolean isGroupShared) {
        if (groupedTabs.isEmpty()) return null;

        // 1. Collect IDs and URLs for each tab in the group. Check if the selected tab is in the
        // tab group, otherwise default select the first tab in the group after re-parenting to
        // destination window.
        ArrayList<Map.Entry<Integer, String>> tabIdsToUrls = new ArrayList<>();
        @Nullable String mhtmlTabTitle = null;
        boolean selectedTabIsInGroup = false;
        // Tabs are stored in reverse to ensure the correct opening order. Because tabs are inserted
        // one-by-one at the same start index in the target window, storing them in their original
        // order would reverse their final order in destination window.
        for (int i = groupedTabs.size() - 1; i >= 0; i--) {
            Tab tab = groupedTabs.get(i);
            if (tab.getId() == selectedTabId) selectedTabIsInGroup = true;
            String url = tab.getUrl().getSpec();
            tabIdsToUrls.add(new AbstractMap.SimpleImmutableEntry<>(tab.getId(), url));
            if (isMhtmlUrl(url)) {
                mhtmlTabTitle = tab.getTitle();
            }
        }
        if (!selectedTabIsInGroup) selectedTabId = groupedTabs.get(0).getId();

        // 2. Get the first tab as a representative to retrieve the tab group ID.
        Tab firstTab = groupedTabs.get(0);
        @Nullable Token tabGroupId = firstTab.getTabGroupId();
        if (tabGroupId == null) return null;

        // 3. Fetch group-level properties using the tab group ID.
        int tabGroupColor = tabGroupModelFilter.getTabGroupColor(tabGroupId);
        @Nullable String tabGroupTitle = tabGroupModelFilter.getTabGroupTitle(tabGroupId);
        boolean tabGroupCollapsed = tabGroupModelFilter.getTabGroupCollapsed(tabGroupId);

        // If the tab group is collapsed, do not select any tab within the group.
        if (tabGroupCollapsed) selectedTabId = Tab.INVALID_TAB_ID;

        // 4. Create and populate TabGroupMetadata with data gathered above.
        return new TabGroupMetadata(
                selectedTabId,
                sourceWindowIndex,
                tabGroupId,
                tabIdsToUrls,
                tabGroupColor,
                tabGroupTitle,
                mhtmlTabTitle,
                tabGroupCollapsed,
                isGroupShared,
                firstTab.isIncognitoBranded());
    }

    private static boolean isMhtmlUrl(String url) {
        String scheme = ExternalNavigationHandler.getSanitizedUrlScheme(url);
        boolean isFileUriScheme = TextUtils.equals(scheme, UrlConstants.FILE_SCHEME);
        String extension = FileUtils.getExtension(url);
        boolean isMhtmlExtension = extension.equals("mhtml") || extension.equals("mht");
        return isFileUriScheme && isMhtmlExtension;
    }
}

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.List;

/** Helper class to handle tab groups related utilities. */
public class TabGroupUtils {
    /**
     * This method gets the selected tab of the group where {@code tab} is in.
     *
     * @param filter The filter that owns the {@code tab}.
     * @param tab The {@link Tab}.
     * @return The selected tab of the group which contains the {@code tab}
     */
    public static Tab getSelectedTabInGroupForTab(TabGroupModelFilter filter, Tab tab) {
        return filter.getTabAt(filter.indexOf(tab));
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
}

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import org.chromium.base.Log;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** This class collects UMA statistics for tab group creation or complex tab navigation patterns. */
public class TasksUma {
    private static final String TAG = "TasksUma";

    /**
     * This method collects the tab creation statistics based on the input {@link TabModel}, and
     * records the statistics.
     * @param model Collects tab creation statistics from this model
     */
    public static void recordTasksUma(TabModel model) {
        Map<Integer, List<Integer>> tabsRelationshipList = new HashMap<>();
        getTabsRelationship(model, tabsRelationshipList);

        int tabInGroupsCount = 0;
        int tabGroupCount = 0;
        List<Integer> rootTabList = tabsRelationshipList.get(Tab.INVALID_TAB_ID);
        if (rootTabList == null) {
            Log.d(TAG, "TabModel should have at least one root tab");
            return;
        }
        for (int i = 0; i < rootTabList.size(); i++) {
            int tabsInThisGroupCount =
                    getTabsInOneGroupCount(tabsRelationshipList, rootTabList.get(i));
            if (tabsInThisGroupCount > 1) {
                tabInGroupsCount += tabsInThisGroupCount;
                tabGroupCount++;
            }
        }
    }

    private static int getTabsInOneGroupCount(
            Map<Integer, List<Integer>> tabsRelationList, int rootId) {
        int count = 1;
        if (tabsRelationList.containsKey(rootId)) {
            List<Integer> childTabsId = tabsRelationList.get(rootId);
            for (int i = 0; i < childTabsId.size(); i++) {
                count += getTabsInOneGroupCount(tabsRelationList, childTabsId.get(i));
            }
        }
        return count;
    }

    private static void getTabsRelationship(
            TabModel model, Map<Integer, List<Integer>> tabsRelationList) {
        int duplicatedTabCount = 0;
        Map<String, Integer> uniqueUrlCounterMap = new HashMap<>();

        for (int i = 0; i < model.getCount(); i++) {
            Tab currentTab = model.getTabAt(i);

            String url = currentTab.getUrl().getSpec();
            int urlDuplicatedCount = 0;
            if (uniqueUrlCounterMap.containsKey(url)) {
                duplicatedTabCount++;
                urlDuplicatedCount = uniqueUrlCounterMap.get(url);
            }
            uniqueUrlCounterMap.put(url, urlDuplicatedCount + 1);

            int parentIdOfCurrentTab = currentTab.getParentId();
            if (!tabsRelationList.containsKey(parentIdOfCurrentTab)) {
                tabsRelationList.put(parentIdOfCurrentTab, new ArrayList<>());
            }
            tabsRelationList.get(parentIdOfCurrentTab).add(currentTab.getId());
        }
    }
}

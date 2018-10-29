// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * This class collects UMA statistics for tab group creation or complex tab navigation patterns.
 */
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

        recordParentChildrenTabStatistic(tabInGroupsCount, tabGroupCount, model.getCount());
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

            String url = currentTab.getUrl();
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

        recordDuplicatedTabStatistic(duplicatedTabCount, model.getCount());
    }

    private static void recordParentChildrenTabStatistic(
            int tabsInGroupCount, int tabGroupCount, int totalTabCount) {
        if (totalTabCount == 0) return;

        RecordHistogram.recordCountHistogram("Tabs.Tasks.TabGroupCount", tabGroupCount);

        RecordHistogram.recordCountHistogram("Tabs.Tasks.TabsInGroupCount", tabsInGroupCount);

        double tabsInGroupRatioPercent = tabsInGroupCount * 1.0 / totalTabCount * 100.0;
        RecordHistogram.recordPercentageHistogram(
                "Tabs.Tasks.TabsInGroupRatio", (int) tabsInGroupRatioPercent);

        if (tabGroupCount != 0) {
            int averageGroupSize = tabsInGroupCount / tabGroupCount;
            RecordHistogram.recordCountHistogram(
                    "Tabs.Tasks.AverageTabGroupSize", averageGroupSize);
            Log.d(TAG, "AverageGroupSize: %d", averageGroupSize);
        }

        double tabGroupDensityPercent = tabGroupCount * 1.0 / totalTabCount * 100.0;
        RecordHistogram.recordPercentageHistogram(
                "Tabs.Tasks.TabGroupDensity", (int) tabGroupDensityPercent);

        Log.d(TAG, "TotalTabCount: %d", totalTabCount);
        Log.d(TAG, "TabGroupCount: %d", tabGroupCount);
        Log.d(TAG, "TabsInGroupCount: %d", tabsInGroupCount);
        Log.d(TAG, "TabsInGroupRatioPercent: %d", (int) tabsInGroupRatioPercent);
        Log.d(TAG, "TabGroupDensityPercent: %d", (int) tabGroupDensityPercent);
    }

    private static void recordDuplicatedTabStatistic(int duplicatedTabCount, int totalTabCount) {
        if (totalTabCount == 0 || duplicatedTabCount >= totalTabCount) return;

        RecordHistogram.recordCountHistogram(
                "Tabs.Tasks.DuplicatedTab.DuplicatedTabCount", duplicatedTabCount);

        int duplicatedTabRatioPercent = 100 * duplicatedTabCount / totalTabCount;
        RecordHistogram.recordPercentageHistogram(
                "Tabs.Tasks.DuplicatedTab.DuplicatedTabRatio", duplicatedTabRatioPercent);
    }
}

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
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

    /**
     * This method collects the tab creation statistics based on the input {@link TabModel} in the
     * Tab Switcher mode, and records the statistics.
     * @param model The model to be used for collecting statistics.
     */
    public static void recordTabLaunchType(TabModel model) {
        int manuallyCreatedCount = 0;
        int targetBlankCreatedCount = 0;
        int externalAppCreatedCount = 0;
        int othersCreatedCount = 0;
        int totalTabCount = model.getCount();

        if (totalTabCount == 0) return;

        for (int i = 0; i < totalTabCount; i++) {
            Integer tabLaunchType =
                    CriticalPersistedTabData.from(model.getTabAt(i)).getTabLaunchTypeAtCreation();
            if (tabLaunchType == null) {
                // This should not happen. Because @{link Tab#TabLaunchType} is never null, except
                // for testing purpose or in the document-mode which it's deprecated.
                othersCreatedCount++;
                continue;
            }
            if (tabLaunchType == TabLaunchType.FROM_CHROME_UI
                    || tabLaunchType == TabLaunchType.FROM_START_SURFACE
                    || tabLaunchType == TabLaunchType.FROM_LONGPRESS_BACKGROUND
                    || tabLaunchType == TabLaunchType.FROM_LAUNCHER_SHORTCUT
                    || tabLaunchType == TabLaunchType.FROM_APP_WIDGET
                    || tabLaunchType == TabLaunchType.FROM_RECENT_TABS) {
                manuallyCreatedCount++;
            } else if (tabLaunchType == TabLaunchType.FROM_LONGPRESS_FOREGROUND
                    || tabLaunchType == TabLaunchType.FROM_LONGPRESS_INCOGNITO) {
                targetBlankCreatedCount++;
            } else if (tabLaunchType == TabLaunchType.FROM_EXTERNAL_APP
                    || tabLaunchType == TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB) {
                externalAppCreatedCount++;
            } else {
                othersCreatedCount++;
            }
        }

        RecordHistogram.recordCount1MHistogram(
                "Tabs.Tasks.TabCreated.Count.FromManuallyCreated", manuallyCreatedCount);

        RecordHistogram.recordCount1MHistogram(
                "Tabs.Tasks.TabCreated.Count.FromTargetBlank", targetBlankCreatedCount);

        RecordHistogram.recordCount1MHistogram(
                "Tabs.Tasks.TabCreated.Count.FromExternalApp", externalAppCreatedCount);

        RecordHistogram.recordCount1MHistogram(
                "Tabs.Tasks.TabCreated.Count.FromOthers", othersCreatedCount);

        RecordHistogram.recordPercentageHistogram(
                "Tabs.Tasks.TabCreated.Percent.FromManuallyCreated",
                manuallyCreatedCount * 100 / totalTabCount);

        RecordHistogram.recordPercentageHistogram("Tabs.Tasks.TabCreated.Percent.FromTargetBlank",
                targetBlankCreatedCount * 100 / totalTabCount);

        RecordHistogram.recordPercentageHistogram("Tabs.Tasks.TabCreated.Percent.FromExternalApp",
                externalAppCreatedCount * 100 / totalTabCount);

        RecordHistogram.recordPercentageHistogram("Tabs.Tasks.TabCreated.Percent.FromOthers",
                othersCreatedCount * 100 / totalTabCount);
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

            int parentIdOfCurrentTab = CriticalPersistedTabData.from(currentTab).getParentId();
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

        RecordHistogram.recordCount1MHistogram("Tabs.Tasks.TabGroupCount", tabGroupCount);

        RecordHistogram.recordCount1MHistogram("Tabs.Tasks.TabsInGroupCount", tabsInGroupCount);

        double tabsInGroupRatioPercent = tabsInGroupCount * 1.0 / totalTabCount * 100.0;
        RecordHistogram.recordPercentageHistogram(
                "Tabs.Tasks.TabsInGroupRatio", (int) tabsInGroupRatioPercent);

        if (tabGroupCount != 0) {
            int averageGroupSize = tabsInGroupCount / tabGroupCount;
            RecordHistogram.recordCount1MHistogram(
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

        RecordHistogram.recordCount1MHistogram(
                "Tabs.Tasks.DuplicatedTab.DuplicatedTabCount", duplicatedTabCount);

        int duplicatedTabRatioPercent = 100 * duplicatedTabCount / totalTabCount;
        RecordHistogram.recordPercentageHistogram(
                "Tabs.Tasks.DuplicatedTab.DuplicatedTabRatio", duplicatedTabRatioPercent);
    }
}

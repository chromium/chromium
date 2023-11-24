// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupTitleUtils;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/** A tab model observer for managing bulk closures. */
public class HistoricalTabModelObserver implements TabModelObserver {
    private final TabModel mTabModel;
    private HistoricalTabSaver mHistoricalTabSaver;

    public HistoricalTabModelObserver(TabModel tabModel) {
        this(tabModel, new HistoricalTabSaverImpl(tabModel));
    }

    public HistoricalTabModelObserver(TabModel tabModel, HistoricalTabSaver historicalTabSaver) {
        mTabModel = tabModel;
        mHistoricalTabSaver = historicalTabSaver;

        mTabModel.addObserver(this);
    }

    public void destroy() {
        mTabModel.removeObserver(this);
    }

    @Override
    public void onFinishingMultipleTabClosure(List<Tab> tabs) {
        if (tabs.isEmpty()) return;

        if (tabs.size() == 1) {
            mHistoricalTabSaver.createHistoricalTab(tabs.get(0));
            return;
        }

        buildGroupsAndCreateClosure(tabs);
    }

    private void buildGroupsAndCreateClosure(List<Tab> tabs) {
        HashMap<Integer, HistoricalEntry> idToGroup = new HashMap<>();
        List<HistoricalEntry> entries = new ArrayList<>();

        for (Tab tab : tabs) {
            // {@link TabGroupModelFilter} removes tabs from its data model as soon as they are
            // pending closure so it cannot be directly relied upon for group structure. Instead
            // rely on the underlying root ID in the tab's persisted data which is used to restore
            // groups across an pending closure cancellation (undo). The root ID is the group ID
            // unless the tab is ungrouped in which case the root ID is the tab's ID.
            int groupId = tab.getRootId();
            if (idToGroup.containsKey(groupId)) {
                idToGroup.get(groupId).getTabs().add(tab);
                continue;
            }
            // null title for default title is handled in HistoricalTabSaver.
            String title = TabGroupTitleUtils.getTabGroupTitle(groupId);
            List<Tab> groupTabs = new ArrayList<Tab>();
            groupTabs.add(tab);
            // Single entry groups are collapsed to tabs in HistoricalTabSaver.
            HistoricalEntry historicalGroup = new HistoricalEntry(groupId, title, groupTabs);
            entries.add(historicalGroup);
            idToGroup.put(groupId, historicalGroup);
        }

        mHistoricalTabSaver.createHistoricalBulkClosure(entries);
    }
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.core.util.Pair;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupFaviconCluster.ClusterData;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupTimeAgo.TimestampEvent;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;

/** Contains the logic to set the state of the model and react to actions. */
@NullMarked
class TabGroupListBottomSheetRowMediator {
    private final SavedTabGroup mSavedTabGroup;
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final PropertyModel mPropertyModel;

    /**
     * @param savedTabGroup The tab group to be represented by this row.
     * @param tabGroupModelFilter Used to read current tab groups.
     * @param tabGroupSyncService Used to fetch synced copy of tab groups.
     * @param dataSharingService Used to fetch shared group data.
     * @param collaborationService Used to fetch collaboration group data.
     * @param faviconResolver Used to fetch favicon images for some tabs.
     * @param onClickRunnable To be run on clicking the row.
     * @param tabs The tabs to be added to a tab group.
     */
    public TabGroupListBottomSheetRowMediator(
            SavedTabGroup savedTabGroup,
            TabGroupModelFilter tabGroupModelFilter,
            @Nullable TabGroupSyncService tabGroupSyncService,
            DataSharingService dataSharingService,
            CollaborationService collaborationService,
            FaviconResolver faviconResolver,
            Runnable onClickRunnable,
            List<Tab> tabs) {
        mSavedTabGroup = savedTabGroup;
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupSyncService = tabGroupSyncService;

        PropertyModel.Builder builder = new PropertyModel.Builder(TabGroupRowProperties.ALL_KEYS);
        int numberOfTabs = savedTabGroup.savedTabs.size();

        List<GURL> urlList = TabGroupFaviconCluster.buildUrlListFromSyncGroup(savedTabGroup);
        ClusterData clusterData = new ClusterData(faviconResolver, numberOfTabs, urlList);
        builder.with(TabGroupRowProperties.CLUSTER_DATA, clusterData);
        builder.with(TabGroupRowProperties.COLOR_INDEX, savedTabGroup.color);

        String userTitle = savedTabGroup.title;
        Pair<String, Integer> titleData = new Pair<>(userTitle, numberOfTabs);
        builder.with(TabGroupRowProperties.TITLE_DATA, titleData);

        builder.with(
                TabGroupRowProperties.TIMESTAMP_EVENT,
                new TabGroupTimeAgo(savedTabGroup.creationTimeMs, TimestampEvent.UPDATED));
        builder.with(
                TabGroupRowProperties.ROW_CLICK_RUNNABLE,
                () -> {
                    addToGroup(tabs);
                    onClickRunnable.run();
                });
        mPropertyModel = builder.build();
    }

    public PropertyModel getModel() {
        return mPropertyModel;
    }

    private void addToGroup(List<Tab> tabs) {
        String syncId = mSavedTabGroup.syncId;
        if (syncId == null || mTabGroupSyncService == null) return;

        // Ensure that the group still exists.
        @Nullable SavedTabGroup group = mTabGroupSyncService.getGroup(syncId);
        if (group == null || group.savedTabs.isEmpty()) return;

        SavedTabGroupTab savedTabGroupTab = group.savedTabs.get(0);
        Integer localId = savedTabGroupTab.localId;
        Tab tab = mTabGroupModelFilter.getTabModel().getTabById(localId);

        mTabGroupModelFilter.mergeListOfTabsToGroup(tabs, tab, true);
    }
}

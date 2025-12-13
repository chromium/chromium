// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tabmodel.TabGroupUtils.mergeTabsToDest;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils.TabMovedCallback;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupFaviconCluster.ClusterData;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupRowView.TabGroupRowViewTitleData;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupTimeAgo.TimestampEvent;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Objects;

/**
 * Contains the logic to set the state of the model and react to actions. Uses the {@link
 * TabGroupSyncService} as its primary source of truth.
 */
@NullMarked
class TabGroupListBottomSheetRowMediator {
    private final SavedTabGroup mSavedTabGroup;
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final @Nullable TabMovedCallback mTabMovedCallback;
    private final PropertyModel mPropertyModel;

    /**
     * @param savedTabGroup The tab group to be represented by this row.
     * @param tabGroupModelFilter Used to read current tab groups.
     * @param faviconResolver Used to fetch favicon images for some tabs.
     * @param tabGroupSyncService Used to fetch synced copy of tab groups.
     * @param onClickRunnable To be run on clicking the row.
     * @param tabMovedCallback Used to follow up on a tab being moved groups or ungrouped.
     * @param tabs The tabs to be added to a tab group.
     */
    public TabGroupListBottomSheetRowMediator(
            SavedTabGroup savedTabGroup,
            TabGroupModelFilter tabGroupModelFilter,
            FaviconResolver faviconResolver,
            @Nullable TabGroupSyncService tabGroupSyncService,
            Runnable onClickRunnable,
            @Nullable TabMovedCallback tabMovedCallback,
            List<Tab> tabs) {
        mSavedTabGroup = savedTabGroup;
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupSyncService = tabGroupSyncService;
        mTabMovedCallback = tabMovedCallback;

        int numTabs = mSavedTabGroup.savedTabs.size();
        List<GURL> urlList = TabGroupFaviconCluster.buildUrlListFromSyncGroup(mSavedTabGroup);

        PropertyModel.Builder builder = new PropertyModel.Builder(TabGroupRowProperties.ALL_KEYS);
        builder.with(
                TabGroupRowProperties.CLUSTER_DATA,
                new ClusterData(faviconResolver, numTabs, urlList));
        builder.with(TabGroupRowProperties.COLOR_INDEX, mSavedTabGroup.color);

        TabGroupRowViewTitleData titleData =
                new TabGroupRowViewTitleData(
                        mSavedTabGroup.title,
                        numTabs,
                        R.plurals.tab_group_bottom_sheet_row_accessibility_text);
        builder.with(TabGroupRowProperties.TITLE_DATA, titleData);

        builder.with(
                TabGroupRowProperties.TIMESTAMP_EVENT,
                new TabGroupTimeAgo(mSavedTabGroup.updateTimeMs, TimestampEvent.UPDATED));
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
        RecordUserAction.record("TabGroupParity.BottomSheetRowSelection.ExistingGroup");

        assert !tabs.isEmpty();
        String syncId = mSavedTabGroup.syncId;
        if (syncId == null || mTabGroupSyncService == null) {
            return;
        }

        // Ensure that the group still exists.
        @Nullable SavedTabGroup group = mTabGroupSyncService.getGroup(syncId);
        if (group == null || group.savedTabs.isEmpty()) {
            return;
        }

        SavedTabGroupTab savedTabGroupTab = group.savedTabs.get(0);
        @Nullable Integer localId = savedTabGroupTab.localId;
        if (localId == null) {
            return;
        }

        // No-op if the tabs to be moved are already in the group.
        if (areTabsAlreadyInGroup(tabs)) {
            return;
        }

        mergeTabsToDest(tabs, localId, mTabGroupModelFilter, mTabMovedCallback);
    }

    private boolean areTabsAlreadyInGroup(List<Tab> tabsToBeMoved) {
        @Nullable LocalTabGroupId tabGroupLocalId = mSavedTabGroup.localId;
        assert tabGroupLocalId != null;

        boolean areTabsAlreadyInGroup = true;
        for (Tab tabToBeMoved : tabsToBeMoved) {
            areTabsAlreadyInGroup &=
                    Objects.equals(tabGroupLocalId.tabGroupId, tabToBeMoved.getTabGroupId());
        }
        return areTabsAlreadyInGroup;
    }
}

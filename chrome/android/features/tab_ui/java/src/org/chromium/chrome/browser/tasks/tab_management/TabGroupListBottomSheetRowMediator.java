// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils.TabMovedCallback;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupFaviconCluster.ClusterData;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupRowView.TabGroupRowViewTitleData;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupTimeAgo.TimestampEvent;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Contains the logic to set the state of the model and react to actions. Uses the {@link
 * TabGroupSyncService} as its primary source of truth.
 */
@NullMarked
class TabGroupListBottomSheetRowMediator {
    private final GroupWindowInfo mGroupInfo;
    private final TabModel mTabModel;
    private final @Nullable TabMovedCallback mTabMovedCallback;
    private final PropertyModel mPropertyModel;

    /**
     * @param groupInfo The tab group to be represented by this row.
     * @param tabModel Used to read current tab groups.
     * @param faviconResolver Used to fetch favicon images for some tabs.
     * @param tabGroupSyncService Used to fetch synced copy of tab groups.
     * @param onClickRunnable To be run on clicking the row.
     * @param tabMovedCallback Used to follow up on a tab being moved groups or ungrouped.
     * @param tabs The tabs to be added to a tab group.
     */
    public TabGroupListBottomSheetRowMediator(
            GroupWindowInfo groupInfo,
            TabModel tabModel,
            FaviconResolver faviconResolver,
            @Nullable TabGroupSyncService tabGroupSyncService,
            Runnable onClickRunnable,
            @Nullable TabMovedCallback tabMovedCallback,
            List<Tab> tabs) {
        mGroupInfo = groupInfo;
        mTabModel = tabModel;
        mTabMovedCallback = tabMovedCallback;

        int numTabs = mGroupInfo.tabCount;
        List<GURL> urlList = mGroupInfo.faviconUrls;

        PropertyModel.Builder builder = new PropertyModel.Builder(TabGroupRowProperties.ALL_KEYS);
        builder.with(
                TabGroupRowProperties.CLUSTER_DATA,
                new ClusterData(faviconResolver, numTabs, urlList));
        builder.with(TabGroupRowProperties.COLOR_INDEX, mGroupInfo.color);

        TabGroupRowViewTitleData titleData =
                new TabGroupRowViewTitleData(
                        mGroupInfo.title,
                        numTabs,
                        R.plurals.tab_group_bottom_sheet_row_accessibility_text);
        builder.with(TabGroupRowProperties.TITLE_DATA, titleData);

        builder.with(
                TabGroupRowProperties.TIMESTAMP_EVENT,
                new TabGroupTimeAgo(mGroupInfo.lastModifiedTimeMs, TimestampEvent.UPDATED));
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
        if (mGroupInfo.localId == null) {
            return;
        }
        TabGroupUiUtils.addTabsToGroup(
                mTabModel, tabs, mGroupInfo.localId, mTabMovedCallback, /* bringToFront= */ false);
    }
}

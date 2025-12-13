// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tabmodel.TabGroupUtils.mergeTabsToDest;

import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils.TabMovedCallback;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupFaviconCluster.ClusterData;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupRowView.TabGroupRowViewTitleData;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Objects;

/**
 * Contains the logic to set the state of the model and react to actions. Uses the {@link
 * TabGroupModelFilter} as its primary source of truth.
 */
@NullMarked
class LocalTabGroupListBottomSheetRowMediator {
    private final Token mGroupId;
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final @Nullable TabMovedCallback mTabMovedCallback;
    private final PropertyModel mPropertyModel;

    /**
     * @param groupId The id of the tab group to be represented by this row.
     * @param tabGroupModelFilter Used to read current tab groups.
     * @param faviconResolver Used to fetch favicon images for some tabs.
     * @param onClickRunnable To be run on clicking the row.
     * @param tabMovedCallback Used to follow up on a tab being moved groups or ungrouped.
     * @param tabs The tabs to be added to a tab group.
     */
    public LocalTabGroupListBottomSheetRowMediator(
            Token groupId,
            TabGroupModelFilter tabGroupModelFilter,
            FaviconResolver faviconResolver,
            Runnable onClickRunnable,
            @Nullable TabMovedCallback tabMovedCallback,
            List<Tab> tabs) {
        mGroupId = groupId;
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabMovedCallback = tabMovedCallback;

        int numTabs = mTabGroupModelFilter.getTabCountForGroup(mGroupId);
        List<GURL> urlList =
                TabGroupFaviconCluster.buildUrlListFromFilter(mGroupId, mTabGroupModelFilter);

        PropertyModel.Builder builder = new PropertyModel.Builder(TabGroupRowProperties.ALL_KEYS);
        builder.with(
                TabGroupRowProperties.CLUSTER_DATA,
                new ClusterData(faviconResolver, numTabs, urlList));
        builder.with(
                TabGroupRowProperties.COLOR_INDEX, mTabGroupModelFilter.getTabGroupColor(groupId));

        TabGroupRowViewTitleData titleData =
                new TabGroupRowViewTitleData(
                        mTabGroupModelFilter.getTabGroupTitle(groupId),
                        numTabs,
                        R.plurals.tab_group_bottom_sheet_row_accessibility_text);
        builder.with(TabGroupRowProperties.TITLE_DATA, titleData);
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

        // Ensure that the group still exists.
        if (!mTabGroupModelFilter.tabGroupExists(mGroupId)) {
            return;
        }

        // No-op if the tabs to be moved are already in the group.
        if (areTabsAlreadyInGroup(tabs)) {
            return;
        }

        @TabId int destTabId = mTabGroupModelFilter.getGroupLastShownTabId(mGroupId);
        mergeTabsToDest(tabs, destTabId, mTabGroupModelFilter, mTabMovedCallback);
    }

    private boolean areTabsAlreadyInGroup(List<Tab> tabsToBeMoved) {
        boolean areTabsAlreadyInGroup = true;
        for (Tab tabToBeMoved : tabsToBeMoved) {
            areTabsAlreadyInGroup &= Objects.equals(mGroupId, tabToBeMoved.getTabGroupId());
        }
        return areTabsAlreadyInGroup;
    }
}

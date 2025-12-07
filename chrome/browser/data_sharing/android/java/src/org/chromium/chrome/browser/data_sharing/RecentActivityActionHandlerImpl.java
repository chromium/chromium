// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityActionHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

/** Implementation for {@code RecentActivityActionHandler}. */
@NullMarked
public class RecentActivityActionHandlerImpl implements RecentActivityActionHandler {
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final TabModelSelector mTabModelSelector;
    private final DataSharingTabGroupsDelegate mDataSharingTabGroupsDelegate;
    private final @Nullable String mSyncTabGroupId;
    private final Runnable mManageSharingCallback;

    /**
     * Constructor.
     *
     * @param tabGroupSyncService The tab group sync backend.
     * @param tabModelSelector The tab model selector to provide access to tab model for focusing
     *     and opening tabs.
     * @param dataSharingTabGroupsDelegate Delegate to open the tab group edit dialog.
     * @param collaborationId The collaboration ID.
     * @param syncTabGroupId The tab group sync ID as referred in {@link TabGroupSyncService}.
     * @param manageSharingCallback The callback to open people group management screen.
     */
    public RecentActivityActionHandlerImpl(
            TabGroupSyncService tabGroupSyncService,
            TabModelSelector tabModelSelector,
            DataSharingTabGroupsDelegate dataSharingTabGroupsDelegate,
            String collaborationId,
            String syncTabGroupId,
            Runnable manageSharingCallback) {
        mTabGroupSyncService = tabGroupSyncService;
        mTabModelSelector = tabModelSelector;
        mDataSharingTabGroupsDelegate = dataSharingTabGroupsDelegate;
        mSyncTabGroupId = syncTabGroupId;
        mManageSharingCallback = manageSharingCallback;
    }

    @Override
    public void focusTab(int tabId) {
        mDataSharingTabGroupsDelegate.hideTabSwitcherAndShowTab(tabId);
    }

    @Override
    public void reopenTab(String url) {
        SavedTabGroup savedTabGroup = getSavedTabGroup();
        assert savedTabGroup != null;
        assert savedTabGroup.localId != null;
        TabGroupModelFilter tabGroupModelFilter =
                mTabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(/* isIncognito= */ false);
        assumeNonNull(tabGroupModelFilter);
        int rootId = tabGroupModelFilter.getGroupLastShownTabId(savedTabGroup.localId.tabGroupId);
        assert rootId != Tab.INVALID_TAB_ID;

        TabGroupUtils.openUrlInGroup(
                tabGroupModelFilter, url, rootId, TabLaunchType.FROM_TAB_GROUP_UI);
    }

    @Override
    public void openTabGroupEditDialog() {
        SavedTabGroup savedTabGroup = getSavedTabGroup();
        LocalTabGroupId localId = assumeNonNull(savedTabGroup).localId;
        mDataSharingTabGroupsDelegate.openTabGroup(assumeNonNull(localId).tabGroupId);
    }

    @Override
    public void manageSharing() {
        mManageSharingCallback.run();
    }

    private @Nullable SavedTabGroup getSavedTabGroup() {
        assumeNonNull(mTabGroupSyncService);
        assumeNonNull(mSyncTabGroupId);
        return mTabGroupSyncService.getGroup(mSyncTabGroupId);
    }
}

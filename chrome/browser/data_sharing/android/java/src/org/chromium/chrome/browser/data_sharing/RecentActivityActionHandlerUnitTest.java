// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link DataSharingFaviconProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RecentActivityActionHandlerUnitTest {
    private static final String COLLABORATION_ID = "collaboration_1";
    private static final String SYNC_TAB_GROUP_ID = "sync_tab_group_1";
    private static final String SYNC_TAB_ID = "sync_tab_1";
    private static final Token TOKEN_1 = new Token(3, 5);
    private static final int ROOT_ID_1 = 9;
    private static final int TAB_ID_1 = 9;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private DataSharingTabGroupsDelegate mDataSharingTabGroupsDelegate;
    @Mock private Runnable mManageSharingCallback;
    @Mock private TabModel mTabModel;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock private TabCreator mTabCreator;
    @Mock private Tab mTab1;
    private RecentActivityActionHandlerImpl mRecentActivityActionHandler;

    @Before
    public void setUp() {
        // Set up tab model with a single tab and as part of a tab group.
        when(mTab1.getId()).thenReturn(TAB_ID_1);
        when(mTab1.getRootId()).thenReturn(ROOT_ID_1);
        when(mTab1.getTabGroupId()).thenReturn(TOKEN_1);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(mTab1);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        when(mTabGroupModelFilterProvider.getTabGroupModelFilter(false))
                .thenReturn(mTabGroupModelFilter);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.getRootIdFromTabGroupId(TOKEN_1)).thenReturn(ROOT_ID_1);
        List<Tab> relatedTabs = new ArrayList<>();
        relatedTabs.add(mTab1);
        when(mTabGroupModelFilter.getTabGroupIdFromRootId(ROOT_ID_1)).thenReturn(TOKEN_1);
        when(mTabGroupModelFilter.getRelatedTabList(ROOT_ID_1)).thenReturn(relatedTabs);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);

        // Setup saved tab group with a single tab.
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.syncId = SYNC_TAB_GROUP_ID;
        savedTabGroup.localId = new LocalTabGroupId(TOKEN_1);
        SavedTabGroupTab savedTab = new SavedTabGroupTab();
        savedTab.syncId = SYNC_TAB_ID;
        savedTab.syncGroupId = SYNC_TAB_GROUP_ID;
        savedTab.localId = TAB_ID_1;
        savedTabGroup.savedTabs.add(savedTab);
        when(mTabGroupSyncService.getGroup(SYNC_TAB_GROUP_ID)).thenReturn(savedTabGroup);
        mRecentActivityActionHandler =
                new RecentActivityActionHandlerImpl(
                        mTabGroupSyncService,
                        mTabModelSelector,
                        mDataSharingTabGroupsDelegate,
                        COLLABORATION_ID,
                        SYNC_TAB_GROUP_ID,
                        mManageSharingCallback);
    }

    @Test
    public void testFocusTab() {
        mRecentActivityActionHandler.focusTab(TAB_ID_1);
        verify(mDataSharingTabGroupsDelegate, times(1)).hideTabSwitcherAndShowTab(eq(TAB_ID_1));
    }

    @Test
    public void testReopenTab() {
        String url = "https://google.com";
        mRecentActivityActionHandler.reopenTab(url);
        verify(mTabCreator, times(1)).createNewTab(any(), anyInt(), any());
    }

    @Test
    public void testOpenTabGroupEditDialog() {
        mRecentActivityActionHandler.openTabGroupEditDialog();
        verify(mDataSharingTabGroupsDelegate, times(1)).openTabGroup(TOKEN_1);
    }

    @Test
    public void testManageSharing() {
        mRecentActivityActionHandler.manageSharing();
        verify(mManageSharingCallback, times(1)).run();
    }
}

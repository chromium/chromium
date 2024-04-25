// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.util.Pair;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncController.TabCreationDelegate;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for the {@link StartupHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StartupHelperUnitTest {
    private static final int TAB_ID_1 = 5;
    private static final int TAB_ID_2 = 6;
    private static final Token TOKEN_1 = new Token(2, 3);
    private static final int ROOT_ID_1 = 1;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    private MockTabModel mTabModel;
    private @Mock TabGroupModelFilter mTabGroupModelFilter;
    private TabGroupSyncService mTabGroupSyncService;
    private @Mock LocalTabGroupMutationHelper mLocalMutationHelper;
    private @Mock RemoteTabGroupMutationHelper mRemoteMutationHelper;
    private StartupHelper mStartupHelper;
    private Tab mTab1;
    private Tab mTab2;

    private static Tab prepareTab(int tabId, int rootId) {
        Tab tab = Mockito.mock(Tab.class);
        Mockito.doReturn(tabId).when(tab).getId();
        Mockito.doReturn(rootId).when(tab).getRootId();
        Mockito.doReturn(GURL.emptyGURL()).when(tab).getUrl();
        return tab;
    }

    @Before
    public void setUp() {
        mTabGroupSyncService = spy(new TestTabGroupSyncService());
        mTab1 = prepareTab(TAB_ID_1, ROOT_ID_1);
        mTab2 = prepareTab(TAB_ID_2, ROOT_ID_1);

        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.getRelatedTabIds(ROOT_ID_1)).thenReturn(new ArrayList<>());

        mLocalMutationHelper =
                spy(
                        new LocalTabGroupMutationHelper(
                                mTabGroupModelFilter,
                                mTabGroupSyncService,
                                new TestTabCreationDelegate(),
                                new NavigationTracker()));
        mRemoteMutationHelper =
                spy(new RemoteTabGroupMutationHelper(mTabGroupModelFilter, mTabGroupSyncService));
        mStartupHelper =
                new StartupHelper(
                        mTabGroupModelFilter,
                        mTabGroupSyncService,
                        mLocalMutationHelper,
                        mRemoteMutationHelper);

        when(mTabGroupModelFilter.getRootIdFromStableId(eq(TOKEN_1))).thenReturn(ROOT_ID_1);
        when(mTabGroupModelFilter.getStableIdFromRootId(eq(ROOT_ID_1))).thenReturn(TOKEN_1);
    }

    @Test
    public void testCloseLocalTabsAndUpdateLocalGroups() {
        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        savedTabGroup.savedTabs.remove(1);
        savedTabGroup.localId = new LocalTabGroupId(TOKEN_1);
        String syncId = savedTabGroup.syncId;
        when(mTab1.getTabGroupId()).thenReturn(TOKEN_1);
        mTabModel.addTab(
                mTab1, 0, TabLaunchType.FROM_TAB_GROUP_UI, TabCreationState.LIVE_IN_BACKGROUND);
        List<Integer> tabIds = new ArrayList<>();
        tabIds.add(mTab1.getId());
        when(mTabGroupModelFilter.getRelatedTabIds(ROOT_ID_1)).thenReturn(tabIds);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {syncId});
        when(mTabGroupSyncService.getGroup(savedTabGroup.localId)).thenReturn(savedTabGroup);
        List<Pair<String, LocalTabGroupId>> ids = new ArrayList<>();
        ids.add(new Pair<>(syncId, savedTabGroup.localId));
        when(mTabGroupSyncService.getDeletedGroupIds()).thenReturn(ids);
        mStartupHelper.initializeTabGroupSync();
        verify(mTabGroupSyncService).updateLocalTabId(any(), anyString(), anyInt());
        verify(mTabGroupSyncService).getDeletedGroupIds();
        verify(mLocalMutationHelper).updateTabGroup(any());
        verify(mTabModel, times(2)).closeMultipleTabs(anyList(), eq(false));
    }

    @Test
    public void testCreatesRemoteGroupsForNewGroups() {
        when(mTab1.getTabGroupId()).thenReturn(TOKEN_1);
        mTabModel.addTab(
                mTab1, 0, TabLaunchType.FROM_TAB_GROUP_UI, TabCreationState.LIVE_IN_BACKGROUND);
        when(mTabGroupSyncService.getDeletedGroupIds()).thenReturn(new ArrayList<>());
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[0]);

        // Initialize. It should add the group to sync and add ID mapping to prefs.
        mStartupHelper.initializeTabGroupSync();
        verify(mTabGroupSyncService).createGroup(new LocalTabGroupId(TOKEN_1));
    }

    private class TestTabCreationDelegate implements TabCreationDelegate {
        private int mNextTabId;

        @Override
        public Tab createBackgroundTab(GURL url, Tab parent, int position) {
            return new MockTab(++mNextTabId, mProfile);
        }
    }
}

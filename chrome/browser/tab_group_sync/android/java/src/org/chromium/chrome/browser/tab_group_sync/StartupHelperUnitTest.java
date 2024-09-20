// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.tab_group_sync.ClosingSource;
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
    private static final LocalTabGroupId LOCAL_TAB_GROUP_ID_1 = new LocalTabGroupId(TOKEN_1);

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
        Mockito.doReturn(System.currentTimeMillis()).when(tab).getTimestampMillis();
        return tab;
    }

    @Before
    public void setUp() {
        mTabGroupSyncService = spy(new TestTabGroupSyncService());
        mTab1 = prepareTab(TAB_ID_1, ROOT_ID_1);
        mTab2 = prepareTab(TAB_ID_2, ROOT_ID_1);

        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);

        doAnswer(
                        invocation -> {
                            LocalTabGroupId groupId = invocation.getArgument(0);
                            mTabGroupSyncService.createGroup(groupId);
                            return null;
                        })
                .when(mRemoteMutationHelper)
                .createRemoteTabGroup(any());

        mStartupHelper =
                new StartupHelper(
                        mTabGroupModelFilter,
                        mTabGroupSyncService,
                        mLocalMutationHelper,
                        mRemoteMutationHelper);

        when(mTabGroupModelFilter.getRootIdFromStableId(eq(TOKEN_1))).thenReturn(ROOT_ID_1);
        when(mTabGroupModelFilter.getStableIdFromRootId(eq(ROOT_ID_1))).thenReturn(TOKEN_1);

        when(mTabGroupSyncService.getDeletedGroupIds()).thenReturn(new ArrayList<>());
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[0]);
    }

    private SavedTabGroup createRemoteGroupWithOneTab(Token mappedLocalId) {
        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        savedTabGroup.savedTabs.remove(1);
        if (mappedLocalId != null) savedTabGroup.localId = new LocalTabGroupId(mappedLocalId);
        String syncId = savedTabGroup.syncId;
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {syncId});
        when(mTabGroupSyncService.getGroup(savedTabGroup.localId)).thenReturn(savedTabGroup);
        return savedTabGroup;
    }

    private void createLocalGroupWithTwoTabs() {
        List<Integer> tabIds = new ArrayList<>();
        List<Tab> tabs = new ArrayList<>();
        tabIds.add(TAB_ID_1);
        tabIds.add(TAB_ID_2);
        tabs.add(mTab1);
        tabs.add(mTab2);
        when(mTabGroupModelFilter.getRelatedTabIds(ROOT_ID_1)).thenReturn(tabIds);
        when(mTabGroupModelFilter.getRelatedTabList(ROOT_ID_1)).thenReturn(tabs);
        when(mTabGroupModelFilter.getRelatedTabListForRootId(ROOT_ID_1)).thenReturn(tabs);

        when(mTab1.getTabGroupId()).thenReturn(TOKEN_1);
        when(mTab2.getTabGroupId()).thenReturn(TOKEN_1);
        mTabModel.addTab(
                mTab1, 0, TabLaunchType.FROM_TAB_GROUP_UI, TabCreationState.LIVE_IN_BACKGROUND);
        mTabModel.addTab(
                mTab2, 1, TabLaunchType.FROM_TAB_GROUP_UI, TabCreationState.LIVE_IN_BACKGROUND);
    }

    @Test
    public void testDeletedGroupsAreClosedOnStartup() {
        // Setup one local group that was deleted from sync.
        createLocalGroupWithTwoTabs();
        List<LocalTabGroupId> deletedIds = new ArrayList<>();
        deletedIds.add(LOCAL_TAB_GROUP_ID_1);
        when(mTabGroupSyncService.getDeletedGroupIds()).thenReturn(deletedIds);

        // Init. Deleted groups should be closed.
        mStartupHelper.initializeTabGroupSync();
        verify(mTabGroupSyncService).getDeletedGroupIds();
        verify(mLocalMutationHelper)
                .closeTabGroup(eq(LOCAL_TAB_GROUP_ID_1), eq(ClosingSource.CLEANED_UP_ON_STARTUP));
    }

    @Test
    public void testUpdateLocalGroupsToMatchSync() {
        // Setup one local group already mapped to sync.
        createLocalGroupWithTwoTabs();
        createRemoteGroupWithOneTab(TOKEN_1);

        // Init. The group should be updated.
        mStartupHelper.initializeTabGroupSync();
        verify(mTabGroupSyncService).getDeletedGroupIds();
        verify(mLocalMutationHelper)
                .reconcileGroupOnStartup(
                        argThat(
                                savedTabGroup ->
                                        savedTabGroup.localId.equals(LOCAL_TAB_GROUP_ID_1)));
    }

    @Test
    public void testCreatesRemoteGroupsForNewGroups() {
        // Setup one local group and no server group.
        createLocalGroupWithTwoTabs();

        // Initialize. It should add the group to sync and add ID mapping to prefs.
        mStartupHelper.initializeTabGroupSync();
        verify(mRemoteMutationHelper).createRemoteTabGroup(eq(LOCAL_TAB_GROUP_ID_1));
    }
}

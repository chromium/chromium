// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.mockito.ArgumentMatchers.eq;
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
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.url.GURL;

/** Unit tests for the {@link StartupHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StartupHelperUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private static final String SYNC_ID_1 = "SYNC_ID_1";

    @Mock private Profile mProfile;
    private MockTabModel mTabModel;
    private @Mock TabGroupModelFilter mTabGroupModelFilter;
    private TabGroupSyncService mTabGroupSyncService;
    private RemoteTabGroupMutationHelper mRemoteMutationHelper;
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
        mTab1 = prepareTab(1, 1);
        mTab2 = prepareTab(2, 2);
        Mockito.doReturn(new Token(2, 3)).when(mTab1).getTabGroupId();

        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        mRemoteMutationHelper =
                new RemoteTabGroupMutationHelper(mTabGroupModelFilter, mTabGroupSyncService);
        mStartupHelper =
                new StartupHelper(
                        mTabGroupModelFilter, mTabGroupSyncService, mRemoteMutationHelper);
    }

    @Test
    public void testNotifiesSyncOfIdMapingOnStartup() {
        when(mTab1.getTabGroupId()).thenReturn(new Token(2, 3));
        mTabModel.addTab(
                mTab1, 0, TabLaunchType.FROM_TAB_GROUP_UI, TabCreationState.LIVE_IN_BACKGROUND);
        when(mTabGroupModelFilter.getTabGroupSyncId(1)).thenReturn(SYNC_ID_1);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_ID_1});
        mStartupHelper.initializeTabGroupSync();
        verify(mTabGroupSyncService).updateLocalTabGroupId(eq(SYNC_ID_1), eq(1));
    }

    @Test
    public void testCreatesRemoteGroupsForNewGroupsAndUpdatesPrefs() {
        when(mTab1.getTabGroupId()).thenReturn(new Token(2, 3));
        mTabModel.addTab(
                mTab1, 0, TabLaunchType.FROM_TAB_GROUP_UI, TabCreationState.LIVE_IN_BACKGROUND);
        when(mTabGroupModelFilter.getTabGroupSyncId(1)).thenReturn(null);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[0]);

        // Initialize. It should add the group to sync and add ID mapping to prefs.
        mStartupHelper.initializeTabGroupSync();
        verify(mTabGroupSyncService).createGroup(1);
        verify(mTabGroupModelFilter)
                .setTabGroupSyncId(eq(1), eq(TestTabGroupSyncService.SYNC_ID_1));
    }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for the {@link RemoteTabGroupMutationHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RemoteTabGroupMutationHelperUnitTest {
    private static final int TAB_ID_1 = 1;
    private static final int TAB_ID_2 = 2;
    private static final int ROOT_ID_1 = 1;
    private static final Token TOKEN_1 = new Token(2, 3);
    private static final LocalTabGroupId LOCAL_TAB_GROUP_ID_1 = new LocalTabGroupId(TOKEN_1);
    private static final String TAB_TITLE_1 = "Tab Title";
    private static final GURL TAB_URL_1 = new GURL("https://google.com");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private Tab mTab1;
    private Tab mTab2;
    @Mock private Profile mProfile;
    private MockTabModel mTabModel;

    private @Mock TabGroupModelFilter mTabGroupModelFilter;
    private @Mock LocalTabGroupMutationHelper mLocalTabGroupMutationHelper;
    private TabGroupSyncService mTabGroupSyncService;
    private RemoteTabGroupMutationHelper mRemoteMutationHelper;
    private @Captor ArgumentCaptor<SavedTabGroup> mSavedTabGroupCaptor;

    private static Tab prepareTab(int tabId, int rootId) {
        Tab tab = Mockito.mock(Tab.class);
        Mockito.doReturn(tabId).when(tab).getId();
        Mockito.doReturn(rootId).when(tab).getRootId();
        Mockito.doReturn(TAB_URL_1).when(tab).getUrl();
        Mockito.doReturn(TAB_TITLE_1).when(tab).getTitle();
        return tab;
    }

    @Before
    public void setUp() {
        mTabGroupSyncService = spy(new TestTabGroupSyncService());
        mTab1 = prepareTab(TAB_ID_1, ROOT_ID_1);
        mTab2 = prepareTab(TAB_ID_2, ROOT_ID_1);
        Mockito.doReturn(TOKEN_1).when(mTab1).getTabGroupId();
        Mockito.doReturn(TOKEN_1).when(mTab2).getTabGroupId();

        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTab1);
        tabs.add(mTab2);
        when(mTabGroupModelFilter.getTabsInGroup(eq(TOKEN_1))).thenReturn(tabs);

        when(mTabGroupModelFilter.getRootIdFromTabGroupId(eq(TOKEN_1))).thenReturn(ROOT_ID_1);
        when(mTabGroupModelFilter.getTabGroupIdFromRootId(eq(ROOT_ID_1))).thenReturn(TOKEN_1);

        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        mRemoteMutationHelper =
                new RemoteTabGroupMutationHelper(
                        mTabGroupModelFilter, mTabGroupSyncService, mLocalTabGroupMutationHelper);
    }

    @Test
    public void testCreateRemoteTabGroup() {
        Mockito.doNothing().when(mTabGroupSyncService).addGroup(mSavedTabGroupCaptor.capture());
        mRemoteMutationHelper.createRemoteTabGroup(LOCAL_TAB_GROUP_ID_1);
        verify(mTabGroupSyncService).addGroup(any());
        Assert.assertEquals(2, mSavedTabGroupCaptor.getValue().savedTabs.size());
        Assert.assertEquals(LOCAL_TAB_GROUP_ID_1, mSavedTabGroupCaptor.getValue().localId);
        SavedTabGroupTab savedTab1 = mSavedTabGroupCaptor.getValue().savedTabs.get(0);
        SavedTabGroupTab savedTab2 = mSavedTabGroupCaptor.getValue().savedTabs.get(1);

        Assert.assertEquals(Integer.valueOf(TAB_ID_1), savedTab1.localId);
        Assert.assertEquals(TAB_TITLE_1, savedTab1.title);
        Assert.assertEquals(TAB_URL_1, savedTab1.url);
        Assert.assertEquals(mSavedTabGroupCaptor.getValue().syncId, savedTab1.syncGroupId);

        Assert.assertEquals(Integer.valueOf(TAB_ID_2), savedTab2.localId);
        Assert.assertEquals(TAB_TITLE_1, savedTab2.title);
        Assert.assertEquals(TAB_URL_1, savedTab2.url);
        Assert.assertEquals(mSavedTabGroupCaptor.getValue().syncId, savedTab2.syncGroupId);

        verify(mTabGroupModelFilter).getTabsInGroup(eq(TOKEN_1));
    }
}

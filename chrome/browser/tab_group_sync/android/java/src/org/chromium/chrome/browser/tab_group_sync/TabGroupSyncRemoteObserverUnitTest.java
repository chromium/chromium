// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Assert;
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

/** Unit tests for the {@link TabGroupSyncRemoteObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupSyncRemoteObserverUnitTest {
    private static final Token TOKEN_1 = new Token(2, 3);
    private static final Token TOKEN_2 = new Token(4, 4);
    private static final int TAB_ID_1 = 1;
    private static final int ROOT_ID_1 = 1;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    private MockTabModel mTabModel;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    private NavigationTracker mNavigationTracker;
    private LocalTabGroupMutationHelper mLocalMutationHelper;
    private TabGroupSyncRemoteObserver mRemoteObserver;
    private TestTabCreationDelegate mTabCreationDelegate;

    private boolean mEnabledLocalObservers;

    @Before
    public void setUp() {
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        mNavigationTracker = new NavigationTracker();
        mTabCreationDelegate = new TestTabCreationDelegate();
        mLocalMutationHelper =
                new LocalTabGroupMutationHelper(
                        mTabGroupModelFilter,
                        mTabGroupSyncService,
                        mTabCreationDelegate,
                        mNavigationTracker);
        mRemoteObserver =
                new TabGroupSyncRemoteObserver(
                        mTabGroupModelFilter,
                        mTabGroupSyncService,
                        mLocalMutationHelper,
                        enable -> {
                            mEnabledLocalObservers = enable;
                        },
                        () -> {});
        mEnabledLocalObservers = true;

        when(mTabGroupModelFilter.getRootIdFromStableId(any())).thenReturn(Tab.INVALID_TAB_ID);
        when(mTabGroupModelFilter.getRootIdFromStableId(eq(TOKEN_1))).thenReturn(ROOT_ID_1);
        when(mTabGroupModelFilter.getStableIdFromRootId(eq(ROOT_ID_1))).thenReturn(TOKEN_1);
    }

    @After
    public void tearDown() {
        // At the end of every method, the local observer should be reset back to observing.
        Assert.assertTrue(mEnabledLocalObservers);
    }

    private void addOneTab() {
        mTabModel.addTab(TAB_ID_1);
        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTabModel.getTabAt(0));
        when(mTabGroupModelFilter.getRelatedTabListForRootId(eq(ROOT_ID_1))).thenReturn(tabs);
    }

    @Test
    public void testTabGroupAdded() {
        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        mRemoteObserver.onTabGroupAdded(savedTabGroup);

        // Verify calls to create local tab group, and update ID mappings for group and tabs.
        verify(mTabGroupModelFilter)
                .mergeListOfTabsToGroup(anyList(), any(), anyBoolean(), anyBoolean());
        verify(mTabGroupModelFilter).setTabGroupColor(anyInt(), anyInt());
        verify(mTabGroupModelFilter).setTabGroupTitle(anyInt(), any());
        verify(mTabGroupSyncService).updateLocalTabGroupMapping(any(), any());
        verify(mTabGroupSyncService, times(2)).updateLocalTabId(any(), any(), anyInt());
    }

    @Test
    public void testTabGroupVisualsUpdated() {
        addOneTab();
        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        savedTabGroup.title = "Updated group";
        savedTabGroup.localId = new LocalTabGroupId(TOKEN_1);
        mRemoteObserver.onTabGroupUpdated(savedTabGroup);
        verify(mTabGroupModelFilter).setTabGroupTitle(eq(ROOT_ID_1), eq(savedTabGroup.title));
        verify(mTabGroupModelFilter).setTabGroupColor(eq(ROOT_ID_1), anyInt());
    }

    @Test
    public void testTabAdded() {
        addOneTab();
        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        savedTabGroup.localId = new LocalTabGroupId(TOKEN_1);
        mRemoteObserver.onTabGroupUpdated(savedTabGroup);
        verify(mTabGroupModelFilter).setTabGroupTitle(eq(ROOT_ID_1), eq(savedTabGroup.title));
        verify(mTabGroupModelFilter)
                .setTabGroupColor(
                        eq(ROOT_ID_1),
                        eq(org.chromium.components.tab_groups.TabGroupColorId.GREEN));
        verify(mTabGroupModelFilter, times(2)).mergeTabsToGroup(anyInt(), eq(ROOT_ID_1));
        verify(mTabGroupSyncService, times(1))
                .updateLocalTabId(eq(new LocalTabGroupId(TOKEN_1)), any(), eq(TAB_ID_1));
        verify(mTabModel).closeMultipleTabs(anyList(), eq(false));
    }

    @Test
    public void testTabGroupVisualsUpdatedForDifferentWindow() {
        addOneTab();
        SavedTabGroup savedTabGroup = TabGroupSyncTestUtils.createSavedTabGroup();
        savedTabGroup.title = "Updated group";
        savedTabGroup.localId = new LocalTabGroupId(TOKEN_2);
        mRemoteObserver.onTabGroupUpdated(savedTabGroup);
        verify(mTabGroupModelFilter, times(0)).setTabGroupTitle(anyInt(), anyString());
        verify(mTabGroupModelFilter, times(0)).setTabGroupColor(anyInt(), anyInt());
    }

    @Test
    public void testTabGroupRemoved() {
        mTabModel.addTab(TAB_ID_1);
        mRemoteObserver.onTabGroupRemoved(new LocalTabGroupId(TOKEN_1));
        verify(mTabModel).closeMultipleTabs(anyList(), eq(false));
    }

    @Test
    public void testTabGroupRemovedForDifferentWindow() {
        mTabModel.addTab(TAB_ID_1);
        mRemoteObserver.onTabGroupRemoved(new LocalTabGroupId(TOKEN_2));
        verify(mTabModel, times(0)).closeMultipleTabs(anyList(), eq(false));
    }

    private class TestTabCreationDelegate implements TabCreationDelegate {
        private int mNextTabId;

        @Override
        public Tab createBackgroundTab(GURL url, Tab parent, int position) {
            MockTab tab = new MockTab(++mNextTabId, mProfile);
            tab.setIsInitialized(true);
            tab.setUrl(url);
            tab.setRootId(parent == null ? tab.getId() : parent.getRootId());
            tab.setTitle("Tab Title");
            mTabModel.addTab(
                    tab, -1, TabLaunchType.FROM_TAB_GROUP_UI, TabCreationState.LIVE_IN_BACKGROUND);
            return tab;
        }
    }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncController.TabCreationDelegate;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for the {@link TabGroupSyncRemoteObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupSyncRemoteObserverUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    private MockTabModel mTabModel;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    private NavigationTracker mNavigationTracker;
    private LocalTabGroupMutationHelper mLocalMutationHelper;
    private TabGroupSyncRemoteObserver mRemoteObserver;
    private TestTabCreationDelegate mTabCreationDelegate;

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
                        mTabCreationDelegate,
                        mNavigationTracker,
                        enable -> {},
                        () -> {});
    }

    @Test
    public void testTabGroupAdded() {
        SavedTabGroup savedTabGroup = createSavedTabGroup();
        mRemoteObserver.onTabGroupAdded(savedTabGroup);

        // Verify calls to create local tab group, and update ID mappings for group and tabs.
        verify(mTabGroupModelFilter)
                .mergeListOfTabsToGroup(anyList(), any(), anyBoolean(), anyBoolean());
        verify(mTabGroupModelFilter).setTabGroupColor(anyInt(), anyInt());
        verify(mTabGroupModelFilter).setTabGroupTitle(anyInt(), any());
        verify(mTabGroupSyncService).updateLocalTabGroupId(any(), anyInt());
        verify(mTabGroupSyncService, times(2)).updateLocalTabId(anyInt(), any(), anyInt());
    }

    @Test
    public void testTabGroupVisualsUpdated() {
        SavedTabGroup savedTabGroup = createSavedTabGroup();
        int rootId = 1;
        mTabModel.addTab(1);
        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTabModel.getTabAt(0));
        when(mTabGroupModelFilter.getRelatedTabListForRootId(eq(rootId))).thenReturn(tabs);
        savedTabGroup.title = "Updated group";
        savedTabGroup.localId = rootId;
        mRemoteObserver.onTabGroupUpdated(savedTabGroup);
        verify(mTabGroupModelFilter).setTabGroupTitle(eq(rootId), eq(savedTabGroup.title));
        verify(mTabGroupModelFilter).setTabGroupColor(anyInt(), anyInt());
    }

    @Test
    public void testTabAdded() {
        SavedTabGroup savedTabGroup = createSavedTabGroup();
        int rootId = 1;
        mTabModel.addTab(1);
        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTabModel.getTabAt(0));

        savedTabGroup.localId = rootId;
        when(mTabGroupModelFilter.getRelatedTabListForRootId(eq(rootId))).thenReturn(tabs);
        mRemoteObserver.onTabGroupUpdated(savedTabGroup);
        verify(mTabGroupModelFilter).setTabGroupTitle(eq(rootId), eq(savedTabGroup.title));
        verify(mTabGroupModelFilter).setTabGroupColor(anyInt(), anyInt());
        verify(mTabGroupModelFilter, times(2)).mergeTabsToGroup(anyInt(), eq(rootId));
        verify(mTabGroupSyncService, times(2)).updateLocalTabId(eq(rootId), any(), anyInt());
        verify(mTabModel).closeMultipleTabs(anyList(), eq(false));
    }

    @Test
    public void testTabGroupRemoved() {
        int rootId = 1;
        mTabModel.addTab(1);
        mRemoteObserver.onTabGroupRemoved(rootId);
        verify(mTabModel).closeMultipleTabs(anyList(), anyBoolean());
    }

    private SavedTabGroup createSavedTabGroup() {
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = "Group_1";
        group.title = "Group 1";
        group.color = TabGroupColorId.GREEN;
        SavedTabGroupTab tab1 =
                createSavedTabGroupTab("Tab_1", group.syncId, "Tab 1", "https://foo1.com", 0);
        group.savedTabs.add(tab1);

        SavedTabGroupTab tab2 =
                createSavedTabGroupTab("Tab_2", group.syncId, "Tab 2", "https://foo2.com", 1);
        group.savedTabs.add(tab2);
        return group;
    }

    private SavedTabGroupTab createSavedTabGroupTab(
            String syncId, String syncGroupId, String title, String url, int position) {
        SavedTabGroupTab tab = new SavedTabGroupTab();
        tab.syncId = syncId;
        tab.syncGroupId = syncGroupId;
        tab.title = title;
        tab.url = new GURL(url);
        tab.position = position;
        return tab;
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

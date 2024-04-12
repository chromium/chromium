// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
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
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for the {@link TabGroupSyncLocalObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupSyncLocalObserverUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock TabModelSelector mTabModelSelector;
    @Mock private Profile mProfile;
    private MockTabModel mTabModel;
    private @Mock TabGroupModelFilter mTabGroupModelFilter;
    private TabGroupSyncService mTabGroupSyncService;
    private NavigationTracker mNavigationTracker;
    private RemoteTabGroupMutationHelper mRemoteMutationHelper;
    private TabGroupSyncLocalObserver mLocalObserver;

    private @Captor ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    private @Captor ArgumentCaptor<TabGroupModelFilterObserver> mTabGroupModelFilterObserverCaptor;

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

        Mockito.doNothing()
                .when(mTabGroupModelFilter)
                .addObserver(mTabModelObserverCaptor.capture());
        Mockito.doNothing()
                .when(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());
        mNavigationTracker = new NavigationTracker();
        mRemoteMutationHelper =
                new RemoteTabGroupMutationHelper(mTabGroupModelFilter, mTabGroupSyncService);
        mLocalObserver =
                new TabGroupSyncLocalObserver(
                        mTabModelSelector,
                        mTabGroupModelFilter,
                        mTabGroupSyncService,
                        mRemoteMutationHelper,
                        mNavigationTracker);
        mLocalObserver.enableObservers(true);
    }

    @After
    public void tearDown() {
        mLocalObserver.destroy();
    }

    @Test
    public void testTabAddedLocally() {
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab1,
                        TabLaunchType.FROM_RESTORE,
                        TabCreationState.LIVE_IN_BACKGROUND,
                        false);
        verify(mTabGroupSyncService, times(1)).addTab(eq(1), eq(1), any(), any(), anyInt());
    }

    @Test
    public void testCloseMultipleTabs() {
        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTab1);
        mTabModelObserverCaptor.getValue().onFinishingMultipleTabClosure(tabs);
    }

    @Test
    public void testDidMergeTabToGroup() {
        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(mTab1, 1);
        verify(mTabGroupSyncService, times(1)).createGroup(eq(mTab1.getRootId()));
    }

    @Test
    public void testDidMoveTabOutOfGroup() {
        // Add tab 1 and 2 to the tab model and create a group.
        mTabModel.addTab(
                mTab1, 0, TabLaunchType.FROM_TAB_GROUP_UI, TabCreationState.LIVE_IN_BACKGROUND);
        mTabModel.addTab(
                mTab2, 1, TabLaunchType.FROM_TAB_GROUP_UI, TabCreationState.LIVE_IN_BACKGROUND);
        Mockito.doReturn(1).when(mTab2).getRootId();

        // Move tab 2 out of group and verify.
        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, 0);
        verify(mTabGroupSyncService, times(1)).removeTab(eq(1), eq(2));
    }

    @Test
    public void testDidCreateNewGroup() {
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didCreateNewGroup(mTab1, mTabGroupModelFilter);
        verify(mTabGroupSyncService, times(1)).createGroup(eq(1));
    }

    @Test
    public void testDidMoveTabWithinGroup() {
        when(mTabGroupModelFilter.getIndexOfTabInGroup(mTab1)).thenReturn(0);
        mTabGroupModelFilterObserverCaptor.getValue().didMoveWithinGroup(mTab1, 0, 1);
        verify(mTabGroupSyncService, times(1))
                .updateTab(anyInt(), anyInt(), any(), any(), anyInt());
    }

    @Test
    public void testDidChangeTitle() {
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupTitle(mTab1.getRootId(), "New Title");
        verify(mTabGroupSyncService, times(1))
                .updateVisualData(eq(mTab1.getRootId()), any(), anyInt());
    }

    @Test
    public void testDidChangeColor() {
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupColor(mTab1.getRootId(), TabGroupColorId.RED);
        verify(mTabGroupSyncService, times(1))
                .updateVisualData(eq(mTab1.getRootId()), any(), anyInt());
    }
}

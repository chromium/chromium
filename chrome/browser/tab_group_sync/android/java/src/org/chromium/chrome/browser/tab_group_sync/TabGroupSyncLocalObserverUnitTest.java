// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
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
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver.DidRemoveTabGroupReason;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.tab_group_sync.ClosingSource;
import org.chromium.components.tab_group_sync.EventDetails;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Unit tests for the {@link TabGroupSyncLocalObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupSyncLocalObserverUnitTest {
    private static final int TAB_ID_1 = 1;
    private static final int TAB_ID_2 = 2;
    private static final int ROOT_ID_1 = 1;
    private static final int ROOT_ID_2 = 2;
    private static final Token TOKEN_1 = new Token(2, 3);
    private static final LocalTabGroupId LOCAL_TAB_GROUP_ID_1 = new LocalTabGroupId(TOKEN_1);
    private static final String TITLE_1 = "Group Title";
    private static final String TAB_TITLE_1 = "Tab Title";
    private static final GURL TAB_URL_1 = new GURL("https://google.com");

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
    private @Captor ArgumentCaptor<EventDetails> mEventDetailsCaptor;

    private Tab mTab1;
    private Tab mTab2;

    private UserActionTester mActionTester;

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
        mActionTester = new UserActionTester();
        mTabGroupSyncService = spy(new TestTabGroupSyncService());
        mTab1 = prepareTab(TAB_ID_1, ROOT_ID_1);
        mTab2 = prepareTab(TAB_ID_2, ROOT_ID_2);
        Mockito.doReturn(TOKEN_1).when(mTab1).getTabGroupId();
        Mockito.doReturn(TOKEN_1).when(mTab2).getTabGroupId();

        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab2)).thenReturn(true);
        when(mTabGroupModelFilter.getRootIdFromStableId(eq(TOKEN_1))).thenReturn(ROOT_ID_1);
        when(mTabGroupModelFilter.getStableIdFromRootId(eq(ROOT_ID_1))).thenReturn(TOKEN_1);

        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);

        Mockito.doNothing()
                .when(mTabGroupModelFilter)
                .addObserver(mTabModelObserverCaptor.capture());
        Mockito.doNothing()
                .when(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());
        Mockito.doNothing()
                .when(mTabGroupSyncService)
                .recordTabGroupEvent(mEventDetailsCaptor.capture());
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
        mActionTester.tearDown();
    }

    @Test
    public void testDidSelectTabRemote() {
        // Stub the bare minimum.
        SavedTabGroup savedGroup = new SavedTabGroup();
        String remoteGuid = "remote_device";
        savedGroup.creatorCacheGuid = remoteGuid;
        savedGroup.lastUpdaterCacheGuid = remoteGuid;
        SavedTabGroupTab savedTab1 = new SavedTabGroupTab();
        SavedTabGroupTab savedTab2 = new SavedTabGroupTab();
        savedTab2.localId = TAB_ID_2;
        savedGroup.savedTabs.add(savedTab1);
        savedGroup.savedTabs.add(savedTab2);
        when(mTabGroupSyncService.getGroup(LOCAL_TAB_GROUP_ID_1)).thenReturn(savedGroup);
        when(mTabGroupSyncService.isRemoteDevice(remoteGuid)).thenReturn(true);

        List<String> actions =
                List.of(
                        "TabGroups.Sync.SelectedTabInRemotelyCreatedGroup",
                        "MobileCrossDeviceTabJourney",
                        "TabGroups.Sync.SelectedRemotelyUpdatedTabInSession");
        for (String action : actions) {
            assertEquals(
                    "Expected no actions for " + action, 0, mActionTester.getActionCount(action));
        }
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab2, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);
        verify(mTabGroupSyncService).onTabSelected(eq(LOCAL_TAB_GROUP_ID_1), eq(TAB_ID_2));
        for (String action : actions) {
            assertEquals(
                    "Expected one action for " + action, 1, mActionTester.getActionCount(action));
        }
    }

    @Test
    public void testDidSelectTabLocal() {
        // Stub the bare minimum.
        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.creatorCacheGuid = TestTabGroupSyncService.LOCAL_DEVICE_CACHE_GUID;
        SavedTabGroupTab savedTab1 = new SavedTabGroupTab();
        SavedTabGroupTab savedTab2 = new SavedTabGroupTab();
        savedTab2.localId = TAB_ID_2;
        savedGroup.savedTabs.add(savedTab1);
        savedGroup.savedTabs.add(savedTab2);
        when(mTabGroupSyncService.getGroup(LOCAL_TAB_GROUP_ID_1)).thenReturn(savedGroup);

        String action = "TabGroups.Sync.SelectedTabInLocallyCreatedGroup";
        assertEquals(0, mActionTester.getActionCount(action));
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab2, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);
        assertEquals(1, mActionTester.getActionCount(action));
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
        verify(mTabGroupSyncService, times(1))
                .addTab(
                        eq(LOCAL_TAB_GROUP_ID_1),
                        eq(TAB_ID_1),
                        eq(TAB_TITLE_1),
                        eq(TAB_URL_1),
                        anyInt());
    }

    @Test
    public void testTabAddedLocally_NonSaveableUrl() {
        Mockito.doReturn(new GURL("ftp://someurl.com")).when(mTab1).getUrl();
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab1,
                        TabLaunchType.FROM_RESTORE,
                        TabCreationState.LIVE_IN_BACKGROUND,
                        false);
        verify(mTabGroupSyncService, times(1))
                .addTab(
                        eq(LOCAL_TAB_GROUP_ID_1),
                        eq(TAB_ID_1),
                        eq(TabGroupSyncUtils.UNSAVEABLE_TAB_TITLE),
                        eq(TabGroupSyncUtils.UNSAVEABLE_URL_OVERRIDE),
                        anyInt());
    }

    @Test
    public void testFinishedClosingTabGroup_Hiding() {
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .committedTabGroupClosure(TOKEN_1, /* wasHiding= */ true);
        verify(mTabGroupSyncService)
                .removeLocalTabGroupMapping(LOCAL_TAB_GROUP_ID_1, ClosingSource.CLOSED_BY_USER);
    }

    @Test
    public void testFinishedClosingTabGroup_Deleted() {
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .committedTabGroupClosure(TOKEN_1, /* wasHiding= */ false);
        verify(mTabGroupSyncService)
                .removeLocalTabGroupMapping(LOCAL_TAB_GROUP_ID_1, ClosingSource.DELETED_BY_USER);
        verify(mTabGroupSyncService).removeGroup(LOCAL_TAB_GROUP_ID_1);
    }

    @Test
    public void testCloseMultipleTabs_EmptyList() {
        mTabModelObserverCaptor
                .getValue()
                .onFinishingMultipleTabClosure(new ArrayList<Tab>(), /* canRestore= */ true);

        verify(mTabGroupSyncService, never()).removeTab(any(), anyInt());
    }

    @Test
    public void testCloseMultipleTabs_HidingTabGroup() {
        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTab1);
        when(mTabGroupModelFilter.getLazyAllTabGroupIdsInComprehensiveModel(any()))
                .thenReturn(LazyOneshotSupplier.fromValue(new HashSet<Token>()));
        when(mTabGroupModelFilter.isTabGroupHiding(TOKEN_1)).thenReturn(true);
        mTabModelObserverCaptor
                .getValue()
                .onFinishingMultipleTabClosure(tabs, /* canRestore= */ true);

        verify(mTabGroupSyncService, never()).removeTab(LOCAL_TAB_GROUP_ID_1, TAB_ID_1);
    }

    @Test
    public void testCloseMultipleTabs_HidingTabGroup_NotLastTabInGroup() {
        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTab1);
        when(mTabGroupModelFilter.getLazyAllTabGroupIdsInComprehensiveModel(any()))
                .thenReturn(LazyOneshotSupplier.fromValue(Set.of(TOKEN_1)));
        when(mTabGroupModelFilter.isTabGroupHiding(TOKEN_1)).thenReturn(true);
        mTabModelObserverCaptor
                .getValue()
                .onFinishingMultipleTabClosure(tabs, /* canRestore= */ true);

        // In this scenario the tab group is hiding, but tabs were closed in multiple phases. We
        // should commit any tab removals as "deletions" from the group except for the last event
        // that actually hides the group.
        verify(mTabGroupSyncService).removeTab(LOCAL_TAB_GROUP_ID_1, TAB_ID_1);
    }

    @Test
    public void testCloseMultipleTabs_DeletingTabGroup() {
        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTab1);
        when(mTabGroupModelFilter.isTabGroupHiding(TOKEN_1)).thenReturn(false);
        mTabModelObserverCaptor
                .getValue()
                .onFinishingMultipleTabClosure(tabs, /* canRestore= */ true);

        verify(mTabGroupSyncService).removeTab(LOCAL_TAB_GROUP_ID_1, TAB_ID_1);
    }

    @Test
    public void testDidMergeTabToGroup() {
        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(mTab1, 1);
        verify(mTabGroupSyncService, times(1)).createGroup(eq(LOCAL_TAB_GROUP_ID_1));
        verify(mTabGroupModelFilter, never()).getRelatedTabList(anyInt());
        verify(mTabGroupModelFilter, times(1)).getRelatedTabListForRootId(1);
    }

    @Test
    public void testDidMoveTabOutOfGroup() {
        // Add tab 1 and 2 to the tab model and create a group.
        mTabModel.addTab(
                mTab1, 0, TabLaunchType.FROM_TAB_GROUP_UI, TabCreationState.LIVE_IN_BACKGROUND);
        mTabModel.addTab(
                mTab2, 1, TabLaunchType.FROM_TAB_GROUP_UI, TabCreationState.LIVE_IN_BACKGROUND);
        when(mTabGroupModelFilter.getTabAt(0)).thenReturn(mTab1);

        // Move tab 2 out of group and verify.
        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, 0);
        verify(mTabGroupSyncService, times(1)).removeTab(eq(LOCAL_TAB_GROUP_ID_1), eq(2));
    }

    @Test
    public void testDidCreateNewGroup() {
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didCreateNewGroup(mTab1, mTabGroupModelFilter);
        verify(mTabGroupSyncService, times(1)).createGroup(eq(LOCAL_TAB_GROUP_ID_1));
    }

    @Test
    public void testDidMoveTabWithinGroup() {
        when(mTabGroupModelFilter.getIndexOfTabInGroup(mTab1)).thenReturn(0);
        mTabGroupModelFilterObserverCaptor.getValue().didMoveWithinGroup(mTab1, 0, 1);
        verify(mTabGroupSyncService, times(1))
                .moveTab(eq(LOCAL_TAB_GROUP_ID_1), eq(TAB_ID_1), anyInt());
    }

    @Test
    public void testDidChangeTitle() {
        // Valid title.
        when(mTabGroupModelFilter.getTabGroupTitle(ROOT_ID_1)).thenReturn(TITLE_1);
        mTabGroupModelFilterObserverCaptor.getValue().didChangeTabGroupTitle(ROOT_ID_1, TITLE_1);
        verify(mTabGroupSyncService, times(1))
                .updateVisualData(eq(LOCAL_TAB_GROUP_ID_1), eq(TITLE_1), anyInt());

        // Null title.
        when(mTabGroupModelFilter.getTabGroupTitle(ROOT_ID_1)).thenReturn(null);
        mTabGroupModelFilterObserverCaptor.getValue().didChangeTabGroupTitle(ROOT_ID_1, null);
        verify(mTabGroupSyncService, times(1))
                .updateVisualData(eq(LOCAL_TAB_GROUP_ID_1), eq(new String()), anyInt());
    }

    @Test
    public void testDidChangeColor() {
        // Mock that we have a stored color (red) stored with reference to ROOT_ID_1.
        when(mTabGroupModelFilter.getTabGroupColor(ROOT_ID_1)).thenReturn(TabGroupColorId.RED);

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupColor(ROOT_ID_1, TabGroupColorId.RED);
        verify(mTabGroupSyncService, times(1))
                .updateVisualData(eq(LOCAL_TAB_GROUP_ID_1), any(), eq(TabGroupColorId.RED));
    }

    @Test
    public void testDidChangeTitleAndColorForNonExistingGroup() {
        // Handle updates for non-existing groups.
        when(mTabGroupModelFilter.getTabGroupTitle(12)).thenReturn(null);
        mTabGroupModelFilterObserverCaptor.getValue().didChangeTabGroupTitle(12, TITLE_1);
        verify(mTabGroupSyncService, never()).updateVisualData(any(), any(), anyInt());

        // Handle updates for non-existing groups.
        when(mTabGroupModelFilter.getTabGroupColorWithFallback(12))
                .thenReturn(TabGroupColorId.BLUE);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupColor(12, TabGroupColorId.BLUE);
        verify(mTabGroupSyncService, never()).updateVisualData(any(), any(), anyInt());
    }

    @Test
    public void testDidRemoveGroup_Close() {
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didRemoveTabGroup(ROOT_ID_1, TOKEN_1, DidRemoveTabGroupReason.CLOSE);
        verify(mTabGroupSyncService, never()).removeGroup(LOCAL_TAB_GROUP_ID_1);
    }

    @Test
    public void testDidRemoveGroup_Merge() {
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didRemoveTabGroup(ROOT_ID_1, TOKEN_1, DidRemoveTabGroupReason.MERGE);
        verify(mTabGroupSyncService).removeGroup(LOCAL_TAB_GROUP_ID_1);
    }

    @Test
    public void testDidRemoveGroup_Ungroup() {
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didRemoveTabGroup(ROOT_ID_1, TOKEN_1, DidRemoveTabGroupReason.UNGROUP);
        verify(mTabGroupSyncService).removeGroup(LOCAL_TAB_GROUP_ID_1);
    }
}

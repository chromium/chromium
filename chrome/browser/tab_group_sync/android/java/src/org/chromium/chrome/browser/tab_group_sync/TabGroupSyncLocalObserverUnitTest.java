// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Token;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver.DidRemoveTabGroupReason;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
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
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Unit tests for the {@link TabGroupSyncLocalObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupSyncLocalObserverUnitTest {
    private static final int TAB_ID_1 = 1;
    private static final int TAB_ID_2 = 2;
    private static final int TAB_ID_3 = 3;
    private static final int ROOT_ID_1 = 1;
    private static final int ROOT_ID_2 = 2;
    private static final int ROOT_ID_3 = 3;
    private static final Token TOKEN_1 = new Token(2, 3);
    private static final LocalTabGroupId LOCAL_TAB_GROUP_ID_1 = new LocalTabGroupId(TOKEN_1);
    private static final String SYNC_ID = "sync_id";
    private static final String TITLE_1 = "Group Title";
    private static final String TAB_TITLE_1 = "Tab Title";
    private static final GURL TAB_URL_1 = new GURL("https://google.com");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock TabModelSelector mTabModelSelector;
    private @Mock Profile mProfile;
    private @Mock TabGroupModelFilter mTabGroupModelFilter;
    private @Mock LocalTabGroupMutationHelper mLocalTabGroupMutationHelper;
    private MockTabModel mTabModel;
    private TabGroupSyncService mTabGroupSyncService;
    private NavigationTracker mNavigationTracker;
    private RemoteTabGroupMutationHelper mRemoteMutationHelper;
    private TabGroupSyncLocalObserver mLocalObserver;

    private @Captor ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    private @Captor ArgumentCaptor<TabGroupModelFilterObserver> mTabGroupModelFilterObserverCaptor;
    private @Captor ArgumentCaptor<EventDetails> mEventDetailsCaptor;
    private @Captor ArgumentCaptor<SavedTabGroup> mSavedTabGroupCaptor;

    private Tab mTab1;
    private Tab mTab2;
    private Tab mTab3;

    private UserActionTester mActionTester;

    private static Tab prepareTab(int tabId, int rootId, @Nullable Token tabGroupId) {
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(tabId);
        when(tab.getRootId()).thenReturn(rootId);
        when(tab.getUrl()).thenReturn(TAB_URL_1);
        when(tab.getTitle()).thenReturn(TAB_TITLE_1);
        when(tab.getTabGroupId()).thenReturn(tabGroupId);
        return tab;
    }

    @Before
    public void setUp() {
        mActionTester = new UserActionTester();
        mTabGroupSyncService = spy(new TestTabGroupSyncService());
        mTab1 = prepareTab(TAB_ID_1, ROOT_ID_1, TOKEN_1);
        mTab2 = prepareTab(TAB_ID_2, ROOT_ID_1, TOKEN_1);
        mTab3 = prepareTab(TAB_ID_3, ROOT_ID_3, null);

        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab2)).thenReturn(true);
        when(mTabGroupModelFilter.getRootIdFromTabGroupId(eq(TOKEN_1))).thenReturn(ROOT_ID_1);
        when(mTabGroupModelFilter.getTabGroupIdFromRootId(eq(ROOT_ID_1))).thenReturn(TOKEN_1);

        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);

        doNothing().when(mTabGroupModelFilter).addObserver(mTabModelObserverCaptor.capture());
        doNothing()
                .when(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());
        doNothing().when(mTabGroupSyncService).recordTabGroupEvent(mEventDetailsCaptor.capture());
        mNavigationTracker = new NavigationTracker();
        mRemoteMutationHelper =
                new RemoteTabGroupMutationHelper(
                        mTabGroupModelFilter, mTabGroupSyncService, mLocalTabGroupMutationHelper);
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
        savedTab2.lastUpdaterCacheGuid = remoteGuid;
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
        verify(mTabGroupSyncService)
                .onTabSelected(eq(LOCAL_TAB_GROUP_ID_1), eq(TAB_ID_2), eq(TAB_TITLE_1));
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
        verify(mTabGroupSyncService)
                .onTabSelected(eq(LOCAL_TAB_GROUP_ID_1), eq(TAB_ID_2), eq(TAB_TITLE_1));

        // Select a non-grouped tab.
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab3, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);
        assertEquals(1, mActionTester.getActionCount(action));
        verify(mTabGroupSyncService).onTabSelected(eq(null), eq(TAB_ID_3), eq(TAB_TITLE_1));
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
        when(mTab1.getUrl()).thenReturn(new GURL("ftp://someurl.com"));
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
    public void testWillCloseTab() {
        when(mTabGroupModelFilter.isTabGroupHiding(TOKEN_1)).thenReturn(false);

        // Not alone.
        mTabModelObserverCaptor.getValue().willCloseTab(mTab1, /* didCloseAlone= */ false);
        verify(mTabGroupSyncService, never()).removeTab(any(), anyInt());

        // Alone.
        mTabModelObserverCaptor.getValue().willCloseTab(mTab1, /* didCloseAlone= */ true);
        verify(mTabGroupSyncService).removeTab(LOCAL_TAB_GROUP_ID_1, TAB_ID_1);
    }

    @Test
    public void testWillCloseMultipleTabs_GroupDeleted() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));

        when(mTabGroupModelFilter.isTabGroupHiding(TOKEN_1)).thenReturn(false);
        mTabModelObserverCaptor.getValue().willCloseMultipleTabs(/* allowUndo= */ true, tabs);

        verify(mTabGroupSyncService).removeTab(LOCAL_TAB_GROUP_ID_1, TAB_ID_1);
        verify(mTabGroupSyncService).removeTab(LOCAL_TAB_GROUP_ID_1, TAB_ID_2);
    }

    @Test
    public void testWillCloseMultipleTabs_IncompleteGroupHiding() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        when(mTabGroupModelFilter.getLazyAllTabGroupIds(any(), anyBoolean()))
                .thenReturn(LazyOneshotSupplier.fromValue(Set.of(TOKEN_1)));
        when(mTabGroupModelFilter.isTabGroupHiding(TOKEN_1)).thenReturn(true);
        mTabModelObserverCaptor.getValue().willCloseMultipleTabs(/* allowUndo= */ true, tabs);

        verify(mTabGroupSyncService).removeTab(LOCAL_TAB_GROUP_ID_1, TAB_ID_1);
    }

    @Test
    public void testWillCloseMultipleTabs_GroupHiding() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        when(mTabGroupModelFilter.getLazyAllTabGroupIds(any(), anyBoolean()))
                .thenReturn(LazyOneshotSupplier.fromValue(new HashSet<Token>()));
        when(mTabGroupModelFilter.isTabGroupHiding(TOKEN_1)).thenReturn(true);
        mTabModelObserverCaptor.getValue().willCloseMultipleTabs(/* allowUndo= */ true, tabs);

        verify(mTabGroupSyncService, never()).removeTab(any(), anyInt());
    }

    @Test
    public void testWillCloseAllTabs() {
        when(mTabGroupModelFilter.isTabGroupHiding(TOKEN_1)).thenReturn(false);
        mTabModel.addTab(
                mTab1, 0, TabLaunchType.FROM_TAB_GROUP_UI, TabCreationState.LIVE_IN_BACKGROUND);
        mTabModel.addTab(
                mTab2, 1, TabLaunchType.FROM_TAB_GROUP_UI, TabCreationState.LIVE_IN_BACKGROUND);
        mTabModelObserverCaptor.getValue().willCloseAllTabs(/* incognito= */ false);

        verify(mTabGroupSyncService).removeTab(LOCAL_TAB_GROUP_ID_1, TAB_ID_1);
        verify(mTabGroupSyncService).removeTab(LOCAL_TAB_GROUP_ID_1, TAB_ID_2);
    }

    @Test
    public void testWillCloseTabGroup_NullGroup() {
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .willCloseTabGroup(TOKEN_1, /* isHiding= */ true);
        verify(mTabGroupSyncService, never()).removeLocalTabGroupMapping(any(), anyInt());
        assertFalse(mLocalObserver.hasAnyPendingTabGroupClosuresForTesting());
    }

    @Test
    public void testWillCloseTabGroup_Hiding() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.localId = LOCAL_TAB_GROUP_ID_1;
        savedTabGroup.syncId = SYNC_ID;
        SavedTabGroupTab savedTab1 = new SavedTabGroupTab();
        savedTab1.localId = TAB_ID_1;
        SavedTabGroupTab savedTab2 = new SavedTabGroupTab();
        savedTab2.localId = TAB_ID_2;
        savedTabGroup.savedTabs.add(savedTab1);
        savedTabGroup.savedTabs.add(savedTab2);
        when(mTabGroupSyncService.getGroup(LOCAL_TAB_GROUP_ID_1)).thenReturn(savedTabGroup);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .willCloseTabGroup(TOKEN_1, /* isHiding= */ true);
        verify(mTabGroupSyncService, never()).removeLocalTabGroupMapping(any(), anyInt());
        verify(mTabGroupSyncService, never()).removeGroup(anyString());
        assertTrue(mLocalObserver.hasAnyPendingTabGroupClosuresForTesting());
    }

    @Test
    public void testWillCloseTabGroup_Deleted() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.localId = LOCAL_TAB_GROUP_ID_1;
        savedTabGroup.syncId = SYNC_ID;
        SavedTabGroupTab savedTab1 = new SavedTabGroupTab();
        savedTab1.localId = TAB_ID_1;
        SavedTabGroupTab savedTab2 = new SavedTabGroupTab();
        savedTab2.localId = TAB_ID_2;
        savedTabGroup.savedTabs.add(savedTab1);
        savedTabGroup.savedTabs.add(savedTab2);
        when(mTabGroupSyncService.getGroup(LOCAL_TAB_GROUP_ID_1)).thenReturn(savedTabGroup);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .willCloseTabGroup(TOKEN_1, /* isHiding= */ false);
        verify(mTabGroupSyncService)
                .removeLocalTabGroupMapping(LOCAL_TAB_GROUP_ID_1, ClosingSource.DELETED_BY_USER);
        verify(mTabGroupSyncService).removeGroup(SYNC_ID);
        assertTrue(mLocalObserver.hasAnyPendingTabGroupClosuresForTesting());
    }

    @Test
    public void testGroupHide_Undone() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.localId = LOCAL_TAB_GROUP_ID_1;
        savedTabGroup.syncId = SYNC_ID;
        SavedTabGroupTab savedTab1 = new SavedTabGroupTab();
        savedTab1.localId = TAB_ID_1;
        SavedTabGroupTab savedTab2 = new SavedTabGroupTab();
        savedTab2.localId = TAB_ID_2;
        savedTabGroup.savedTabs.add(savedTab1);
        savedTabGroup.savedTabs.add(savedTab2);
        when(mTabGroupSyncService.getGroup(LOCAL_TAB_GROUP_ID_1)).thenReturn(savedTabGroup);
        boolean hiding = true;
        when(mTabGroupModelFilter.isTabGroupHiding(TOKEN_1)).thenReturn(hiding);
        when(mTabGroupModelFilter.getTabsInGroup(TOKEN_1)).thenReturn(List.of(mTab1, mTab2));
        TabModelObserver modelObserver = mTabModelObserverCaptor.getValue();
        TabGroupModelFilterObserver groupObserver = mTabGroupModelFilterObserverCaptor.getValue();

        // Close starting.
        groupObserver.willCloseTabGroup(TOKEN_1, hiding);
        assertTrue(mLocalObserver.hasAnyPendingTabGroupClosuresForTesting());
        modelObserver.willCloseTab(mTab1, /* didCloseAlone= */ false);
        modelObserver.willCloseTab(mTab2, /* didCloseAlone= */ false);

        // Undo.
        modelObserver.tabClosureUndone(mTab1);
        assertTrue(mLocalObserver.hasAnyPendingTabGroupClosuresForTesting());
        modelObserver.tabClosureUndone(mTab2);
        assertFalse(mLocalObserver.hasAnyPendingTabGroupClosuresForTesting());

        ShadowLooper.runUiThreadTasks();
        verify(mLocalTabGroupMutationHelper).updateTabGroup(savedTabGroup);
        verify(mTabGroupSyncService, never()).addGroup(any());
        verify(mTabGroupSyncService, never()).removeLocalTabGroupMapping(any(), anyInt());
    }

    @Test
    public void testGroupHide_Undone_DeletedRemotely() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.localId = LOCAL_TAB_GROUP_ID_1;
        savedTabGroup.syncId = SYNC_ID;
        SavedTabGroupTab savedTab1 = new SavedTabGroupTab();
        savedTab1.localId = TAB_ID_1;
        SavedTabGroupTab savedTab2 = new SavedTabGroupTab();
        savedTab2.localId = TAB_ID_2;
        savedTabGroup.savedTabs.add(savedTab1);
        savedTabGroup.savedTabs.add(savedTab2);
        when(mTabGroupSyncService.getGroup(LOCAL_TAB_GROUP_ID_1)).thenReturn(savedTabGroup);
        boolean hiding = true;
        when(mTabGroupModelFilter.isTabGroupHiding(TOKEN_1)).thenReturn(hiding);
        when(mTabGroupModelFilter.getTabsInGroup(TOKEN_1)).thenReturn(List.of(mTab1, mTab2));
        TabModelObserver modelObserver = mTabModelObserverCaptor.getValue();
        TabGroupModelFilterObserver groupObserver = mTabGroupModelFilterObserverCaptor.getValue();

        // Close starting.
        groupObserver.willCloseTabGroup(TOKEN_1, hiding);
        assertTrue(mLocalObserver.hasAnyPendingTabGroupClosuresForTesting());
        modelObserver.willCloseTab(mTab1, /* didCloseAlone= */ false);
        modelObserver.willCloseTab(mTab2, /* didCloseAlone= */ false);

        // During the undo window the group was deleted remotely.
        when(mTabGroupSyncService.getGroup(LOCAL_TAB_GROUP_ID_1)).thenReturn(null);

        // Undo.
        modelObserver.tabClosureUndone(mTab1);
        assertTrue(mLocalObserver.hasAnyPendingTabGroupClosuresForTesting());
        modelObserver.tabClosureUndone(mTab2);
        assertFalse(mLocalObserver.hasAnyPendingTabGroupClosuresForTesting());

        // This should no-op since restoring could cause issues as the group was deleted remotely.
        ShadowLooper.runUiThreadTasks();
        verify(mLocalTabGroupMutationHelper, never()).updateTabGroup(any());
        verify(mTabGroupSyncService, never()).addGroup(any());
        verify(mTabGroupSyncService, never()).removeLocalTabGroupMapping(any(), anyInt());
    }

    @Test
    public void testGroupHide_Committed() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.localId = LOCAL_TAB_GROUP_ID_1;
        savedTabGroup.syncId = SYNC_ID;
        SavedTabGroupTab savedTab1 = new SavedTabGroupTab();
        savedTab1.localId = TAB_ID_1;
        SavedTabGroupTab savedTab2 = new SavedTabGroupTab();
        savedTab2.localId = TAB_ID_2;
        savedTabGroup.savedTabs.add(savedTab1);
        savedTabGroup.savedTabs.add(savedTab2);
        when(mTabGroupSyncService.getGroup(LOCAL_TAB_GROUP_ID_1)).thenReturn(savedTabGroup);
        boolean hiding = true;
        when(mTabGroupModelFilter.isTabGroupHiding(TOKEN_1)).thenReturn(hiding);
        TabModelObserver modelObserver = mTabModelObserverCaptor.getValue();
        TabGroupModelFilterObserver groupObserver = mTabGroupModelFilterObserverCaptor.getValue();

        // Close starting.
        groupObserver.willCloseTabGroup(TOKEN_1, hiding);
        assertTrue(mLocalObserver.hasAnyPendingTabGroupClosuresForTesting());
        modelObserver.willCloseTab(mTab1, /* didCloseAlone= */ false);
        modelObserver.willCloseTab(mTab2, /* didCloseAlone= */ false);

        // Commit.
        modelObserver.onFinishingMultipleTabClosure(List.of(mTab1, mTab2), /* canRestore= */ true);
        groupObserver.committedTabGroupClosure(TOKEN_1, hiding);
        assertFalse(mLocalObserver.hasAnyPendingTabGroupClosuresForTesting());
        verify(mTabGroupSyncService)
                .removeLocalTabGroupMapping(LOCAL_TAB_GROUP_ID_1, ClosingSource.CLOSED_BY_USER);

        ShadowLooper.runUiThreadTasks();
        verify(mLocalTabGroupMutationHelper, never()).updateTabGroup(any());
    }

    @Test
    public void testGroupDelete_Undone() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.localId = LOCAL_TAB_GROUP_ID_1;
        savedTabGroup.syncId = SYNC_ID;
        SavedTabGroupTab savedTab1 = new SavedTabGroupTab();
        savedTab1.localId = TAB_ID_1;
        SavedTabGroupTab savedTab2 = new SavedTabGroupTab();
        savedTab2.localId = TAB_ID_2;
        savedTabGroup.savedTabs.add(savedTab1);
        savedTabGroup.savedTabs.add(savedTab2);
        when(mTabGroupSyncService.getGroup(LOCAL_TAB_GROUP_ID_1)).thenReturn(savedTabGroup);
        boolean hiding = false;
        when(mTabGroupModelFilter.isTabGroupHiding(TOKEN_1)).thenReturn(hiding);
        TabModelObserver modelObserver = mTabModelObserverCaptor.getValue();
        TabGroupModelFilterObserver groupObserver = mTabGroupModelFilterObserverCaptor.getValue();

        // Close starting.
        groupObserver.willCloseTabGroup(TOKEN_1, hiding);
        verify(mTabGroupSyncService)
                .removeLocalTabGroupMapping(LOCAL_TAB_GROUP_ID_1, ClosingSource.DELETED_BY_USER);
        verify(mTabGroupSyncService).removeGroup(SYNC_ID);
        assertTrue(mLocalObserver.hasAnyPendingTabGroupClosuresForTesting());
        modelObserver.willCloseTab(mTab1, /* didCloseAlone= */ false);
        modelObserver.willCloseTab(mTab2, /* didCloseAlone= */ false);

        // Undo.
        modelObserver.tabClosureUndone(mTab1);
        assertTrue(mLocalObserver.hasAnyPendingTabGroupClosuresForTesting());
        modelObserver.tabClosureUndone(mTab2);
        assertFalse(mLocalObserver.hasAnyPendingTabGroupClosuresForTesting());

        // Group is recreated not reconciled.
        ShadowLooper.runUiThreadTasks();
        verify(mTabGroupSyncService).addGroup(any());
        verify(mLocalTabGroupMutationHelper, never()).updateTabGroup(any());
    }

    @Test
    public void testGroupDelete_Committed() {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.localId = LOCAL_TAB_GROUP_ID_1;
        savedTabGroup.syncId = SYNC_ID;
        SavedTabGroupTab savedTab1 = new SavedTabGroupTab();
        savedTab1.localId = TAB_ID_1;
        SavedTabGroupTab savedTab2 = new SavedTabGroupTab();
        savedTab2.localId = TAB_ID_2;
        savedTabGroup.savedTabs.add(savedTab1);
        savedTabGroup.savedTabs.add(savedTab2);
        when(mTabGroupSyncService.getGroup(LOCAL_TAB_GROUP_ID_1)).thenReturn(savedTabGroup);
        boolean hiding = false;
        when(mTabGroupModelFilter.isTabGroupHiding(TOKEN_1)).thenReturn(hiding);
        TabModelObserver modelObserver = mTabModelObserverCaptor.getValue();
        TabGroupModelFilterObserver groupObserver = mTabGroupModelFilterObserverCaptor.getValue();

        // Close starting.
        groupObserver.willCloseTabGroup(TOKEN_1, hiding);
        verify(mTabGroupSyncService)
                .removeLocalTabGroupMapping(LOCAL_TAB_GROUP_ID_1, ClosingSource.DELETED_BY_USER);
        verify(mTabGroupSyncService).removeGroup(SYNC_ID);
        assertTrue(mLocalObserver.hasAnyPendingTabGroupClosuresForTesting());
        modelObserver.willCloseTab(mTab1, /* didCloseAlone= */ false);
        modelObserver.willCloseTab(mTab2, /* didCloseAlone= */ false);

        // Commit.
        modelObserver.onFinishingMultipleTabClosure(List.of(mTab1, mTab2), /* canRestore= */ true);
        groupObserver.committedTabGroupClosure(TOKEN_1, hiding);
        assertFalse(mLocalObserver.hasAnyPendingTabGroupClosuresForTesting());

        ShadowLooper.runUiThreadTasks();
        verify(mTabGroupSyncService, never()).addGroup(any());
        verify(mLocalTabGroupMutationHelper, never()).updateTabGroup(any());
    }

    @Test
    public void testDidMergeTabToGroup() {
        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(mTab1);
        verify(mTabGroupSyncService, times(1)).addGroup(mSavedTabGroupCaptor.capture());
        Assert.assertEquals(LOCAL_TAB_GROUP_ID_1, mSavedTabGroupCaptor.getValue().localId);
        verify(mTabGroupModelFilter, never()).getRelatedTabList(anyInt());
        verify(mTabGroupModelFilter, times(1)).getTabsInGroup(TOKEN_1);
    }

    @Test
    public void testWillMoveTabOutOfGroup() {
        // Add tab 1 and 2 to the tab model and create a group.
        mTabModel.addTab(
                mTab1, 0, TabLaunchType.FROM_TAB_GROUP_UI, TabCreationState.LIVE_IN_BACKGROUND);
        mTabModel.addTab(
                mTab2, 1, TabLaunchType.FROM_TAB_GROUP_UI, TabCreationState.LIVE_IN_BACKGROUND);
        when(mTabGroupModelFilter.getRepresentativeTabAt(0)).thenReturn(mTab1);

        // Move tab 2 out of group and verify.
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .willMoveTabOutOfGroup(mTab2, /* destinationTabGroupId= */ null);
        verify(mTabGroupSyncService, times(1)).removeTab(eq(LOCAL_TAB_GROUP_ID_1), eq(2));
    }

    @Test
    public void testDidCreateNewGroup() {
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didCreateNewGroup(mTab1, mTabGroupModelFilter);
        verify(mTabGroupSyncService, times(1)).addGroup(mSavedTabGroupCaptor.capture());
        Assert.assertEquals(LOCAL_TAB_GROUP_ID_1, mSavedTabGroupCaptor.getValue().localId);
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

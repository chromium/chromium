// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils.TabMovedCallback;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.Collection;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link TabGroupUiUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupUiUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private TabGroupUiActionHandler mUiActionHandler;
    @Mock private Tab mTab;
    @Mock private Tab mDestTab;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
    }

    private GroupWindowInfo createGroupWindowInfo(
            Token groupId, @GroupWindowState int windowState) {
        return createGroupWindowInfo(groupId, /* syncId= */ null, windowState);
    }

    private GroupWindowInfo createGroupWindowInfo(
            Token groupId, String syncId, @GroupWindowState int windowState) {
        return new GroupWindowInfo(
                groupId,
                syncId,
                "Title",
                TabGroupColorId.GREY,
                1,
                Collections.emptyList(),
                windowState,
                0L);
    }

    @Test
    public void testGetAddToGroupMenuItemString_withHasTabGroups() {
        Token tabGroupId = new Token(1L, 1L);
        assertEquals(
                R.string.menu_move_tab_to_group,
                TabGroupUiUtils.getAddToGroupMenuItemString(tabGroupId, /* hasTabGroups= */ true));
        assertEquals(
                R.string.menu_move_tab_to_group,
                TabGroupUiUtils.getAddToGroupMenuItemString(tabGroupId, /* hasTabGroups= */ false));

        assertEquals(
                R.string.menu_add_tab_to_group,
                TabGroupUiUtils.getAddToGroupMenuItemString(
                        /* currentTabGroupId= */ null, /* hasTabGroups= */ true));
        assertEquals(
                R.string.menu_add_tab_to_new_group,
                TabGroupUiUtils.getAddToGroupMenuItemString(
                        /* currentTabGroupId= */ null, /* hasTabGroups= */ false));
    }

    @Test
    public void testHasTabGroups_SingleWindow_HasGroups() {
        when(mTabModel.getTabGroupCount()).thenReturn(1);
        assertTrue(TabGroupUtils.hasTabGroups(mTabModel));
        assertTrue(TabGroupUtils.hasTabGroups(mTabModel, List.of(mTabModelSelector)));
    }

    @Test
    public void testHasTabGroups_SingleWindow_NoGroups() {
        when(mTabModel.getTabGroupCount()).thenReturn(0);
        assertFalse(TabGroupUtils.hasTabGroups(mTabModel));
        assertFalse(TabGroupUtils.hasTabGroups(mTabModel, List.of(mTabModelSelector)));
    }

    @Test
    public void testHasTabGroups_NullModel() {
        assertFalse(TabGroupUtils.hasTabGroups(/* tabModel= */ null));
        assertFalse(
                TabGroupUtils.hasTabGroups(
                        /* tabModel= */ null, (Collection<TabModelSelector>) null));
    }

    @Test
    public void testHasTabGroups_CrossWindow_HasGroupsInOtherWindow() {
        when(mTabModel.getTabGroupCount()).thenReturn(0);
        when(mTabModel.isIncognito()).thenReturn(false);

        TabModelSelector otherSelector = mock(TabModelSelector.class);
        TabModel otherModel = mock(TabModel.class);

        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(otherSelector.getModel(false)).thenReturn(otherModel);
        when(otherModel.getTabGroupCount()).thenReturn(2);

        List<TabModelSelector> selectors = List.of(mTabModelSelector, otherSelector);
        assertTrue(TabGroupUtils.hasTabGroups(mTabModel, selectors));
        assertFalse(TabGroupUtils.hasTabGroups(mTabModel, (Collection<TabModelSelector>) null));
    }

    @Test
    public void testAddTabsToGroup_emptyTabs() {
        TabModel model = mock(TabModel.class);
        TabGroupUiUtils.addTabsToGroup(
                model,
                List.of(),
                createGroupWindowInfo(Token.createRandom(), GroupWindowState.IN_CURRENT),
                /* syncService= */ null,
                /* uiActionHandler= */ null,
                /* tabMovedCallback= */ null,
                false);
        verify(model, never()).tabGroupExists(any());
    }

    @Test
    public void testAddTabsToGroup_alreadyInGroup() {
        Token groupId = Token.createRandom();
        Tab tab = mock(Tab.class);
        when(tab.getTabGroupId()).thenReturn(groupId);

        TabMovedCallback callback = mock(TabMovedCallback.class);
        TabGroupUiUtils.addTabsToGroup(
                mTabModel,
                List.of(tab),
                createGroupWindowInfo(groupId, GroupWindowState.IN_CURRENT),
                /* syncService= */ null,
                /* uiActionHandler= */ null,
                callback,
                false);

        verify(mTabModel, never()).tabGroupExists(any());
        verify(callback, never()).onTabMoved();
    }

    @Test
    public void testAddTabsToGroup_localMerge() {
        Token groupId = Token.createRandom();
        Tab tab = mock(Tab.class);
        when(tab.getTabGroupId()).thenReturn(null);

        Tab destTab = mock(Tab.class);
        when(destTab.getId()).thenReturn(100);

        when(mTabModel.tabGroupExists(groupId)).thenReturn(true);
        when(mTabModel.getGroupLastShownTabId(groupId)).thenReturn(100);
        when(mTabModel.getTabById(100)).thenReturn(destTab);

        TabMovedCallback callback = mock(TabMovedCallback.class);
        TabGroupUiUtils.addTabsToGroup(
                mTabModel,
                List.of(tab),
                createGroupWindowInfo(groupId, GroupWindowState.IN_CURRENT),
                /* syncService= */ null,
                /* uiActionHandler= */ null,
                callback,
                false);

        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        eq(List.of(tab)),
                        eq(destTab),
                        eq(TabGroupMergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP));
        verify(callback).onTabMoved();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testAddTabsToGroup_crossWindowMove() {
        MultiInstanceOrchestrator orchestrator = mock(MultiInstanceOrchestrator.class);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(orchestrator);

        Token groupId = Token.createRandom();
        Tab tab = mock(Tab.class);
        when(tab.getTabGroupId()).thenReturn(null);
        when(mTabModel.isTabInTabGroup(tab)).thenReturn(true);

        TabUngrouper ungrouper = mock(TabUngrouper.class);
        when(mTabModel.getTabUngrouper()).thenReturn(ungrouper);
        when(mTabModel.tabGroupExists(groupId)).thenReturn(false);
        when(mTabModel.isIncognito()).thenReturn(false);

        when(mTabWindowManager.findWindowIdForTabGroup(groupId)).thenReturn(2);
        TabModelSelector destSelector = mock(TabModelSelector.class);
        TabModel destTabModel = mock(TabModel.class);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(destSelector);
        when(destSelector.getModel(false)).thenReturn(destTabModel);
        when(destTabModel.getGroupLastShownTabId(groupId)).thenReturn(200);

        TabMovedCallback callback = mock(TabMovedCallback.class);
        TabGroupUiUtils.addTabsToGroup(
                mTabModel,
                List.of(tab),
                createGroupWindowInfo(groupId, GroupWindowState.IN_ANOTHER),
                /* syncService= */ null,
                /* uiActionHandler= */ null,
                callback,
                true);

        verify(ungrouper).ungroupTabs(eq(List.of(tab)), eq(true), eq(false));
        verify(orchestrator)
                .moveTabsToWindowByIdChecked(
                        eq(2), eq(List.of(tab)), eq(TabList.INVALID_TAB_INDEX), eq(200), eq(true));
        verify(callback).onTabMoved();
    }

    @Test
    public void testGetAddToGroupMenuItemTitle() {
        Context context = ApplicationProvider.getApplicationContext();
        assertEquals(
                "Add tab to group", TabGroupUiUtils.getAddToGroupMenuItemTitle(context, null, 1));
        assertEquals(
                "Move tab to group",
                TabGroupUiUtils.getAddToGroupMenuItemTitle(context, Token.createRandom(), 1));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testIsCrossWindowTabGroupOperationsEnabled() {
        assertTrue(TabGroupUiUtils.isCrossWindowTabGroupOperationsEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testIsCrossWindowTabGroupOperationsEnabled_disabled() {
        assertFalse(TabGroupUiUtils.isCrossWindowTabGroupOperationsEnabled());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testIsRemoteGroupOperationsEnabled() {
        assertTrue(TabGroupUiUtils.isRemoteGroupOperationsEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testIsRemoteGroupOperationsEnabled_defaultFalse() {
        assertFalse(TabGroupUiUtils.isRemoteGroupOperationsEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testIsRemoteGroupOperationsEnabled_disabled() {
        assertFalse(TabGroupUiUtils.isRemoteGroupOperationsEnabled());
    }

    @Test
    public void testIsValidDestination_nullGroup() {
        assertFalse(
                TabGroupUiUtils.isValidDestination(null, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    public void testIsValidDestination_nullLocalIdAndSyncId() {
        GroupWindowInfo group =
                createGroupWindowInfo(
                        /* groupId= */ null, /* syncId= */ null, GroupWindowState.IN_CURRENT);
        assertFalse(
                TabGroupUiUtils.isValidDestination(group, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    public void testIsValidDestination_inCurrentClosing() {
        GroupWindowInfo group =
                createGroupWindowInfo(Token.createRandom(), GroupWindowState.IN_CURRENT_CLOSING);
        assertFalse(
                TabGroupUiUtils.isValidDestination(group, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    public void testIsValidDestination_inCurrent() {
        GroupWindowInfo group =
                createGroupWindowInfo(Token.createRandom(), GroupWindowState.IN_CURRENT);
        assertTrue(
                TabGroupUiUtils.isValidDestination(group, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testIsValidDestination_hidden_remoteDisabled() {
        GroupWindowInfo group =
                createGroupWindowInfo(/* groupId= */ null, "sync_id", GroupWindowState.HIDDEN);
        assertFalse(
                TabGroupUiUtils.isValidDestination(group, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testIsValidDestination_hidden_remoteEnabled() {
        GroupWindowInfo groupWithoutSyncId =
                createGroupWindowInfo(
                        /* groupId= */ null, /* syncId= */ null, GroupWindowState.HIDDEN);
        assertFalse(
                TabGroupUiUtils.isValidDestination(
                        groupWithoutSyncId, mTabGroupSyncService, mUiActionHandler));

        GroupWindowInfo groupWithSyncId =
                createGroupWindowInfo(/* groupId= */ null, "sync_id", GroupWindowState.HIDDEN);
        assertFalse(
                TabGroupUiUtils.isValidDestination(groupWithSyncId, mTabGroupSyncService, null));
        assertFalse(TabGroupUiUtils.isValidDestination(groupWithSyncId, null, mUiActionHandler));
        assertTrue(
                TabGroupUiUtils.isValidDestination(
                        groupWithSyncId, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testIsValidDestination_inAnother_disabled() {
        GroupWindowInfo group =
                createGroupWindowInfo(Token.createRandom(), GroupWindowState.IN_ANOTHER);
        assertFalse(
                TabGroupUiUtils.isValidDestination(group, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testIsValidDestination_inAnother_enabled() {
        GroupWindowInfo groupWithoutLocalId =
                createGroupWindowInfo(/* groupId= */ null, "sync_id", GroupWindowState.IN_ANOTHER);
        assertFalse(
                TabGroupUiUtils.isValidDestination(
                        groupWithoutLocalId, mTabGroupSyncService, mUiActionHandler));

        GroupWindowInfo groupWithLocalId =
                createGroupWindowInfo(Token.createRandom(), GroupWindowState.IN_ANOTHER);
        assertTrue(
                TabGroupUiUtils.isValidDestination(
                        groupWithLocalId, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testIsValidDestination_nullLocalId_remoteDisabled() {
        GroupWindowInfo group =
                createGroupWindowInfo(/* groupId= */ null, "sync_id", GroupWindowState.IN_CURRENT);
        assertFalse(
                TabGroupUiUtils.isValidDestination(group, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testIsValidDestination_nullLocalId_inCurrent_remoteEnabled() {
        GroupWindowInfo group =
                createGroupWindowInfo(/* groupId= */ null, "sync_id", GroupWindowState.IN_CURRENT);
        assertTrue(
                TabGroupUiUtils.isValidDestination(group, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testIsValidDestination_remoteGroup_nullSyncService() {
        GroupWindowInfo group =
                createGroupWindowInfo(/* groupId= */ null, "sync_id", GroupWindowState.HIDDEN);
        assertFalse(TabGroupUiUtils.isValidDestination(group, null, mUiActionHandler));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testAddTabsToGroup_emptyTabs_doesNotOpenTabGroup() {
        String syncId = "sync-group-123";
        GroupWindowInfo hiddenGroup =
                createGroupWindowInfo(/* groupId= */ null, syncId, GroupWindowState.HIDDEN);
        TabMovedCallback callback = mock(TabMovedCallback.class);

        TabGroupUiUtils.addTabsToGroup(
                mTabModel,
                Collections.emptyList(),
                hiddenGroup,
                mTabGroupSyncService,
                mUiActionHandler,
                callback,
                false);

        verify(mUiActionHandler, never()).openTabGroup(any());
        verify(mTabModel, never()).mergeListOfTabsToGroup(any(), any(), anyInt());
        verify(callback, never()).onTabMoved();
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testAddTabsToGroup_nullLocalIdInCurrentRestoresGroup() {
        Token restoredGroupId = Token.createRandom();
        String syncId = "sync-group-456";
        GroupWindowInfo syntheticGroup =
                createGroupWindowInfo(/* groupId= */ null, syncId, GroupWindowState.IN_CURRENT);

        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.syncId = syncId;
        savedGroup.localId = new LocalTabGroupId(restoredGroupId);

        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(savedGroup);
        when(mTabModel.tabGroupExists(restoredGroupId)).thenReturn(true);
        when(mTabModel.getGroupLastShownTabId(restoredGroupId)).thenReturn(100);
        when(mTabModel.getTabById(100)).thenReturn(mDestTab);

        TabMovedCallback callback = mock(TabMovedCallback.class);
        TabGroupUiUtils.addTabsToGroup(
                mTabModel,
                List.of(mTab),
                syntheticGroup,
                mTabGroupSyncService,
                mUiActionHandler,
                callback,
                false);

        verify(mUiActionHandler).openTabGroup(syncId);
        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        eq(List.of(mTab)),
                        eq(mDestTab),
                        eq(TabGroupMergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP));
        verify(callback).onTabMoved();
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testAddTabsToGroup_hiddenRestoresGroup() {
        Token restoredGroupId = Token.createRandom();
        String syncId = "sync-group-123";
        GroupWindowInfo hiddenGroup =
                createGroupWindowInfo(/* groupId= */ null, syncId, GroupWindowState.HIDDEN);

        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.syncId = syncId;
        savedGroup.localId = new LocalTabGroupId(restoredGroupId);

        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(savedGroup);
        when(mTabModel.tabGroupExists(restoredGroupId)).thenReturn(true);
        when(mTabModel.getGroupLastShownTabId(restoredGroupId)).thenReturn(100);
        when(mTabModel.getTabById(100)).thenReturn(mDestTab);

        TabMovedCallback callback = mock(TabMovedCallback.class);
        TabGroupUiUtils.addTabsToGroup(
                mTabModel,
                List.of(mTab),
                hiddenGroup,
                mTabGroupSyncService,
                mUiActionHandler,
                callback,
                false);

        verify(mUiActionHandler).openTabGroup(syncId);
        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        eq(List.of(mTab)),
                        eq(mDestTab),
                        eq(TabGroupMergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP));
        verify(callback).onTabMoved();
    }

    @Test
    public void testAddTabsToGroup_invalidDestination() {
        TabMovedCallback callback = mock(TabMovedCallback.class);
        GroupWindowInfo closingGroup =
                createGroupWindowInfo(Token.createRandom(), GroupWindowState.IN_CURRENT_CLOSING);
        TabGroupUiUtils.addTabsToGroup(
                mTabModel,
                List.of(mTab),
                closingGroup,
                mTabGroupSyncService,
                mUiActionHandler,
                callback,
                false);
        verify(mTabModel, never()).mergeListOfTabsToGroup(any(), any(), anyInt());
        verify(callback, never()).onTabMoved();
    }

    @Test
    public void testGetGroupWindowInfo_localGroupId() {
        Token groupId = Token.createRandom();
        when(mTabModel.tabGroupExists(groupId)).thenReturn(true);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(mTab));

        GroupWindowInfo info =
                TabGroupUiUtils.getGroupWindowInfo(
                        mContext,
                        mTabModel,
                        mTabGroupSyncService,
                        groupId,
                        /* syncGroupId= */ null);

        assertNotNull(info);
        assertEquals(groupId, info.localId);
        assertNull(info.syncId);
        assertEquals(GroupWindowState.IN_CURRENT, info.groupWindowState);
    }

    @Test
    public void testGetGroupWindowInfo_syncGroupIdFound() {
        String syncId = "sync-group-123";
        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.syncId = syncId;
        savedGroup.title = "Synced Title";
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(savedGroup);

        GroupWindowInfo info =
                TabGroupUiUtils.getGroupWindowInfo(
                        mContext, mTabModel, mTabGroupSyncService, /* groupId= */ null, syncId);

        assertNotNull(info);
        assertEquals(syncId, info.syncId);
        assertNull(info.localId);
        assertEquals(GroupWindowState.HIDDEN, info.groupWindowState);
    }

    @Test
    public void testGetGroupWindowInfo_syncGroupIdNotFound() {
        String syncId = "sync-missing";
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(null);

        GroupWindowInfo info =
                TabGroupUiUtils.getGroupWindowInfo(
                        mContext, mTabModel, mTabGroupSyncService, /* groupId= */ null, syncId);

        assertNull(info);
    }

    @Test
    public void testGetGroupWindowInfo_bothIdsNull() {
        GroupWindowInfo info =
                TabGroupUiUtils.getGroupWindowInfo(
                        mContext,
                        mTabModel,
                        mTabGroupSyncService,
                        /* groupId= */ null,
                        /* syncGroupId= */ null);

        assertNull(info);
    }
}

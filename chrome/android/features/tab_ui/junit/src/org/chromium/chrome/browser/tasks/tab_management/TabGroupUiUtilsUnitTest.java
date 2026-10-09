// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Application;
import android.content.Context;
import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils.TabMovedCallback;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_group_sync.ClosingSource;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.url.JUnitTestGURLs;

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
    @Mock private Tab mClosingTab;
    @Mock private Tab mTabToAdd;
    @Mock private Tab mClosingTabInWindow2;
    @Mock private TabList mComprehensiveModel;
    @Mock private TabList mOtherComprehensiveModel;
    @Mock private TabModelSelector mOtherSelector;
    @Mock private TabModel mOtherModel;
    @Mock private TabModel mOtherIncognitoModel;
    @Mock private TabRemover mTabRemover;
    @Mock private TabRemover mOtherTabRemover;

    private Context mContext;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        when(mOtherSelector.getModel(true)).thenReturn(mOtherIncognitoModel);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mOtherModel.getTabRemover()).thenReturn(mOtherTabRemover);
        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mUserActionTester.tearDown();
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
                0L,
                /* collaborationId= */ null,
                /* isArchived= */ false);
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
        assertTrue(TabGroupUtils.hasTabGroups(mTabModel, /* selectorsForAllWindows= */ null));
        assertTrue(TabGroupUtils.hasTabGroups(mTabModel, List.of(mTabModelSelector)));
    }

    @Test
    public void testHasTabGroups_SingleWindow_NoGroups() {
        when(mTabModel.getTabGroupCount()).thenReturn(0);
        assertFalse(TabGroupUtils.hasTabGroups(mTabModel, /* selectorsForAllWindows= */ null));
        assertFalse(TabGroupUtils.hasTabGroups(mTabModel, List.of(mTabModelSelector)));
    }

    @Test
    public void testHasTabGroups_NullModel() {
        assertFalse(
                TabGroupUtils.hasTabGroups(
                        /* tabModel= */ null, /* selectorsForAllWindows= */ null));
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
        assertFalse(TabGroupUtils.hasTabGroups(mTabModel, /* selectorsForAllWindows= */ null));
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
        verify(model, never()).containsTabGroup(any());
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

        verify(mTabModel, never()).containsTabGroup(any());
        verify(callback, never()).onTabMoved();
    }

    @Test
    public void testAddTabsToGroup_localMerge() {
        Token groupId = Token.createRandom();
        Tab tab = mock(Tab.class);
        when(tab.getTabGroupId()).thenReturn(null);

        Tab destTab = mock(Tab.class);
        when(destTab.getId()).thenReturn(100);

        when(mTabModel.containsTabGroup(groupId)).thenReturn(true);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(destTab));
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
        when(mTabModel.containsTabGroup(groupId)).thenReturn(false);
        when(mTabModel.isIncognito()).thenReturn(false);

        when(mTabWindowManager.findWindowIdForTabGroup(groupId)).thenReturn(2);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean())).thenReturn(2);
        TabModelSelector destSelector = mock(TabModelSelector.class);
        TabModel destTabModel = mock(TabModel.class);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(destSelector);
        when(destSelector.getModel(false)).thenReturn(destTabModel);
        when(destTabModel.containsTabGroup(groupId)).thenReturn(true);
        when(mDestTab.getId()).thenReturn(200);
        when(destTabModel.getTabsInGroup(groupId)).thenReturn(List.of(mDestTab));

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

        Token groupId = Token.createRandom();
        GroupWindowInfo groupWithLocalId =
                createGroupWindowInfo(groupId, GroupWindowState.IN_ANOTHER);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId))).thenReturn(2);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);
        when(mOtherSelector.getModel(false)).thenReturn(mOtherModel);
        when(mOtherModel.containsTabGroup(groupId)).thenReturn(true);
        assertTrue(
                TabGroupUiUtils.isValidDestination(
                        groupWithLocalId, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testIsValidDestination_inAnother_active_returnsTrue() {
        Token groupId = Token.createRandom();
        GroupWindowInfo group =
                createGroupWindowInfo(groupId, "sync-123", GroupWindowState.IN_ANOTHER);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);
        when(mOtherSelector.getModel(false)).thenReturn(mOtherModel);
        when(mOtherModel.containsTabGroup(groupId)).thenReturn(true);

        assertTrue(
                TabGroupUiUtils.isValidDestination(group, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testIsValidDestination_inAnother_incognitoGroup_doesNotAssert() {
        Token groupId = Token.createRandom();
        GroupWindowInfo incognitoGroup =
                createGroupWindowInfo(groupId, /* syncId= */ null, GroupWindowState.IN_ANOTHER);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);

        TabModel incognitoModel = mock(TabModel.class);
        Tab incognitoTab = mock(Tab.class);
        when(incognitoModel.getTabsInGroup(groupId)).thenReturn(List.of(incognitoTab));
        when(mOtherSelector.getModel(/* incognito= */ true)).thenReturn(incognitoModel);

        assertTrue(
                TabGroupUiUtils.isValidDestination(
                        incognitoGroup, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testIsGroupClosingInAnotherWindow_incognitoGroup_asserts() {
        Token groupId = Token.createRandom();
        GroupWindowInfo group =
                createGroupWindowInfo(groupId, "sync-123", GroupWindowState.IN_ANOTHER);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);

        TabModel incognitoModel = mock(TabModel.class);
        Tab incognitoTab = mock(Tab.class);
        when(incognitoModel.getTabsInGroup(groupId)).thenReturn(List.of(incognitoTab));
        when(mOtherSelector.getModel(/* incognito= */ true)).thenReturn(incognitoModel);

        assertThrows(
                AssertionError.class,
                () ->
                        TabGroupUiUtils.isValidDestination(
                                group, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testIsValidDestination_inAnother_closing_remoteEnabled_returnsTrue() {
        Token groupId = Token.createRandom();
        GroupWindowInfo group =
                createGroupWindowInfo(groupId, "sync-123", GroupWindowState.IN_ANOTHER);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);
        when(mOtherSelector.getModel(false)).thenReturn(mOtherModel);
        when(mOtherModel.containsTabGroup(groupId)).thenReturn(false);

        assertTrue(
                TabGroupUiUtils.isValidDestination(group, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testIsValidDestination_inAnother_closing_remoteDisabled_returnsFalse() {
        Token groupId = Token.createRandom();
        GroupWindowInfo group =
                createGroupWindowInfo(groupId, "sync-123", GroupWindowState.IN_ANOTHER);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);
        when(mOtherSelector.getModel(false)).thenReturn(mOtherModel);
        when(mOtherModel.containsTabGroup(groupId)).thenReturn(false);

        assertFalse(
                TabGroupUiUtils.isValidDestination(group, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testIsValidDestination_hiddenGroup_remoteEnabled_returnsTrue() {
        Token groupId = Token.createRandom();
        GroupWindowInfo group = createGroupWindowInfo(groupId, "sync-123", GroupWindowState.HIDDEN);
        assertTrue(
                TabGroupUiUtils.isValidDestination(group, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testIsValidDestination_hiddenGroup_remoteDisabled_returnsFalse() {
        Token groupId = Token.createRandom();
        GroupWindowInfo group = createGroupWindowInfo(groupId, "sync-123", GroupWindowState.HIDDEN);
        assertFalse(
                TabGroupUiUtils.isValidDestination(group, mTabGroupSyncService, mUiActionHandler));
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
        when(mTabModel.containsTabGroup(restoredGroupId)).thenReturn(true);
        when(mDestTab.getId()).thenReturn(100);
        when(mTabModel.getTabsInGroup(restoredGroupId)).thenReturn(List.of(mDestTab));
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
        when(mTabModel.containsTabGroup(restoredGroupId)).thenReturn(true);
        when(mDestTab.getId()).thenReturn(100);
        when(mTabModel.getTabsInGroup(restoredGroupId)).thenReturn(List.of(mDestTab));
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
        when(mTabModel.containsTabGroup(groupId)).thenReturn(true);
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

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testGetGroupWindowInfo_retainsBothLocalAndSyncId() {
        Token groupId = Token.createRandom();
        String syncId = "sync-closing-123";
        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.syncId = syncId;
        savedGroup.localId = new LocalTabGroupId(groupId);
        savedGroup.title = "Remote Title";
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(savedGroup);

        when(mTab.getTabGroupId()).thenReturn(groupId);
        when(mTab.isClosing()).thenReturn(true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

        when(mComprehensiveModel.iterator()).thenAnswer(inv -> List.of(mTab).iterator());
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(Collections.emptyList());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveModel);
        when(mTabModel.getTabGroupTitle(groupId)).thenReturn("Live Closing Title");
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(TabGroupColorId.CYAN);

        GroupWindowInfo info =
                TabGroupUiUtils.getGroupWindowInfo(
                        mContext, mTabModel, mTabGroupSyncService, groupId, syncId);

        assertNotNull(info);
        assertEquals(groupId, info.localId);
        assertEquals(syncId, info.syncId);
        assertEquals("Live Closing Title", info.title);
        assertEquals(TabGroupColorId.CYAN, info.color);
        assertEquals(GroupWindowState.IN_CURRENT_CLOSING, info.groupWindowState);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testGetGroupWindowInfo_flagDisabled_returnsLocalGroupDirectly() {
        Token groupId = Token.createRandom();
        String syncId = "sync-closing-123";

        when(mTabModel.getTabCountForGroup(groupId)).thenReturn(2);
        when(mTabModel.containsTabGroup(groupId)).thenReturn(true);
        when(mTabModel.getTabGroupTitle(groupId)).thenReturn("Local Title");
        when(mTabModel.getTabGroupColorWithFallback(groupId)).thenReturn(TabGroupColorId.CYAN);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(Collections.emptyList());

        GroupWindowInfo info =
                TabGroupUiUtils.getGroupWindowInfo(
                        mContext, mTabModel, mTabGroupSyncService, groupId, syncId);

        assertNotNull(info);
        assertEquals(groupId, info.localId);
        assertNull(info.syncId);
        assertEquals("Local Title", info.title);
        verify(mTabGroupSyncService, never()).getGroup(syncId);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testGetLocalTabsInGroup_fallsBackToComprehensiveModel() {
        Token groupId = Token.createRandom();
        when(mTab.getTabGroupId()).thenReturn(groupId);
        when(mTab.isClosing()).thenReturn(true);

        when(mComprehensiveModel.iterator()).thenAnswer(inv -> List.of(mTab).iterator());
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(Collections.emptyList());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveModel);

        List<Tab> tabs = TabGroupUiUtils.getLocalTabsInGroup(mTabModel, groupId);
        assertEquals(1, tabs.size());
        assertEquals(mTab, tabs.get(0));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testGetLocalTabsInGroup_flagDisabled_doesNotFallBack() {
        Token groupId = Token.createRandom();
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(Collections.emptyList());

        List<Tab> tabs = TabGroupUiUtils.getLocalTabsInGroup(mTabModel, groupId);
        assertTrue(tabs.isEmpty());
        verify(mTabModel, never()).getComprehensiveModel();
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testGetLocalTabsInGroup_activeTabs_usesTabModelDirectly() {
        Token groupId = Token.createRandom();
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(mTab));

        List<Tab> tabs = TabGroupUiUtils.getLocalTabsInGroup(mTabModel, groupId);
        assertEquals(1, tabs.size());
        assertEquals(mTab, tabs.get(0));
        verify(mTabModel, never()).getComprehensiveModel();
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testGetTabModelForGroup_crossWindow() {
        Token groupId = Token.createRandom();
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(Collections.emptyList());
        when(mTabModel.isIncognito()).thenReturn(false);

        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), eq(true))).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);
        when(mOtherSelector.getModel(false)).thenReturn(mOtherModel);

        assertEquals(mOtherModel, TabGroupUiUtils.getTabModelForGroup(mTabModel, groupId));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testGetLocalOrCrossWindowTabsInGroup_crossWindow() {
        Token groupId = Token.createRandom();
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(Collections.emptyList());
        when(mTabModel.isIncognito()).thenReturn(false);

        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), eq(true))).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);
        when(mOtherSelector.getModel(false)).thenReturn(mOtherModel);
        when(mOtherModel.getTabsInGroup(groupId)).thenReturn(List.of(mTab));

        List<Tab> tabs = TabGroupUiUtils.getLocalOrCrossWindowTabsInGroup(mTabModel, groupId);
        assertEquals(1, tabs.size());
        assertEquals(mTab, tabs.get(0));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testGetGroupWindowInfo_resolvesCrossWindowLocalGroup() {
        Token groupId = Token.createRandom();
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(Collections.emptyList());
        when(mTabModel.isIncognito()).thenReturn(false);

        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);
        when(mOtherSelector.getModel(false)).thenReturn(mOtherModel);

        when(mOtherModel.containsTabGroup(groupId)).thenReturn(true);
        when(mOtherModel.getTabCountForGroup(groupId)).thenReturn(3);
        when(mOtherModel.getTabGroupTitle(groupId)).thenReturn("Window 2 Local Title");
        when(mOtherModel.getTabGroupColorWithFallback(groupId)).thenReturn(TabGroupColorId.PURPLE);
        when(mOtherModel.getTabsInGroup(groupId)).thenReturn(Collections.emptyList());

        GroupWindowInfo info =
                TabGroupUiUtils.getGroupWindowInfo(
                        mContext,
                        mTabModel,
                        /* syncService= */ null,
                        groupId,
                        /* syncGroupId= */ null);

        assertNotNull(info);
        assertEquals(groupId, info.localId);
        assertNull(info.syncId);
        assertEquals("Window 2 Local Title", info.title);
        assertEquals(TabGroupColorId.PURPLE, info.color);
        assertEquals(3, info.tabCount);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testCommitClosingTabsForGroup() {
        Token groupId = Token.createRandom();
        when(mTab.getId()).thenReturn(42);
        when(mTab.getTabGroupId()).thenReturn(groupId);
        when(mTab.isClosing()).thenReturn(true);

        when(mComprehensiveModel.iterator()).thenAnswer(inv -> List.of(mTab).iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveModel);

        TabGroupUiUtils.commitClosingTabsForGroup(mTabModel, groupId);
        verify(mTabModel).commitTabClosure(42);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testCommitClosingTabsForGroup_flagDisabled_doesNotCommit() {
        Token groupId = Token.createRandom();
        when(mTab.getId()).thenReturn(42);
        when(mTab.getTabGroupId()).thenReturn(groupId);
        when(mTab.isClosing()).thenReturn(true);

        TabGroupUiUtils.commitClosingTabsForGroup(mTabModel, groupId);
        verify(mTabModel, never()).commitTabClosure(anyInt());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testIsValidDestination_inCurrentClosingSyncedGroup() {
        Token groupId = Token.createRandom();
        GroupWindowInfo closingSynced =
                createGroupWindowInfo(groupId, "sync-123", GroupWindowState.IN_CURRENT_CLOSING);
        assertTrue(
                TabGroupUiUtils.isValidDestination(
                        closingSynced, mTabGroupSyncService, mUiActionHandler));

        GroupWindowInfo closingLocalOnly =
                createGroupWindowInfo(
                        groupId, /* syncId= */ null, GroupWindowState.IN_CURRENT_CLOSING);
        assertFalse(
                TabGroupUiUtils.isValidDestination(
                        closingLocalOnly, mTabGroupSyncService, mUiActionHandler));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testOpenTabGroup_withPendingClosure_commitsAndRemovesMappingBeforeOpening() {
        Token closingGroupId = Token.createRandom();
        String syncId = "sync-open-123";

        when(mClosingTab.getId()).thenReturn(111);
        when(mClosingTab.getTabGroupId()).thenReturn(closingGroupId);
        when(mClosingTab.isClosing()).thenReturn(true);

        when(mComprehensiveModel.iterator()).thenAnswer(inv -> List.of(mClosingTab).iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveModel);

        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.syncId = syncId;
        savedGroup.localId = new LocalTabGroupId(closingGroupId);
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(savedGroup);

        TabGroupUiUtils.openTabGroup(
                mContext, mTabModel, mTabGroupSyncService, mUiActionHandler, syncId);

        InOrder inOrder = inOrder(mTabModel, mTabGroupSyncService, mUiActionHandler);
        inOrder.verify(mTabModel).commitTabClosure(111);
        inOrder.verify(mTabGroupSyncService)
                .removeLocalTabGroupMapping(savedGroup.localId, ClosingSource.CLOSED_BY_USER);
        inOrder.verify(mUiActionHandler).openTabGroup(syncId);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testAddTabsToGroup_commitsClosingTabsBeforeOpeningRemoteGroup() {
        Token closingGroupId = Token.createRandom();
        Token reopenedGroupId = Token.createRandom();
        String syncId = "sync-closing-456";

        when(mClosingTab.getId()).thenReturn(99);
        when(mClosingTab.getTabGroupId()).thenReturn(closingGroupId);
        when(mClosingTab.isClosing()).thenReturn(true);

        when(mComprehensiveModel.iterator()).thenAnswer(inv -> List.of(mClosingTab).iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveModel);

        when(mTabToAdd.getTabGroupId()).thenReturn(null);

        SavedTabGroup reopenedSavedGroup = new SavedTabGroup();
        reopenedSavedGroup.syncId = syncId;
        reopenedSavedGroup.localId = new LocalTabGroupId(reopenedGroupId);
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(reopenedSavedGroup);
        when(mTabModel.containsTabGroup(reopenedGroupId)).thenReturn(false, true);

        GroupWindowInfo destInfo =
                createGroupWindowInfo(closingGroupId, syncId, GroupWindowState.IN_CURRENT_CLOSING);

        TabGroupUiUtils.addTabsToGroup(
                mTabModel,
                List.of(mTabToAdd),
                destInfo,
                mTabGroupSyncService,
                mUiActionHandler,
                /* tabMovedCallback= */ null,
                false);

        InOrder inOrder = inOrder(mTabModel, mTabGroupSyncService, mUiActionHandler);
        inOrder.verify(mTabModel).commitTabClosure(99);
        inOrder.verify(mTabGroupSyncService)
                .removeLocalTabGroupMapping(
                        reopenedSavedGroup.localId, ClosingSource.CLOSED_BY_USER);
        inOrder.verify(mUiActionHandler).openTabGroup(syncId);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testCommitClosingTabsForGroup_crossWindow() {
        Token groupId = Token.createRandom();

        when(mTabModel.isIncognito()).thenReturn(false);

        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);
        when(mOtherSelector.getModel(false)).thenReturn(mOtherModel);

        when(mClosingTabInWindow2.getId()).thenReturn(88);
        when(mClosingTabInWindow2.getTabGroupId()).thenReturn(groupId);
        when(mClosingTabInWindow2.isClosing()).thenReturn(true);
        when(mOtherComprehensiveModel.iterator())
                .thenAnswer(inv -> List.of(mClosingTabInWindow2).iterator());
        when(mOtherModel.getComprehensiveModel()).thenReturn(mOtherComprehensiveModel);

        TabGroupUiUtils.commitClosingTabsForGroup(mTabModel, groupId);

        verify(mOtherModel).commitTabClosure(88);
        verify(mTabModel, never()).commitTabClosure(anyInt());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testGetGroupWindowInfo_resolvesCrossWindowTabModelWhenClosing() {
        Token groupId = Token.createRandom();
        String syncId = "sync-closing-cross";
        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.syncId = syncId;
        savedGroup.localId = new LocalTabGroupId(groupId);
        savedGroup.title = "Stale Sync Title";
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(savedGroup);

        when(mTabModel.getTabsInGroup(groupId)).thenReturn(Collections.emptyList());
        when(mTabModel.isIncognito()).thenReturn(false);

        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);
        when(mOtherSelector.getModel(false)).thenReturn(mOtherModel);

        when(mClosingTabInWindow2.getTabGroupId()).thenReturn(groupId);
        when(mClosingTabInWindow2.isClosing()).thenReturn(true);
        when(mClosingTabInWindow2.getUrl()).thenReturn(JUnitTestGURLs.URL_2);
        when(mOtherComprehensiveModel.iterator())
                .thenAnswer(inv -> List.of(mClosingTabInWindow2).iterator());
        when(mOtherModel.getComprehensiveModel()).thenReturn(mOtherComprehensiveModel);
        when(mOtherModel.getTabsInGroup(groupId)).thenReturn(Collections.emptyList());
        when(mOtherModel.getTabGroupTitle(groupId)).thenReturn("Live Window 2 Closing Title");
        when(mOtherModel.getTabGroupColorWithFallback(groupId)).thenReturn(TabGroupColorId.RED);

        GroupWindowInfo info =
                TabGroupUiUtils.getGroupWindowInfo(
                        mContext, mTabModel, mTabGroupSyncService, groupId, syncId);

        assertNotNull(info);
        assertEquals(groupId, info.localId);
        assertEquals(syncId, info.syncId);
        assertEquals("Live Window 2 Closing Title", info.title);
        assertEquals(TabGroupColorId.RED, info.color);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testAddTabsToGroup_closingGroupInAnotherWindow_commitsClosureInOtherWindow() {
        Token closingGroupId = Token.createRandom();
        Token reopenedGroupId = Token.createRandom();
        String syncId = "sync-closing-cross-win";

        when(mTabModel.isIncognito()).thenReturn(false);

        when(mTabWindowManager.findWindowIdForTabGroup(eq(closingGroupId), anyBoolean()))
                .thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);
        when(mOtherSelector.getModel(false)).thenReturn(mOtherModel);

        when(mClosingTabInWindow2.getId()).thenReturn(88);
        when(mClosingTabInWindow2.getTabGroupId()).thenReturn(closingGroupId);
        when(mClosingTabInWindow2.isClosing()).thenReturn(true);

        when(mOtherComprehensiveModel.iterator())
                .thenAnswer(inv -> List.of(mClosingTabInWindow2).iterator());
        when(mOtherModel.getComprehensiveModel()).thenReturn(mOtherComprehensiveModel);

        when(mTabToAdd.getTabGroupId()).thenReturn(null);

        SavedTabGroup reopenedSavedGroup = new SavedTabGroup();
        reopenedSavedGroup.syncId = syncId;
        reopenedSavedGroup.localId = new LocalTabGroupId(reopenedGroupId);
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(reopenedSavedGroup);
        when(mTabModel.containsTabGroup(reopenedGroupId)).thenReturn(false, true);

        GroupWindowInfo destInfo =
                createGroupWindowInfo(closingGroupId, syncId, GroupWindowState.IN_CURRENT_CLOSING);

        TabGroupUiUtils.addTabsToGroup(
                mTabModel,
                List.of(mTabToAdd),
                destInfo,
                mTabGroupSyncService,
                mUiActionHandler,
                /* tabMovedCallback= */ null,
                false);

        InOrder inOrder = inOrder(mOtherModel, mTabGroupSyncService, mUiActionHandler);
        inOrder.verify(mOtherModel).commitTabClosure(88);
        inOrder.verify(mTabGroupSyncService)
                .removeLocalTabGroupMapping(
                        reopenedSavedGroup.localId, ClosingSource.CLOSED_BY_USER);
        inOrder.verify(mUiActionHandler).openTabGroup(syncId);
        verify(mTabModel, never()).commitTabClosure(anyInt());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testCommitClosingTabsForTab_flagDisabled_doesNotCommit() {
        Token groupId = Token.createRandom();
        when(mClosingTab.getId()).thenReturn(42);
        when(mClosingTab.getTabGroupId()).thenReturn(groupId);
        when(mClosingTab.isClosing()).thenReturn(true);

        TabGroupUiUtils.commitClosingTabsForTab(mTabModel, 42);
        verify(mTabModel, never()).commitTabClosure(anyInt());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testCommitClosingTabsForTab_tabClosingInGroup_commitsClosure() {
        Token groupId = Token.createRandom();
        when(mClosingTab.getId()).thenReturn(42);
        when(mClosingTab.getTabGroupId()).thenReturn(groupId);
        when(mClosingTab.isClosing()).thenReturn(true);

        when(mComprehensiveModel.iterator()).thenAnswer(inv -> List.of(mClosingTab).iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveModel);

        TabGroupUiUtils.commitClosingTabsForTab(mTabModel, 42);
        verify(mTabModel).commitTabClosure(42);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testCommitClosingTabsForTab_activeTab_doesNotCommit() {
        Token groupId = Token.createRandom();
        when(mTab.getId()).thenReturn(42);
        when(mTab.getTabGroupId()).thenReturn(groupId);
        when(mTab.isClosing()).thenReturn(false);

        when(mComprehensiveModel.iterator()).thenAnswer(inv -> List.of(mTab).iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveModel);

        TabGroupUiUtils.commitClosingTabsForTab(mTabModel, 42);
        verify(mTabModel, never()).commitTabClosure(anyInt());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true")
    public void testCommitClosingTabsForTab_invalidTabId_doesNotCommit() {
        TabGroupUiUtils.commitClosingTabsForTab(mTabModel, Tab.INVALID_TAB_ID);
        verify(mTabModel, never()).commitTabClosure(anyInt());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testAddTabsToGroup_targetingRemoteGroup_restoresGroup() {
        Token oldGroupId = Token.createRandom();
        Token reopenedGroupId = Token.createRandom();
        String syncId = "sync-closed-window-123";

        SavedTabGroup reopenedSavedGroup = new SavedTabGroup();
        reopenedSavedGroup.syncId = syncId;
        reopenedSavedGroup.localId = new LocalTabGroupId(reopenedGroupId);
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(reopenedSavedGroup);
        when(mTabModel.containsTabGroup(reopenedGroupId)).thenReturn(false, true);
        when(mDestTab.getId()).thenReturn(105);
        when(mTabModel.getTabsInGroup(reopenedGroupId)).thenReturn(List.of(mDestTab));
        when(mTabModel.getTabById(105)).thenReturn(mDestTab);

        when(mTabToAdd.getTabGroupId()).thenReturn(null);

        GroupWindowInfo destInfo =
                createGroupWindowInfo(oldGroupId, syncId, GroupWindowState.HIDDEN);

        TabMovedCallback callback = mock(TabMovedCallback.class);
        TabGroupUiUtils.addTabsToGroup(
                mTabModel,
                List.of(mTabToAdd),
                destInfo,
                mTabGroupSyncService,
                mUiActionHandler,
                callback,
                false);

        verify(mUiActionHandler).openTabGroup(syncId);
        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        eq(List.of(mTabToAdd)),
                        eq(mDestTab),
                        eq(TabGroupMergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP));
        verify(callback).onTabMoved();
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testAddTabsToGroup_targetingInAnotherClosing_commitsAndRestoresInCurrentWindow() {
        Token oldGroupId = Token.createRandom();
        Token reopenedGroupId = Token.createRandom();
        String syncId = "sync-closing-other-window-123";

        Tab closingTabInOtherWindow = mock(Tab.class);
        when(closingTabInOtherWindow.isClosing()).thenReturn(true);
        when(closingTabInOtherWindow.getTabGroupId()).thenReturn(oldGroupId);
        when(closingTabInOtherWindow.getId()).thenReturn(55);

        TabList otherTabList = mock(TabList.class);
        when(otherTabList.iterator())
                .thenAnswer(inv -> List.of(closingTabInOtherWindow).iterator());
        when(mOtherModel.getComprehensiveModel()).thenReturn(otherTabList);

        when(mTabWindowManager.findWindowIdForTabGroup(eq(oldGroupId), eq(true))).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);
        when(mOtherSelector.getModel(false)).thenReturn(mOtherModel);
        when(mOtherModel.containsTabGroup(oldGroupId)).thenReturn(false);

        SavedTabGroup reopenedSavedGroup = new SavedTabGroup();
        reopenedSavedGroup.syncId = syncId;
        reopenedSavedGroup.localId = new LocalTabGroupId(reopenedGroupId);
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(reopenedSavedGroup);
        when(mTabModel.containsTabGroup(reopenedGroupId)).thenReturn(false, true);
        when(mDestTab.getId()).thenReturn(105);
        when(mTabModel.getTabsInGroup(reopenedGroupId)).thenReturn(List.of(mDestTab));
        when(mTabModel.getTabById(105)).thenReturn(mDestTab);
        when(mTabModel.isIncognito()).thenReturn(false);

        TabList emptyTabList = mock(TabList.class);
        when(emptyTabList.iterator()).thenAnswer(inv -> Collections.emptyIterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(emptyTabList);

        when(mTabToAdd.getTabGroupId()).thenReturn(null);

        GroupWindowInfo destInfo =
                createGroupWindowInfo(oldGroupId, syncId, GroupWindowState.IN_ANOTHER);

        TabMovedCallback callback = mock(TabMovedCallback.class);
        TabGroupUiUtils.addTabsToGroup(
                mTabModel,
                List.of(mTabToAdd),
                destInfo,
                mTabGroupSyncService,
                mUiActionHandler,
                callback,
                false);

        verify(mOtherModel).commitTabClosure(55);
        verify(mUiActionHandler).openTabGroup(syncId);
        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        eq(List.of(mTabToAdd)),
                        eq(mDestTab),
                        eq(TabGroupMergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP));
        verify(callback).onTabMoved();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testAddTabsToGroup_hiddenGroup_remoteDisabled_doesNotRestoreOrMove() {
        Token groupId = Token.createRandom();
        String syncId = "sync-hidden-123";
        GroupWindowInfo destInfo = createGroupWindowInfo(groupId, syncId, GroupWindowState.HIDDEN);

        TabMovedCallback callback = mock(TabMovedCallback.class);
        TabGroupUiUtils.addTabsToGroup(
                mTabModel,
                List.of(mTabToAdd),
                destInfo,
                mTabGroupSyncService,
                mUiActionHandler,
                callback,
                false);

        verify(mUiActionHandler, never()).openTabGroup(any());
        verify(mTabModel, never()).mergeListOfTabsToGroup(any(), any(), anyInt());
        verify(callback, never()).onTabMoved();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testAddTabsToGroup_crossWindow_destTabIdInvalid_doesNotMove() {
        MultiInstanceOrchestrator orchestrator = mock(MultiInstanceOrchestrator.class);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(orchestrator);

        Token groupId = Token.createRandom();
        when(mTabModel.containsTabGroup(groupId)).thenReturn(false);
        when(mTabModel.isIncognito()).thenReturn(false);

        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId))).thenReturn(2);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);
        when(mOtherSelector.getModel(false)).thenReturn(mOtherModel);
        when(mOtherModel.containsTabGroup(groupId)).thenReturn(true);
        when(mOtherModel.getTabsInGroup(groupId)).thenReturn(Collections.emptyList());

        TabMovedCallback callback = mock(TabMovedCallback.class);
        TabGroupUiUtils.addTabsToGroup(
                mTabModel,
                List.of(mTabToAdd),
                createGroupWindowInfo(groupId, GroupWindowState.IN_ANOTHER),
                /* syncService= */ null,
                /* uiActionHandler= */ null,
                callback,
                true);

        verify(orchestrator, never())
                .moveTabsToWindowByIdChecked(anyInt(), any(), anyInt(), anyInt(), anyBoolean());
        verify(callback, never()).onTabMoved();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testAddTabsToGroup_crossWindow_invalidWindowId_doesNotMove() {
        MultiInstanceOrchestrator orchestrator = mock(MultiInstanceOrchestrator.class);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(orchestrator);

        Token groupId = Token.createRandom();
        when(mTabModel.containsTabGroup(groupId)).thenReturn(false);

        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId)))
                .thenReturn(TabWindowManager.INVALID_WINDOW_ID);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean()))
                .thenReturn(TabWindowManager.INVALID_WINDOW_ID);

        TabMovedCallback callback = mock(TabMovedCallback.class);
        TabGroupUiUtils.addTabsToGroup(
                mTabModel,
                List.of(mTabToAdd),
                createGroupWindowInfo(groupId, GroupWindowState.IN_ANOTHER),
                /* syncService= */ null,
                /* uiActionHandler= */ null,
                callback,
                true);

        verify(orchestrator, never())
                .moveTabsToWindowByIdChecked(anyInt(), any(), anyInt(), anyInt(), anyBoolean());
        verify(callback, never()).onTabMoved();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testOpenTabGroup_otherWindow_opensInOtherWindow() {
        MultiWindowTestUtils.enableMultiInstance();
        Token groupId = Token.createRandom();
        String syncId = "sync-other-123";

        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.syncId = syncId;
        savedGroup.localId = new LocalTabGroupId(groupId);
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(savedGroup);

        when(mTabModel.containsTabGroup(groupId)).thenReturn(false);
        when(mTabModel.isIncognito()).thenReturn(false);

        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId))).thenReturn(2);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);
        when(mOtherSelector.getModel(false)).thenReturn(mOtherModel);
        when(mOtherModel.containsTabGroup(groupId)).thenReturn(true);

        TabGroupUiUtils.openTabGroup(
                mContext, mTabModel, mTabGroupSyncService, mUiActionHandler, syncId);

        Intent intent =
                Shadows.shadowOf((Application) ApplicationProvider.getApplicationContext())
                        .getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(
                2,
                intent.getIntExtra(
                        IntentHandler.EXTRA_WINDOW_ID, TabWindowManager.INVALID_WINDOW_ID));
        assertEquals(syncId, IntentHandler.getBringTabGroupToFrontId(intent));
        verify(mUiActionHandler, never()).openTabGroup(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testOpenTabGroup_otherWindow_closedWindow_fallsBackToOpen() {
        Token groupId = Token.createRandom();
        String syncId = "sync-closed-456";

        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.syncId = syncId;
        savedGroup.localId = new LocalTabGroupId(groupId);
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(savedGroup);

        when(mTabModel.containsTabGroup(groupId)).thenReturn(false);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId)))
                .thenReturn(TabWindowManager.INVALID_WINDOW_ID);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean()))
                .thenReturn(TabWindowManager.INVALID_WINDOW_ID);

        TabGroupUiUtils.openTabGroup(
                mContext, mTabModel, mTabGroupSyncService, mUiActionHandler, syncId);

        verify(mTabGroupSyncService)
                .removeLocalTabGroupMapping(savedGroup.localId, ClosingSource.CLOSED_BY_USER);
        verify(mUiActionHandler).openTabGroup(syncId);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testOpenTabGroup_otherWindow_nullContext_fallsBackToOpen() {
        MultiWindowTestUtils.enableMultiInstance();
        Token groupId = Token.createRandom();
        String syncId = "sync-null-context";

        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.syncId = syncId;
        savedGroup.localId = new LocalTabGroupId(groupId);
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(savedGroup);

        when(mTabModel.containsTabGroup(groupId)).thenReturn(false);
        when(mTabModel.isIncognito()).thenReturn(false);

        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId))).thenReturn(2);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);
        when(mOtherSelector.getModel(false)).thenReturn(mOtherModel);
        when(mOtherModel.containsTabGroup(groupId)).thenReturn(true);

        TabGroupUiUtils.openTabGroup(
                /* context= */ null, mTabModel, mTabGroupSyncService, mUiActionHandler, syncId);

        verify(mUiActionHandler).openTabGroup(syncId);
        verify(mTabGroupSyncService, never()).removeLocalTabGroupMapping(any(), anyInt());
        assertNull(
                Shadows.shadowOf((Application) ApplicationProvider.getApplicationContext())
                        .getNextStartedActivity());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testOpenTabGroup_flagDisabled_fallsBackToOpen() {
        Token groupId = Token.createRandom();
        String syncId = "sync-disabled-789";

        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.syncId = syncId;
        savedGroup.localId = new LocalTabGroupId(groupId);
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(savedGroup);

        when(mTabModel.containsTabGroup(groupId)).thenReturn(false);

        TabGroupUiUtils.openTabGroup(
                mContext, mTabModel, mTabGroupSyncService, mUiActionHandler, syncId);

        verify(mTabGroupSyncService)
                .removeLocalTabGroupMapping(savedGroup.localId, ClosingSource.CLOSED_BY_USER);
        verify(mUiActionHandler).openTabGroup(syncId);
    }

    @Test
    public void testDeleteTabGroup_inCurrent() {
        Token groupId = Token.createRandom();
        GroupWindowInfo groupInfo =
                createGroupWindowInfo(groupId, "sync-curr", GroupWindowState.IN_CURRENT);

        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(mTab));

        TabGroupUiUtils.deleteTabGroup(
                mTabModel,
                mTabGroupSyncService,
                groupInfo,
                GroupWindowState.IN_CURRENT,
                /* allowDialog= */ true);

        verify(mTabRemover).closeTabs(any(), eq(true));
        verify(mTabGroupSyncService, never()).removeGroup(any(String.class));
        verify(mTabGroupSyncService, never()).removeGroup(any(LocalTabGroupId.class));
        assertEquals(1, mUserActionTester.getActionCount("SyncedTabGroup.DeleteWithLocal"));
    }

    @Test
    public void testDeleteTabGroup_inCurrent_noTabsInGroup() {
        Token groupId = Token.createRandom();
        GroupWindowInfo groupInfo =
                createGroupWindowInfo(groupId, "sync-curr-empty", GroupWindowState.IN_CURRENT);

        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of());

        TabGroupUiUtils.deleteTabGroup(
                mTabModel,
                mTabGroupSyncService,
                groupInfo,
                GroupWindowState.IN_CURRENT,
                /* allowDialog= */ true);

        verify(mTabRemover, never()).closeTabs(any(), anyBoolean());
        verify(mTabGroupSyncService).removeGroup("sync-curr-empty");
        assertEquals(1, mUserActionTester.getActionCount("SyncedTabGroup.DeleteWithLocal"));
    }

    @Test
    public void testDeleteTabGroup_inCurrent_tabsAlreadyClosing_commitsAndRemovesSync() {
        Token groupId = Token.createRandom();
        String syncId = "sync-curr-closing-tabs";
        GroupWindowInfo groupInfo =
                createGroupWindowInfo(groupId, syncId, GroupWindowState.IN_CURRENT);

        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of());
        when(mClosingTab.getId()).thenReturn(42);
        when(mClosingTab.getTabGroupId()).thenReturn(groupId);
        when(mClosingTab.isClosing()).thenReturn(true);
        when(mComprehensiveModel.iterator()).thenAnswer(inv -> List.of(mClosingTab).iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveModel);

        TabGroupUiUtils.deleteTabGroup(
                mTabModel,
                mTabGroupSyncService,
                groupInfo,
                GroupWindowState.IN_CURRENT,
                /* allowDialog= */ true);

        verify(mTabRemover, never()).closeTabs(any(), anyBoolean());
        verify(mTabModel).commitTabClosure(42);
        verify(mTabGroupSyncService).removeGroup(syncId);
        assertEquals(1, mUserActionTester.getActionCount("SyncedTabGroup.DeleteWithLocal"));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testDeleteTabGroup_inCurrentClosing() {
        Token groupId = Token.createRandom();
        String syncId = "sync-curr-closing";
        GroupWindowInfo groupInfo =
                createGroupWindowInfo(groupId, syncId, GroupWindowState.IN_CURRENT_CLOSING);

        when(mClosingTab.getId()).thenReturn(42);
        when(mClosingTab.getTabGroupId()).thenReturn(groupId);
        when(mClosingTab.isClosing()).thenReturn(true);
        when(mComprehensiveModel.iterator()).thenAnswer(inv -> List.of(mClosingTab).iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveModel);

        TabGroupUiUtils.deleteTabGroup(
                mTabModel,
                mTabGroupSyncService,
                groupInfo,
                GroupWindowState.IN_CURRENT_CLOSING,
                /* allowDialog= */ false);

        verify(mTabModel).commitTabClosure(42);
        verify(mTabGroupSyncService).removeGroup(syncId);
        assertEquals(1, mUserActionTester.getActionCount("SyncedTabGroup.DeleteWithLocal"));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testDeleteTabGroup_inAnother_activeWindow() {
        Token groupId = Token.createRandom();
        String syncId = "sync-other-123";
        GroupWindowInfo groupInfo =
                createGroupWindowInfo(groupId, syncId, GroupWindowState.IN_ANOTHER);

        when(mTabModel.isIncognito()).thenReturn(false);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);
        when(mOtherSelector.getModel(false)).thenReturn(mOtherModel);
        when(mOtherModel.getTabsInGroup(groupId)).thenReturn(List.of(mTab));

        TabGroupUiUtils.deleteTabGroup(
                mTabModel,
                mTabGroupSyncService,
                groupInfo,
                GroupWindowState.IN_ANOTHER,
                /* allowDialog= */ false);

        verify(mOtherTabRemover).closeTabs(any(), eq(false));
        verify(mTabGroupSyncService, never()).removeGroup(any(String.class));
        verify(mTabGroupSyncService, never()).removeGroup(any(LocalTabGroupId.class));
        assertEquals(1, mUserActionTester.getActionCount("SyncedTabGroup.DeleteWithLocal"));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testDeleteTabGroup_inAnother_windowNotFound() {
        Token groupId = Token.createRandom();
        String syncId = "sync-closed-456";
        GroupWindowInfo groupInfo =
                createGroupWindowInfo(groupId, syncId, GroupWindowState.IN_ANOTHER);

        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean()))
                .thenReturn(TabWindowManager.INVALID_WINDOW_ID);

        TabGroupUiUtils.deleteTabGroup(
                mTabModel,
                mTabGroupSyncService,
                groupInfo,
                GroupWindowState.IN_ANOTHER,
                /* allowDialog= */ false);

        verify(mTabGroupSyncService).removeGroup(syncId);
        verify(mOtherTabRemover, never()).closeTabs(any(), anyBoolean());
        assertEquals(1, mUserActionTester.getActionCount("SyncedTabGroup.DeleteWithLocal"));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testDeleteTabGroup_inAnother_tabsAlreadyClosing_commitsAndRemovesSync() {
        Token groupId = Token.createRandom();
        String syncId = "sync-other-closing-tabs";
        GroupWindowInfo groupInfo =
                createGroupWindowInfo(groupId, syncId, GroupWindowState.IN_ANOTHER);

        when(mTabModel.isIncognito()).thenReturn(false);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);
        when(mOtherSelector.getModel(false)).thenReturn(mOtherModel);
        when(mOtherModel.getTabsInGroup(groupId)).thenReturn(List.of());

        when(mClosingTabInWindow2.getId()).thenReturn(88);
        when(mClosingTabInWindow2.getTabGroupId()).thenReturn(groupId);
        when(mClosingTabInWindow2.isClosing()).thenReturn(true);
        when(mOtherComprehensiveModel.iterator())
                .thenAnswer(inv -> List.of(mClosingTabInWindow2).iterator());
        when(mOtherModel.getComprehensiveModel()).thenReturn(mOtherComprehensiveModel);

        TabGroupUiUtils.deleteTabGroup(
                mTabModel,
                mTabGroupSyncService,
                groupInfo,
                GroupWindowState.IN_ANOTHER,
                /* allowDialog= */ false);

        verify(mOtherTabRemover, never()).closeTabs(any(), anyBoolean());
        verify(mOtherModel).commitTabClosure(88);
        verify(mTabGroupSyncService).removeGroup(syncId);
        assertEquals(1, mUserActionTester.getActionCount("SyncedTabGroup.DeleteWithLocal"));
    }

    @Test
    public void testDeleteTabGroup_inAnother_groupInCurrentModel_closesTabs() {
        Token groupId = Token.createRandom();
        String syncId = "sync-moved-to-curr";
        GroupWindowInfo groupInfo =
                createGroupWindowInfo(groupId, syncId, GroupWindowState.IN_ANOTHER);

        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(mTab));

        TabGroupUiUtils.deleteTabGroup(
                mTabModel,
                mTabGroupSyncService,
                groupInfo,
                GroupWindowState.IN_ANOTHER,
                /* allowDialog= */ false);

        verify(mTabRemover).closeTabs(any(), eq(false));
        verify(mTabGroupSyncService, never()).removeGroup(any(String.class));
        assertEquals(1, mUserActionTester.getActionCount("SyncedTabGroup.DeleteWithLocal"));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testDeleteTabGroup_hidden() {
        Token groupId = Token.createRandom();
        String syncId = "sync-hidden-789";
        GroupWindowInfo groupInfo = createGroupWindowInfo(groupId, syncId, GroupWindowState.HIDDEN);

        when(mClosingTab.getId()).thenReturn(42);
        when(mClosingTab.getTabGroupId()).thenReturn(groupId);
        when(mClosingTab.isClosing()).thenReturn(true);
        when(mComprehensiveModel.iterator()).thenAnswer(inv -> List.of(mClosingTab).iterator());
        when(mTabModel.getComprehensiveModel()).thenReturn(mComprehensiveModel);

        TabGroupUiUtils.deleteTabGroup(
                mTabModel,
                mTabGroupSyncService,
                groupInfo,
                GroupWindowState.HIDDEN,
                /* allowDialog= */ false);

        verify(mTabModel).commitTabClosure(42);
        verify(mTabGroupSyncService).removeGroup(syncId);
        assertEquals(1, mUserActionTester.getActionCount("SyncedTabGroup.DeleteWithLocal"));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testDeleteTabGroup_hidden_withoutLocal() {
        String syncId = "sync-hidden-nolocal";
        GroupWindowInfo groupInfo =
                createGroupWindowInfo(/* groupId= */ null, syncId, GroupWindowState.HIDDEN);

        TabGroupUiUtils.deleteTabGroup(
                mTabModel,
                mTabGroupSyncService,
                groupInfo,
                GroupWindowState.HIDDEN,
                /* allowDialog= */ false);

        verify(mTabGroupSyncService).removeGroup(syncId);
        verify(mTabModel, never()).commitTabClosure(anyInt());
        assertEquals(1, mUserActionTester.getActionCount("SyncedTabGroup.DeleteWithoutLocal"));
    }

    @Test
    public void testDeleteTabGroup_nullSyncService() {
        Token groupId = Token.createRandom();
        GroupWindowInfo groupInfo =
                createGroupWindowInfo(groupId, "sync-null", GroupWindowState.IN_CURRENT);

        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(mTab));

        TabGroupUiUtils.deleteTabGroup(
                mTabModel,
                /* syncService= */ null,
                groupInfo,
                GroupWindowState.IN_CURRENT,
                /* allowDialog= */ true);

        verify(mTabRemover).closeTabs(any(), eq(true));
        assertEquals(1, mUserActionTester.getActionCount("SyncedTabGroup.DeleteWithLocal"));
    }

    @Test
    public void testDeleteTabGroup_nullSyncService_hidden() {
        GroupWindowInfo groupInfo =
                createGroupWindowInfo(/* groupId= */ null, "sync-null", GroupWindowState.HIDDEN);

        TabGroupUiUtils.deleteTabGroup(
                mTabModel,
                /* syncService= */ null,
                groupInfo,
                GroupWindowState.HIDDEN,
                /* allowDialog= */ false);

        assertEquals(1, mUserActionTester.getActionCount("SyncedTabGroup.DeleteWithoutLocal"));
    }

    @Test
    public void testDeleteTabGroup_dynamicLocalIdFallback_inCurrent() {
        Token groupId = Token.createRandom();
        String syncId = "sync-dynamic-123";
        GroupWindowInfo groupInfo =
                createGroupWindowInfo(/* groupId= */ null, syncId, GroupWindowState.IN_CURRENT);

        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.syncId = syncId;
        savedGroup.localId = new LocalTabGroupId(groupId);
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(savedGroup);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(mTab));

        TabGroupUiUtils.deleteTabGroup(
                mTabModel,
                mTabGroupSyncService,
                groupInfo,
                GroupWindowState.IN_CURRENT,
                /* allowDialog= */ true);

        verify(mTabRemover).closeTabs(any(), eq(true));
        verify(mTabGroupSyncService, never()).removeGroup(any(String.class));
        assertEquals(1, mUserActionTester.getActionCount("SyncedTabGroup.DeleteWithLocal"));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS + ":remote_group_operations/true"
    })
    public void testDeleteTabGroup_dynamicLocalIdFallback_inAnother() {
        Token groupId = Token.createRandom();
        String syncId = "sync-dynamic-other";
        GroupWindowInfo groupInfo =
                createGroupWindowInfo(/* groupId= */ null, syncId, GroupWindowState.IN_ANOTHER);

        SavedTabGroup savedGroup = new SavedTabGroup();
        savedGroup.syncId = syncId;
        savedGroup.localId = new LocalTabGroupId(groupId);
        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(savedGroup);

        when(mTabModel.isIncognito()).thenReturn(false);
        when(mTabWindowManager.findWindowIdForTabGroup(eq(groupId), anyBoolean())).thenReturn(2);
        when(mTabWindowManager.getTabModelSelectorById(2)).thenReturn(mOtherSelector);
        when(mOtherSelector.getModel(false)).thenReturn(mOtherModel);
        when(mOtherModel.getTabsInGroup(groupId)).thenReturn(List.of(mTab));

        TabGroupUiUtils.deleteTabGroup(
                mTabModel,
                mTabGroupSyncService,
                groupInfo,
                GroupWindowState.IN_ANOTHER,
                /* allowDialog= */ false);

        verify(mOtherTabRemover).closeTabs(any(), eq(false));
        verify(mTabGroupSyncService, never()).removeGroup(any(String.class));
        assertEquals(1, mUserActionTester.getActionCount("SyncedTabGroup.DeleteWithLocal"));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testDeleteTabGroup_inAnother_flagDisabled_returnsEarly() {
        String syncId = "sync_id";
        Token groupId = Token.createRandom();
        GroupWindowInfo groupInfo =
                createGroupWindowInfo(groupId, syncId, GroupWindowState.IN_ANOTHER);

        TabGroupUiUtils.deleteTabGroup(
                mTabModel,
                mTabGroupSyncService,
                groupInfo,
                GroupWindowState.IN_ANOTHER,
                /* allowDialog= */ false);

        verify(mTabGroupSyncService, never()).removeGroup(any(String.class));
        verify(mTabRemover, never()).closeTabs(any(), anyBoolean());
    }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabGroupUtils.GroupsPendingDestroy;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.chrome.browser.tabmodel.TabModelRemover.TabModelRemoverFlowHandler;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.List;

/** Unit tests for {@link TabRemoverImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabRemoverImplUnitTest {
    private static final String SYNC_ID = "sync_id";
    private static final String COLLABORATION_ID = "collaboration_id";
    private static final LocalTabGroupId TAB_GROUP_ID = new LocalTabGroupId(new Token(1L, 2L));
    private static final String TITLE = "My title";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ActionConfirmationManager mActionConfirmationManager;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModelRemover mTabModelRemover;
    @Mock private Runnable mUndoRunnable;
    @Mock private TabRemover mMockTabRemover;
    @Mock private TabModelActionListener mListener;
    @Mock private Callback<Integer> mOnResult;
    @Mock private DataSharingService mDataSharingService;
    @Mock private TabGroupSyncService mTabGroupSyncService;

    @Captor private ArgumentCaptor<TabModelRemoverFlowHandler> mHandlerCaptor;
    @Captor private ArgumentCaptor<Callback<Integer>> mOnResultCaptor;

    private MockTabModel mTabModel;
    private TabRemoverImpl mTabRemoverImpl;

    @Before
    public void setUp() {
        DataSharingServiceFactory.setForTesting(mDataSharingService);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});

        when(mProfile.isOffTheRecord()).thenReturn(false);
        mTabModel = spy(new MockTabModel(mProfile, null));
        mTabModel.setTabRemoverForTesting(mMockTabRemover);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModelRemover.getTabGroupModelFilter()).thenReturn(mTabGroupModelFilter);
        when(mTabModelRemover.getActionConfirmationManager())
                .thenReturn(mActionConfirmationManager);
        mTabRemoverImpl = new TabRemoverImpl(mTabModelRemover);
    }

    @Test
    public void testForceCloseTabs() {
        mTabModel.addTab(/* id= */ 0);
        TabClosureParams params = TabClosureParams.closeAllTabs().build();
        mTabRemoverImpl.forceCloseTabs(params);
        verify(mTabGroupModelFilter).closeTabs(params);
    }

    @Test
    public void testCloseTabsHandler_NoDialog() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        tab0.setTabGroupId(TAB_GROUP_ID.tabGroupId);
        TabClosureParams params = TabClosureParams.closeAllTabs().build();

        mTabRemoverImpl.closeTabs(params, /* allowDialog= */ true, mListener);
        verify(mTabModelRemover).doTabRemovalFlow(mHandlerCaptor.capture(), eq(true));
        TabModelRemoverFlowHandler handler = mHandlerCaptor.getValue();

        GroupsPendingDestroy groupsPendingDestroy = handler.computeGroupsPendingDestroy();
        assertTrue(groupsPendingDestroy.isEmpty());

        // No placeholder created.

        handler.performAction();
        verify(mTabGroupModelFilter).closeTabs(eq(params));
        verify(mListener)
                .onConfirmationDialogResult(
                        DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verifyNoMoreInteractions(mListener);
    }

    @Test
    public void testCloseTabsHandler_Group_Deletion() {
        int id = 0;
        Tab tab0 = mTabModel.addTab(id);
        tab0.setTabGroupId(TAB_GROUP_ID.tabGroupId);
        tab0.setRootId(id);
        when(mTabGroupModelFilter.getRelatedTabListForRootId(id)).thenReturn(List.of(tab0));
        TabClosureParams params =
                TabClosureParams.forCloseTabGroup(mTabGroupModelFilter, id).build();

        mTabRemoverImpl.closeTabs(params, /* allowDialog= */ true, mListener);
        verify(mTabModelRemover).doTabRemovalFlow(mHandlerCaptor.capture(), eq(true));
        TabModelRemoverFlowHandler handler = mHandlerCaptor.getValue();

        SavedTabGroupTab savedTab = new SavedTabGroupTab();
        savedTab.localId = id;
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.localId = TAB_GROUP_ID;
        savedTabGroup.savedTabs.add(savedTab);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_ID});
        when(mTabGroupSyncService.getGroup(SYNC_ID)).thenReturn(savedTabGroup);

        GroupsPendingDestroy groupsPendingDestroy = handler.computeGroupsPendingDestroy();
        assertFalse(groupsPendingDestroy.isEmpty());
        assertTrue(groupsPendingDestroy.collaborationGroupsDestroyed.isEmpty());
        assertFalse(groupsPendingDestroy.syncedGroupsDestroyed.isEmpty());

        // No placeholder tabs created.

        handler.showTabGroupDeletionConfirmationDialog(mOnResult);
        verify(mActionConfirmationManager).processDeleteGroupAttempt(mOnResultCaptor.capture());
        mOnResultCaptor.getValue().onResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verify(mListener)
                .onConfirmationDialogResult(
                        DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verify(mOnResult).onResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);

        handler.performAction();
        verify(mTabGroupModelFilter).closeTabs(any(TabClosureParams.class));

        verifyNoMoreInteractions(mListener);
    }

    @Test
    public void testCloseTabsHandler_Deletion() {
        int id = 0;
        Tab tab0 = mTabModel.addTab(id);
        tab0.setTabGroupId(TAB_GROUP_ID.tabGroupId);
        TabClosureParams params = TabClosureParams.closeAllTabs().build();

        mTabRemoverImpl.closeTabs(params, /* allowDialog= */ true, mListener);
        verify(mTabModelRemover).doTabRemovalFlow(mHandlerCaptor.capture(), eq(true));
        TabModelRemoverFlowHandler handler = mHandlerCaptor.getValue();

        SavedTabGroupTab savedTab = new SavedTabGroupTab();
        savedTab.localId = id;
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.localId = TAB_GROUP_ID;
        savedTabGroup.savedTabs.add(savedTab);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_ID});
        when(mTabGroupSyncService.getGroup(SYNC_ID)).thenReturn(savedTabGroup);

        GroupsPendingDestroy groupsPendingDestroy = handler.computeGroupsPendingDestroy();
        assertFalse(groupsPendingDestroy.isEmpty());
        assertTrue(groupsPendingDestroy.collaborationGroupsDestroyed.isEmpty());
        assertFalse(groupsPendingDestroy.syncedGroupsDestroyed.isEmpty());

        // No placeholder tabs created.

        handler.showTabGroupDeletionConfirmationDialog(mOnResult);
        verify(mActionConfirmationManager).processCloseTabAttempt(mOnResultCaptor.capture());
        mOnResultCaptor.getValue().onResult(ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mListener)
                .onConfirmationDialogResult(
                        DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mOnResult).onResult(ActionConfirmationResult.CONFIRMATION_POSITIVE);

        handler.performAction();
        verify(mTabGroupModelFilter).closeTabs(any(TabClosureParams.class));

        verifyNoMoreInteractions(mListener);
    }

    @Test
    public void testCloseTabsHandler_Owner() {
        int id = 0;
        Tab tab0 = mTabModel.addTab(id);
        tab0.setTabGroupId(TAB_GROUP_ID.tabGroupId);
        TabClosureParams params = TabClosureParams.closeAllTabs().build();

        mTabRemoverImpl.closeTabs(params, /* allowDialog= */ true, mListener);
        verify(mTabModelRemover).doTabRemovalFlow(mHandlerCaptor.capture(), eq(true));
        TabModelRemoverFlowHandler handler = mHandlerCaptor.getValue();

        SavedTabGroupTab savedTab = new SavedTabGroupTab();
        savedTab.localId = id;
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.localId = TAB_GROUP_ID;
        savedTabGroup.collaborationId = COLLABORATION_ID;
        savedTabGroup.savedTabs.add(savedTab);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_ID});
        when(mTabGroupSyncService.getGroup(SYNC_ID)).thenReturn(savedTabGroup);

        GroupsPendingDestroy groupsPendingDestroy = handler.computeGroupsPendingDestroy();
        assertFalse(groupsPendingDestroy.isEmpty());
        assertFalse(groupsPendingDestroy.collaborationGroupsDestroyed.isEmpty());
        assertTrue(groupsPendingDestroy.syncedGroupsDestroyed.isEmpty());

        Tab placeholderTab = mTabModel.addTab(/* id= */ 1);
        handler.onPlaceholderTabsCreated(List.of(placeholderTab));

        handler.showCollaborationKeepDialog(MemberRole.OWNER, TITLE, mOnResult);
        verify(mActionConfirmationManager)
                .processCollaborationOwnerRemoveLastTab(eq(TITLE), mOnResultCaptor.capture());
        mOnResultCaptor.getValue().onResult(ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mListener)
                .onConfirmationDialogResult(
                        DialogType.COLLABORATION, ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mOnResult).onResult(ActionConfirmationResult.CONFIRMATION_POSITIVE);

        handler.performAction();
        verify(mTabGroupModelFilter).closeTabs(any(TabClosureParams.class));

        verifyNoMoreInteractions(mListener);
    }

    @Test
    public void testCloseTabsHandler_Member() {
        int id = 0;
        Tab tab0 = mTabModel.addTab(id);
        tab0.setTabGroupId(TAB_GROUP_ID.tabGroupId);
        TabClosureParams params = TabClosureParams.closeAllTabs().build();

        mTabRemoverImpl.closeTabs(params, /* allowDialog= */ true, mListener);
        verify(mTabModelRemover).doTabRemovalFlow(mHandlerCaptor.capture(), eq(true));
        TabModelRemoverFlowHandler handler = mHandlerCaptor.getValue();

        SavedTabGroupTab savedTab = new SavedTabGroupTab();
        savedTab.localId = id;
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.localId = TAB_GROUP_ID;
        savedTabGroup.savedTabs.add(savedTab);
        savedTabGroup.collaborationId = COLLABORATION_ID;
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_ID});
        when(mTabGroupSyncService.getGroup(SYNC_ID)).thenReturn(savedTabGroup);

        GroupsPendingDestroy groupsPendingDestroy = handler.computeGroupsPendingDestroy();
        assertFalse(groupsPendingDestroy.isEmpty());
        assertFalse(groupsPendingDestroy.collaborationGroupsDestroyed.isEmpty());
        assertTrue(groupsPendingDestroy.syncedGroupsDestroyed.isEmpty());

        Tab placeholderTab = mTabModel.addTab(/* id= */ 1);
        handler.onPlaceholderTabsCreated(List.of(placeholderTab));

        handler.showCollaborationKeepDialog(MemberRole.MEMBER, TITLE, mOnResult);
        verify(mActionConfirmationManager)
                .processCollaborationMemberRemoveLastTab(eq(TITLE), mOnResultCaptor.capture());
        mOnResultCaptor.getValue().onResult(ActionConfirmationResult.CONFIRMATION_NEGATIVE);
        verify(mListener)
                .onConfirmationDialogResult(
                        DialogType.COLLABORATION, ActionConfirmationResult.CONFIRMATION_NEGATIVE);
        verify(mOnResult).onResult(ActionConfirmationResult.CONFIRMATION_NEGATIVE);

        handler.performAction();
        verify(mTabGroupModelFilter).closeTabs(any(TabClosureParams.class));

        verifyNoMoreInteractions(mListener);
    }

    @Test
    public void testUpdateTabClosureParams_NoOp_NoPlaceholders_AllTabs() {
        TabClosureParams params = TabClosureParams.closeAllTabs().build();
        TabClosureParams newParams =
                TabRemoverImpl.fixupTabClosureParams(
                        mTabModel, params, /* placeholderTabs= */ null, /* preventUndo= */ false);
        assertEquals(params, newParams);
    }

    @Test
    public void testUpdateTabClosureParams_Placeholders_AllTabs() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        Tab tab1 = mTabModel.addTab(/* id= */ 1);
        TabClosureParams params = TabClosureParams.closeAllTabs().build();
        TabClosureParams newParams =
                TabRemoverImpl.fixupTabClosureParams(
                        mTabModel, params, List.of(tab1), /* preventUndo= */ false);
        assertNotEquals(params, newParams);
        assertFalse(newParams.isAllTabs);
        assertEquals(List.of(tab0), newParams.tabs);
    }

    @Test
    public void testUpdateTabClosureParams_NoPlaceholders_CloseTab() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        TabClosureParams params =
                TabClosureParams.closeTab(tab0)
                        .allowUndo(true)
                        .withUndoRunnable(mUndoRunnable)
                        .build();
        TabClosureParams newParams =
                TabRemoverImpl.fixupTabClosureParams(
                        mTabModel, params, /* placeholderTabs= */ null, /* preventUndo= */ false);
        assertEquals(params, newParams);
    }

    @Test
    public void testUpdateTabClosureParams_NoPlaceholders_CloseTab_PreventUndo() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        TabClosureParams params =
                TabClosureParams.closeTab(tab0)
                        .allowUndo(true)
                        .withUndoRunnable(mUndoRunnable)
                        .build();
        TabClosureParams newParams =
                TabRemoverImpl.fixupTabClosureParams(
                        mTabModel, params, /* placeholderTabs= */ null, /* preventUndo= */ true);
        assertEquals(params.tabs, newParams.tabs);
        assertFalse(newParams.allowUndo);
    }

    @Test
    public void testUpdateTabClosureParams_Placeholder_CloseTab() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        Tab tab1 = mTabModel.addTab(/* id= */ 1);
        TabClosureParams params =
                TabClosureParams.closeTab(tab0)
                        .allowUndo(true)
                        .withUndoRunnable(mUndoRunnable)
                        .build();
        List<Tab> placeholderTabs = List.of(tab1);
        TabClosureParams newParams =
                TabRemoverImpl.fixupTabClosureParams(
                        mTabModel, params, placeholderTabs, /* preventUndo= */ false);
        assertNotEquals(params, newParams);
        assertEquals(params.tabCloseType, newParams.tabCloseType);
        assertEquals(params.tabs, newParams.tabs);
        assertNotEquals(params.undoRunnable, newParams.undoRunnable);

        newParams.undoRunnable.run();
        verify(mUndoRunnable).run();
        verify(mMockTabRemover)
                .forceCloseTabs(
                        argThat(
                                (TabClosureParams placeholderCloseParams) -> {
                                    return placeholderCloseParams.tabs.equals(placeholderTabs);
                                }));
    }

    @Test
    public void testUpdateTabClosureParams_NoPlaceholders_CloseTabs() {
        mTabModel.addTab(/* id= */ 0);
        Tab tab1 = mTabModel.addTab(/* id= */ 1);
        TabClosureParams params =
                TabClosureParams.closeTabs(List.of(tab1))
                        .allowUndo(true)
                        .hideTabGroups(true)
                        .withUndoRunnable(mUndoRunnable)
                        .build();
        TabClosureParams newParams =
                TabRemoverImpl.fixupTabClosureParams(
                        mTabModel, params, /* placeholderTabs= */ null, /* preventUndo= */ false);
        assertEquals(params, newParams);
    }

    @Test
    public void testUpdateTabClosureParams_NoPlaceholders_CloseTabs_PreventUndo() {
        mTabModel.addTab(/* id= */ 0);
        Tab tab1 = mTabModel.addTab(/* id= */ 1);
        TabClosureParams params =
                TabClosureParams.closeTabs(List.of(tab1))
                        .allowUndo(true)
                        .hideTabGroups(true)
                        .withUndoRunnable(mUndoRunnable)
                        .build();
        TabClosureParams newParams =
                TabRemoverImpl.fixupTabClosureParams(
                        mTabModel, params, /* placeholderTabs= */ null, /* preventUndo= */ true);
        assertEquals(params.tabs, newParams.tabs);
        assertFalse(newParams.allowUndo);
    }

    @Test
    public void testUpdateTabClosureParams_Placeholder_CloseTabs() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        Tab tab1 = mTabModel.addTab(/* id= */ 1);
        Tab tab2 = mTabModel.addTab(/* id= */ 2);
        TabClosureParams params =
                TabClosureParams.closeTabs(List.of(tab0, tab1))
                        .allowUndo(true)
                        .withUndoRunnable(mUndoRunnable)
                        .build();
        List<Tab> placeholderTabs = List.of(tab2);
        TabClosureParams newParams =
                TabRemoverImpl.fixupTabClosureParams(
                        mTabModel, params, placeholderTabs, /* preventUndo= */ false);
        assertNotEquals(params, newParams);
        assertEquals(params.tabCloseType, newParams.tabCloseType);
        assertEquals(params.tabs, newParams.tabs);
        assertNotEquals(params.undoRunnable, newParams.undoRunnable);

        newParams.undoRunnable.run();
        verify(mUndoRunnable).run();
        verify(mMockTabRemover)
                .forceCloseTabs(
                        argThat(
                                (TabClosureParams placeholderCloseParams) -> {
                                    return placeholderCloseParams.tabs.equals(placeholderTabs);
                                }));
    }

    @Test
    public void testRemoveTabHandler_NoDialog() {
        int id = 0;
        Tab tab0 = mTabModel.addTab(id);
        tab0.setTabGroupId(TAB_GROUP_ID.tabGroupId);

        mTabRemoverImpl.removeTab(tab0, /* allowDialog= */ false, mListener);
        verify(mTabModelRemover).doTabRemovalFlow(mHandlerCaptor.capture(), eq(false));
        TabModelRemoverFlowHandler handler = mHandlerCaptor.getValue();

        SavedTabGroupTab savedTab = new SavedTabGroupTab();
        savedTab.localId = id;
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.localId = TAB_GROUP_ID;
        savedTabGroup.collaborationId = COLLABORATION_ID;
        savedTabGroup.savedTabs.add(savedTab);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_ID});
        when(mTabGroupSyncService.getGroup(SYNC_ID)).thenReturn(savedTabGroup);

        GroupsPendingDestroy groupsPendingDestroy = handler.computeGroupsPendingDestroy();
        assertFalse(groupsPendingDestroy.isEmpty());
        assertFalse(groupsPendingDestroy.collaborationGroupsDestroyed.isEmpty());
        assertTrue(groupsPendingDestroy.syncedGroupsDestroyed.isEmpty());

        Tab placeholderTab = mTabModel.addTab(/* id= */ 1);
        handler.onPlaceholderTabsCreated(List.of(placeholderTab));

        handler.performAction();
        verify(mTabModel).removeTab(tab0);
        verify(mListener)
                .onConfirmationDialogResult(
                        DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verifyNoMoreInteractions(mListener);
    }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.eq;
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

/** Unit tests for {@link TabUngrouperImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabUngrouperImplUnitTest {
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
    @Mock private TabModelActionListener mListener;
    @Mock private Callback<Integer> mOnResult;
    @Mock private DataSharingService mDataSharingService;
    @Mock private TabGroupSyncService mTabGroupSyncService;

    @Captor private ArgumentCaptor<TabModelRemoverFlowHandler> mHandlerCaptor;
    @Captor private ArgumentCaptor<Callback<Integer>> mOnResultCaptor;

    private MockTabModel mTabModel;
    private TabUngrouperImpl mTabUngrouperImpl;

    @Before
    public void setUp() {
        DataSharingServiceFactory.setForTesting(mDataSharingService);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});

        when(mProfile.isOffTheRecord()).thenReturn(false);
        mTabModel = new MockTabModel(mProfile, null);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModelRemover.getTabGroupModelFilter()).thenReturn(mTabGroupModelFilter);
        when(mTabModelRemover.getActionConfirmationManager())
                .thenReturn(mActionConfirmationManager);
        mTabUngrouperImpl = new TabUngrouperImpl(mTabModelRemover);
    }

    @Test
    public void testUngroupTabsHandler_NoDialog() {
        Tab tab0 = mTabModel.addTab(/* id= */ 0);
        tab0.setTabGroupId(TAB_GROUP_ID.tabGroupId);
        when(mTabGroupModelFilter.isTabInTabGroup(tab0)).thenReturn(true);

        mTabUngrouperImpl.ungroupTabs(
                List.of(tab0), /* trailing= */ true, /* allowDialog= */ true, mListener);
        verify(mTabModelRemover).doTabRemovalFlow(mHandlerCaptor.capture(), eq(true));
        TabModelRemoverFlowHandler handler = mHandlerCaptor.getValue();

        GroupsPendingDestroy groupsPendingDestroy = handler.computeGroupsPendingDestroy();
        assertTrue(groupsPendingDestroy.isEmpty());

        // No placeholder created.

        handler.performAction();
        verify(mTabGroupModelFilter)
                .moveTabOutOfGroupInDirection(tab0.getId(), /* trailing= */ true);
        verify(mListener)
                .onConfirmationDialogResult(
                        DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verifyNoMoreInteractions(mListener);
    }

    @Test
    public void testUngroupTabsHandler_UngroupTabGroup_RootId_DestructionOnly_ImmediateContinue() {
        int id = 0;
        Tab tab0 = mTabModel.addTab(id);
        tab0.setTabGroupId(TAB_GROUP_ID.tabGroupId);
        tab0.setRootId(id);
        when(mTabGroupModelFilter.getRelatedTabListForRootId(id)).thenReturn(List.of(tab0));
        when(mTabGroupModelFilter.isTabInTabGroup(tab0)).thenReturn(true);

        mTabUngrouperImpl.ungroupTabGroup(
                id, /* trailing= */ true, /* allowDialog= */ true, mListener);
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
        verify(mActionConfirmationManager).processUngroupAttempt(mOnResultCaptor.capture());
        mOnResultCaptor.getValue().onResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verify(mListener)
                .onConfirmationDialogResult(
                        DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verify(mOnResult).onResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);

        handler.performAction();
        verify(mTabGroupModelFilter).moveTabOutOfGroupInDirection(id, /* trailing= */ true);

        verifyNoMoreInteractions(mListener);
    }

    @Test
    public void
            testUngroupTabsHandler_UngroupTabGroup_RootId_DestructionOnly_ConfirmationPositive() {
        int id = 0;
        Tab tab0 = mTabModel.addTab(id);
        tab0.setTabGroupId(TAB_GROUP_ID.tabGroupId);
        tab0.setRootId(id);
        when(mTabGroupModelFilter.getRelatedTabListForRootId(id)).thenReturn(List.of(tab0));
        when(mTabGroupModelFilter.isTabInTabGroup(tab0)).thenReturn(true);

        mTabUngrouperImpl.ungroupTabGroup(
                id, /* trailing= */ true, /* allowDialog= */ true, mListener);
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
        verify(mActionConfirmationManager).processUngroupAttempt(mOnResultCaptor.capture());
        mOnResultCaptor.getValue().onResult(ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mListener)
                .onConfirmationDialogResult(
                        DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mOnResult).onResult(ActionConfirmationResult.CONFIRMATION_POSITIVE);

        handler.performAction();
        verify(mTabGroupModelFilter).moveTabOutOfGroupInDirection(id, /* trailing= */ true);

        verifyNoMoreInteractions(mListener);
    }

    @Test
    public void testUngroupTabsHandler_UngroupTabGroup_TabGroupId_DestructionOnly() {
        int id = 0;
        Tab tab0 = mTabModel.addTab(id);
        Token tabGroupId = TAB_GROUP_ID.tabGroupId;
        tab0.setTabGroupId(tabGroupId);
        tab0.setRootId(id);
        when(mTabGroupModelFilter.getRootIdFromStableId(tabGroupId)).thenReturn(id);
        when(mTabGroupModelFilter.getRelatedTabListForRootId(id)).thenReturn(List.of(tab0));
        when(mTabGroupModelFilter.isTabInTabGroup(tab0)).thenReturn(true);

        mTabUngrouperImpl.ungroupTabGroup(
                tabGroupId, /* trailing= */ false, /* allowDialog= */ true, mListener);
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
        verify(mActionConfirmationManager).processUngroupAttempt(mOnResultCaptor.capture());
        mOnResultCaptor.getValue().onResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verify(mListener)
                .onConfirmationDialogResult(
                        DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verify(mOnResult).onResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);

        handler.performAction();
        verify(mTabGroupModelFilter).moveTabOutOfGroupInDirection(id, /* trailing= */ false);

        verifyNoMoreInteractions(mListener);
    }

    @Test
    public void testUngroupTabsHandler_UngroupsTabs_DestructionOnly() {
        int id = 0;
        Tab tab0 = mTabModel.addTab(id);
        tab0.setTabGroupId(TAB_GROUP_ID.tabGroupId);
        when(mTabGroupModelFilter.isTabInTabGroup(tab0)).thenReturn(true);

        mTabUngrouperImpl.ungroupTabs(
                List.of(tab0), /* trailing= */ false, /* allowDialog= */ true, mListener);
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
        verify(mActionConfirmationManager).processUngroupTabAttempt(mOnResultCaptor.capture());
        mOnResultCaptor.getValue().onResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verify(mListener)
                .onConfirmationDialogResult(
                        DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verify(mOnResult).onResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);

        handler.performAction();
        verify(mTabGroupModelFilter).moveTabOutOfGroupInDirection(id, /* trailing= */ false);

        verifyNoMoreInteractions(mListener);
    }

    @Test
    public void testUngroupTabsHandler_Owner() {
        int id = 0;
        Tab tab0 = mTabModel.addTab(id);
        when(mTabGroupModelFilter.isTabInTabGroup(tab0)).thenReturn(true);
        tab0.setTabGroupId(TAB_GROUP_ID.tabGroupId);

        mTabUngrouperImpl.ungroupTabs(
                List.of(tab0), /* trailing= */ true, /* allowDialog= */ true, mListener);
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
        verify(mTabGroupModelFilter).moveTabOutOfGroupInDirection(id, /* trailing= */ true);

        verifyNoMoreInteractions(mListener);
    }

    @Test
    public void testUngroupTabsHandler_Member() {
        int id = 0;
        Tab tab0 = mTabModel.addTab(id);
        tab0.setTabGroupId(TAB_GROUP_ID.tabGroupId);
        when(mTabGroupModelFilter.isTabInTabGroup(tab0)).thenReturn(true);

        mTabUngrouperImpl.ungroupTabs(
                List.of(tab0), /* trailing= */ true, /* allowDialog= */ true, mListener);
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
        verify(mTabGroupModelFilter).moveTabOutOfGroupInDirection(id, /* trailing= */ true);

        verifyNoMoreInteractions(mListener);
    }
}

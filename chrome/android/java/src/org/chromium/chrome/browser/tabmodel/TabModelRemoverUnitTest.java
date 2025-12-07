// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
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
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabGroupUtils.GroupsPendingDestroy;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager.MaybeBlockingResult;
import org.chromium.chrome.browser.tabmodel.TabModelRemover.TabModelRemoverFlowHandler;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;

/** Unit tests for {@link TabModelRemover}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabModelRemoverUnitTest {
    private static final String COLLABORATION_ID = "collaboration";
    private static final String TAB_GROUP_TITLE = "My Title";
    private static final LocalTabGroupId TAB_GROUP_1 = new LocalTabGroupId(new Token(1L, 2L));
    private static final int ROOT_ID_1 = 1;
    private static final LocalTabGroupId TAB_GROUP_2 = new LocalTabGroupId(new Token(2L, 3L));
    private static final int ROOT_ID_2 = 3;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private TabGroupModelFilterInternal mTabGroupModelFilter;
    @Mock private TabModelRemoverFlowHandler mHandler;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabCreator mTabCreator;
    @Mock private DataSharingService mDataSharingService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock private Runnable mFinishBlocking;

    @Captor private ArgumentCaptor<Callback<@ActionConfirmationResult Integer>> mOnResultCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mOnDeleteGroupResultCaptor;
    @Captor private ArgumentCaptor<Callback<MaybeBlockingResult>> mOnMaybeBlockingResultCaptor;
    @Captor private ArgumentCaptor<List<Tab>> mNewTabCreationCaptor;

    private MockTabModel mTabModel;
    private TabModelRemover mTabModelRemover;
    private InOrder mHandlerInOrder;
    private int mNextTabId;
    private SavedTabGroup mSavedTabGroup1;
    private SavedTabGroup mSavedTabGroup2;

    @Before
    public void setUp() {
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        doReturn(true).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfile);

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(TestAccounts.ACCOUNT1);

        DataSharingServiceFactory.setForTesting(mDataSharingService);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);

        mNextTabId = 0;
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mTabModel = spy(new MockTabModel(mProfile, null));
        mTabModel.setTabCreatorForTesting(mTabCreator);
        mTabModel.setActive(true);

        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_1.tabGroupId)).thenReturn(true);
        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_2.tabGroupId)).thenReturn(true);
        when(mTabGroupModelFilter.getTabGroupTitle(any(Token.class))).thenReturn(TAB_GROUP_TITLE);

        doAnswer(
                        invocation -> {
                            Tab tab = mTabModel.addTab(mNextTabId++);
                            tab.setTabGroupId(TAB_GROUP_1.tabGroupId);
                            return tab;
                        })
                .when(mTabCreator)
                .createNewTab(any(), anyInt(), any());

        mTabModelRemover =
                new TabModelRemover(
                        RuntimeEnvironment.application,
                        mModalDialogManager,
                        () -> mTabGroupModelFilter);
        mHandlerInOrder = inOrder(mHandler);

        mSavedTabGroup1 = new SavedTabGroup();
        mSavedTabGroup1.localId = TAB_GROUP_1;
        mSavedTabGroup1.title = TAB_GROUP_TITLE;
        mSavedTabGroup1.collaborationId = COLLABORATION_ID;
        when(mTabGroupSyncService.getGroup(TAB_GROUP_1)).thenReturn(mSavedTabGroup1);

        mSavedTabGroup2 = new SavedTabGroup();
        mSavedTabGroup2.localId = TAB_GROUP_2;
        mSavedTabGroup2.title = TAB_GROUP_TITLE;
        mSavedTabGroup2.collaborationId = COLLABORATION_ID;
        when(mTabGroupSyncService.getGroup(TAB_GROUP_2)).thenReturn(mSavedTabGroup2);
    }

    @Test
    public void testGetActionConfirmationManager() {
        ActionConfirmationManager actionConfirmationManager =
                mTabModelRemover.getActionConfirmationManager();
        assertNotNull(actionConfirmationManager);
        assertEquals(actionConfirmationManager, mTabModelRemover.getActionConfirmationManager());
    }

    @Test
    public void testGetTabGroupModelFilter() {
        assertEquals(mTabGroupModelFilter, mTabModelRemover.getTabGroupModelFilter());
    }

    @Test
    public void testTabRemovalFlow_SingleCollaboration_WithDialog_PositiveConfirmation() {
        GroupsPendingDestroy groupsPendingDestroy = new GroupsPendingDestroy();
        groupsPendingDestroy.collaborationGroupsDestroyed.add(TAB_GROUP_1);
        when(mHandler.computeGroupsPendingDestroy()).thenReturn(groupsPendingDestroy);
        when(mCollaborationService.getCurrentUserRoleForGroup(COLLABORATION_ID))
                .thenReturn(MemberRole.OWNER);

        Tab standaloneTab = mTabModel.addTab(mNextTabId++);
        Tab tab = mTabModel.addTab(mNextTabId++);
        tab.setTabGroupId(TAB_GROUP_1.tabGroupId);
        mTabModel.setIndex(mTabModel.indexOf(tab), TabSelectionType.FROM_USER);
        assertEquals(1, mTabModel.index());
        assertEquals(tab, mTabModel.getTabAt(mTabModel.index()));

        mTabModelRemover.doTabRemovalFlow(mHandler, /* allowDialog= */ true);

        mHandlerInOrder.verify(mHandler).computeGroupsPendingDestroy();

        mHandlerInOrder
                .verify(mHandler)
                .showCollaborationKeepDialog(
                        eq(MemberRole.OWNER),
                        eq(TAB_GROUP_TITLE),
                        mOnMaybeBlockingResultCaptor.capture());
        mHandlerInOrder.verify(mHandler).onPlaceholderTabsCreated(mNewTabCreationCaptor.capture());
        assertEquals(
                groupsPendingDestroy.collaborationGroupsDestroyed.size(),
                mNewTabCreationCaptor.getValue().size());

        // New placeholder tab should be selected.
        assertNotEquals(standaloneTab, mTabModel.getTabAt(mTabModel.index()));
        assertNotEquals(tab, mTabModel.getTabAt(mTabModel.index()));

        mHandlerInOrder.verify(mHandler).performAction();

        mOnMaybeBlockingResultCaptor
                .getValue()
                .onResult(
                        new MaybeBlockingResult(
                                ActionConfirmationResult.CONFIRMATION_POSITIVE, null));

        verifyNoMoreInteractions(mHandler);

        // New placeholder tab should still be selected.
        assertNotEquals(standaloneTab, mTabModel.getTabAt(mTabModel.index()));
        assertNotEquals(tab, mTabModel.getTabAt(mTabModel.index()));
    }

    @Test
    public void testTabRemovalFlow_SingleCollaboration_WithDialog_NegativeConfirmation_Owner() {
        GroupsPendingDestroy groupsPendingDestroy = new GroupsPendingDestroy();
        groupsPendingDestroy.collaborationGroupsDestroyed.add(TAB_GROUP_1);
        when(mHandler.computeGroupsPendingDestroy()).thenReturn(groupsPendingDestroy);
        when(mCollaborationService.getCurrentUserRoleForGroup(COLLABORATION_ID))
                .thenReturn(MemberRole.OWNER);

        Tab tab = mTabModel.addTab(mNextTabId++);
        tab.setTabGroupId(TAB_GROUP_1.tabGroupId);

        mTabModelRemover.doTabRemovalFlow(mHandler, /* allowDialog= */ true);

        mHandlerInOrder.verify(mHandler).computeGroupsPendingDestroy();

        mHandlerInOrder
                .verify(mHandler)
                .showCollaborationKeepDialog(
                        eq(MemberRole.OWNER),
                        eq(TAB_GROUP_TITLE),
                        mOnMaybeBlockingResultCaptor.capture());
        mHandlerInOrder.verify(mHandler).onPlaceholderTabsCreated(mNewTabCreationCaptor.capture());
        assertEquals(
                groupsPendingDestroy.collaborationGroupsDestroyed.size(),
                mNewTabCreationCaptor.getValue().size());
        mHandlerInOrder.verify(mHandler).performAction();

        mOnMaybeBlockingResultCaptor
                .getValue()
                .onResult(
                        new MaybeBlockingResult(
                                ActionConfirmationResult.CONFIRMATION_NEGATIVE, mFinishBlocking));

        verify(mTabModel).commitAllTabClosures();

        verify(mCollaborationService)
                .deleteGroup(eq(COLLABORATION_ID), mOnDeleteGroupResultCaptor.capture());

        mOnDeleteGroupResultCaptor.getValue().onResult(true);
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
        verify(mFinishBlocking).run();

        verifyNoMoreInteractions(mHandler);
    }

    @Test
    public void testTabRemovalFlow_SingleCollaboration_WithDialog_NegativeConfirmation_Member() {
        GroupsPendingDestroy groupsPendingDestroy = new GroupsPendingDestroy();
        groupsPendingDestroy.collaborationGroupsDestroyed.add(TAB_GROUP_1);
        when(mHandler.computeGroupsPendingDestroy()).thenReturn(groupsPendingDestroy);
        when(mCollaborationService.getCurrentUserRoleForGroup(COLLABORATION_ID))
                .thenReturn(MemberRole.MEMBER);

        Tab tab = mTabModel.addTab(mNextTabId++);
        tab.setTabGroupId(TAB_GROUP_1.tabGroupId);

        mTabModelRemover.doTabRemovalFlow(mHandler, /* allowDialog= */ true);

        mHandlerInOrder.verify(mHandler).computeGroupsPendingDestroy();

        mHandlerInOrder
                .verify(mHandler)
                .showCollaborationKeepDialog(
                        eq(MemberRole.MEMBER),
                        eq(TAB_GROUP_TITLE),
                        mOnMaybeBlockingResultCaptor.capture());
        mHandlerInOrder.verify(mHandler).onPlaceholderTabsCreated(mNewTabCreationCaptor.capture());
        assertEquals(
                groupsPendingDestroy.collaborationGroupsDestroyed.size(),
                mNewTabCreationCaptor.getValue().size());
        mHandlerInOrder.verify(mHandler).performAction();

        mOnMaybeBlockingResultCaptor
                .getValue()
                .onResult(
                        new MaybeBlockingResult(
                                ActionConfirmationResult.CONFIRMATION_NEGATIVE, mFinishBlocking));

        verify(mTabModel).commitAllTabClosures();
        verify(mCollaborationService)
                .leaveGroup(eq(COLLABORATION_ID), mOnDeleteGroupResultCaptor.capture());

        mOnDeleteGroupResultCaptor.getValue().onResult(false);
        verify(mModalDialogManager).showDialog(any(), anyInt());
        verify(mFinishBlocking).run();

        verifyNoMoreInteractions(mHandler);
    }

    @Test
    public void
            testTabRemovalFlow_SingleCollaboration_WithDialog_NoCollaborationData_UnknownRole() {
        GroupsPendingDestroy groupsPendingDestroy = new GroupsPendingDestroy();
        groupsPendingDestroy.collaborationGroupsDestroyed.add(TAB_GROUP_1);
        when(mHandler.computeGroupsPendingDestroy()).thenReturn(groupsPendingDestroy);
        when(mCollaborationService.getCurrentUserRoleForGroup(COLLABORATION_ID))
                .thenReturn(MemberRole.UNKNOWN);

        Tab tab = mTabModel.addTab(mNextTabId++);
        tab.setTabGroupId(TAB_GROUP_1.tabGroupId);

        mTabModelRemover.doTabRemovalFlow(mHandler, /* allowDialog= */ true);

        mHandlerInOrder.verify(mHandler).computeGroupsPendingDestroy();

        mHandlerInOrder.verify(mHandler).onPlaceholderTabsCreated(mNewTabCreationCaptor.capture());
        assertEquals(
                groupsPendingDestroy.collaborationGroupsDestroyed.size(),
                mNewTabCreationCaptor.getValue().size());
        mHandlerInOrder.verify(mHandler).performAction();
        verifyNoMoreInteractions(mHandler);
    }

    @Test
    public void testTabRemovalFlow_MultipleCollaborations() {
        GroupsPendingDestroy groupsPendingDestroy = new GroupsPendingDestroy();
        groupsPendingDestroy.collaborationGroupsDestroyed.add(TAB_GROUP_1);
        groupsPendingDestroy.collaborationGroupsDestroyed.add(TAB_GROUP_2);
        when(mHandler.computeGroupsPendingDestroy()).thenReturn(groupsPendingDestroy);

        Tab tab = mTabModel.addTab(mNextTabId++);
        tab.setTabGroupId(TAB_GROUP_1.tabGroupId);
        tab = mTabModel.addTab(mNextTabId++);
        tab.setTabGroupId(TAB_GROUP_2.tabGroupId);

        mTabModelRemover.doTabRemovalFlow(mHandler, /* allowDialog= */ false);

        mHandlerInOrder.verify(mHandler).computeGroupsPendingDestroy();
        mHandlerInOrder.verify(mHandler).onPlaceholderTabsCreated(mNewTabCreationCaptor.capture());
        assertEquals(
                groupsPendingDestroy.collaborationGroupsDestroyed.size(),
                mNewTabCreationCaptor.getValue().size());
        mHandlerInOrder.verify(mHandler).performAction();
        verifyNoMoreInteractions(mHandler);
    }

    @Test
    public void testTabRemovalFlow_SingleCollaboration_WithoutDialog() {
        GroupsPendingDestroy groupsPendingDestroy = new GroupsPendingDestroy();
        groupsPendingDestroy.collaborationGroupsDestroyed.add(TAB_GROUP_1);
        when(mHandler.computeGroupsPendingDestroy()).thenReturn(groupsPendingDestroy);

        Tab tab = mTabModel.addTab(mNextTabId++);
        tab.setTabGroupId(TAB_GROUP_1.tabGroupId);

        mTabModelRemover.doTabRemovalFlow(mHandler, /* allowDialog= */ false);

        mHandlerInOrder.verify(mHandler).computeGroupsPendingDestroy();
        mHandlerInOrder.verify(mHandler).onPlaceholderTabsCreated(mNewTabCreationCaptor.capture());
        assertEquals(
                groupsPendingDestroy.collaborationGroupsDestroyed.size(),
                mNewTabCreationCaptor.getValue().size());
        mHandlerInOrder.verify(mHandler).performAction();
        verifyNoMoreInteractions(mHandler);
    }

    @Test
    public void testTabRemovalFlow_SyncedGroupsOnly_WithDialog_Confirm() {
        GroupsPendingDestroy groupsPendingDestroy = new GroupsPendingDestroy();
        groupsPendingDestroy.syncedGroupsDestroyed.add(TAB_GROUP_1);
        when(mHandler.computeGroupsPendingDestroy()).thenReturn(groupsPendingDestroy);

        mTabModelRemover.doTabRemovalFlow(mHandler, /* allowDialog= */ true);

        mHandlerInOrder.verify(mHandler).computeGroupsPendingDestroy();
        mHandlerInOrder
                .verify(mHandler)
                .showTabGroupDeletionConfirmationDialog(mOnResultCaptor.capture());

        mOnResultCaptor.getValue().onResult(ActionConfirmationResult.CONFIRMATION_POSITIVE);

        mHandlerInOrder.verify(mHandler).performAction();
        verifyNoMoreInteractions(mHandler);
    }

    @Test
    public void testTabRemovalFlow_SyncedGroupsOnly_WithDialog_ImmediateContinue() {
        GroupsPendingDestroy groupsPendingDestroy = new GroupsPendingDestroy();
        groupsPendingDestroy.syncedGroupsDestroyed.add(TAB_GROUP_1);
        groupsPendingDestroy.syncedGroupsDestroyed.add(TAB_GROUP_2);
        when(mHandler.computeGroupsPendingDestroy()).thenReturn(groupsPendingDestroy);

        mTabModelRemover.doTabRemovalFlow(mHandler, /* allowDialog= */ true);

        mHandlerInOrder.verify(mHandler).computeGroupsPendingDestroy();
        mHandlerInOrder
                .verify(mHandler)
                .showTabGroupDeletionConfirmationDialog(mOnResultCaptor.capture());

        mOnResultCaptor.getValue().onResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);

        mHandlerInOrder.verify(mHandler).performAction();
        verifyNoMoreInteractions(mHandler);
    }

    @Test
    public void testTabRemovalFlow_SyncedGroupsOnly_WithDialog_ConfirmationNegative() {
        GroupsPendingDestroy groupsPendingDestroy = new GroupsPendingDestroy();
        groupsPendingDestroy.syncedGroupsDestroyed.add(TAB_GROUP_1);
        when(mHandler.computeGroupsPendingDestroy()).thenReturn(groupsPendingDestroy);

        mTabModelRemover.doTabRemovalFlow(mHandler, /* allowDialog= */ true);

        mHandlerInOrder.verify(mHandler).computeGroupsPendingDestroy();
        mHandlerInOrder
                .verify(mHandler)
                .showTabGroupDeletionConfirmationDialog(mOnResultCaptor.capture());

        mOnResultCaptor.getValue().onResult(ActionConfirmationResult.CONFIRMATION_NEGATIVE);
        verifyNoMoreInteractions(mHandler);
    }

    @Test
    public void testTabRemovalFlow_SyncedGroupsOnly_WithoutDialog() {
        GroupsPendingDestroy groupsPendingDestroy = new GroupsPendingDestroy();
        groupsPendingDestroy.syncedGroupsDestroyed.add(TAB_GROUP_1);
        when(mHandler.computeGroupsPendingDestroy()).thenReturn(groupsPendingDestroy);

        mTabModelRemover.doTabRemovalFlow(mHandler, /* allowDialog= */ false);

        mHandlerInOrder.verify(mHandler).computeGroupsPendingDestroy();
        mHandlerInOrder.verify(mHandler).performAction();
        verifyNoMoreInteractions(mHandler);
    }

    @Test
    public void testTabRemovalFlow_NoGroupsDestroyed() {
        GroupsPendingDestroy groupsPendingDestroy = new GroupsPendingDestroy();
        when(mHandler.computeGroupsPendingDestroy()).thenReturn(groupsPendingDestroy);

        mTabModelRemover.doTabRemovalFlow(mHandler, /* allowDialog= */ true);

        mHandlerInOrder.verify(mHandler).computeGroupsPendingDestroy();
        mHandlerInOrder.verify(mHandler).performAction();
        verifyNoMoreInteractions(mHandler);
    }
}

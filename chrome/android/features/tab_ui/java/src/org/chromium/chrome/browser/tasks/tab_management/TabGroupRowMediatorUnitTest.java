// Copyright 2024 The Chromium Authors
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
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.CLUSTER_DATA;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DELETE_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DESTROYABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DISPLAY_AS_SHARED;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.LEAVE_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.OPEN_RUNNABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.SHARED_IMAGE_TILES_VIEW;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.COLLABORATION_ID1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER2;
import static org.chromium.components.tab_group_sync.SyncedGroupTestHelper.SYNC_GROUP_ID1;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.core.util.Supplier;

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
import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupFaviconCluster.ClusterData;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.SharedGroupTestHelper;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.SyncedGroupTestHelper;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link TabGroupRowMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.DATA_SHARING})
public class TabGroupRowMediatorUnitTest {
    private static final Token GROUP_ID1 = new Token(1, 1);
    private static final String TITLE = "Title";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private TabRemover mTabRemover;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private DataSharingUIDelegate mDataSharingUiDelegate;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ServiceStatus mServiceStatus;
    @Mock private PaneManager mPaneManager;
    @Mock private TabGroupUiActionHandler mTabGroupUiActionHandler;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ActionConfirmationManager mActionConfirmationManager;
    @Mock private FaviconResolver mFaviconResolver;
    @Mock private Supplier<@GroupWindowState Integer> mFetchGroupState;
    @Mock private TabSwitcherPaneBase mTabSwitcherPaneBase;
    @Mock private DataSharingTabManager mDataSharingTabManager;

    @Captor private ArgumentCaptor<Callback<@ActionConfirmationResult Integer>> mConfirmationCaptor;

    private GURL mUrl1;
    private GURL mUrl2;
    private GURL mUrl3;
    private GURL mUrl4;
    private GURL mUrl5;
    private SyncedGroupTestHelper mSyncedGroupTestHelper;
    private SharedGroupTestHelper mSharedGroupTestHelper;
    private Context mContext;
    private SavedTabGroup mSyncGroup;
    private int mRootId = Tab.INVALID_TAB_ID;

    @Before
    public void setUp() {
        // Cannot initialize even test GURLs too early.
        mUrl1 = JUnitTestGURLs.URL_1;
        mUrl2 = JUnitTestGURLs.URL_2;
        mUrl3 = JUnitTestGURLs.URL_3;
        mUrl4 = JUnitTestGURLs.BLUE_1;
        mUrl5 = JUnitTestGURLs.BLUE_2;
        mSyncedGroupTestHelper = new SyncedGroupTestHelper(mTabGroupSyncService);
        mSharedGroupTestHelper = new SharedGroupTestHelper(mCollaborationService);
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);
        when(mDataSharingService.getUiDelegate()).thenReturn(mDataSharingUiDelegate);
        when(mPaneManager.getPaneForId(PaneId.TAB_SWITCHER)).thenReturn(mTabSwitcherPaneBase);
        when(mTabSwitcherPaneBase.requestOpenTabGroupDialog(anyInt())).thenReturn(true);
    }

    private PropertyModel buildTestModel(GURL... urls) {
        return buildTestModel(/* isShared= */ false, urls);
    }

    private PropertyModel buildTestModel(boolean isShared, GURL... urls) {
        mSyncGroup = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1, GROUP_ID1);
        mSyncGroup.collaborationId = isShared ? COLLABORATION_ID1 : null;
        mSyncGroup.title = TITLE;
        mSyncGroup.savedTabs = SyncedGroupTestHelper.tabsFromUrls(urls);

        mRootId =
                mSyncGroup.savedTabs.isEmpty()
                        ? Tab.INVALID_TAB_ID
                        : mSyncGroup.savedTabs.get(0).localId;
        List<Tab> tabList = new ArrayList<>();
        for (SavedTabGroupTab syncTab : mSyncGroup.savedTabs) {
            Tab tab = mock(Tab.class);
            when(tab.getId()).thenReturn(syncTab.localId);
            when(tab.getRootId()).thenReturn(mRootId);
            when(tab.getTabGroupId()).thenReturn(GROUP_ID1);
            tabList.add(tab);
        }
        when(mTabGroupModelFilter.getRootIdFromTabGroupId(GROUP_ID1)).thenReturn(mRootId);
        when(mTabGroupModelFilter.getTabsInGroup(GROUP_ID1)).thenReturn(tabList);

        TabGroupRowMediator mediator =
                new TabGroupRowMediator(
                        mContext,
                        mSyncGroup,
                        mTabGroupModelFilter,
                        mTabGroupSyncService,
                        mDataSharingService,
                        mCollaborationService,
                        mPaneManager,
                        mTabGroupUiActionHandler,
                        mActionConfirmationManager,
                        mFaviconResolver,
                        mFetchGroupState,
                        /* enableContainment= */ true,
                        mDataSharingTabManager);
        return mediator.getModel();
    }

    @Test
    public void testFavicons_Zero() {
        PropertyModel propertyModel = buildTestModel();
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(0, clusterData.totalCount);
        assertEquals(0, clusterData.firstUrls.size());
    }

    @Test
    public void testFavicons_One() {
        PropertyModel propertyModel = buildTestModel(mUrl1);
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(1, clusterData.totalCount);
        assertEquals(1, clusterData.firstUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, clusterData.firstUrls.get(0));
    }

    @Test
    public void testFavicons_Two() {
        PropertyModel propertyModel = buildTestModel(mUrl1, mUrl2);
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(2, clusterData.totalCount);
        assertEquals(2, clusterData.firstUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, clusterData.firstUrls.get(0));
        assertEquals(JUnitTestGURLs.URL_2, clusterData.firstUrls.get(1));
    }

    @Test
    public void testFavicons_Three() {
        PropertyModel propertyModel = buildTestModel(mUrl1, mUrl2, mUrl3);
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(3, clusterData.totalCount);
        assertEquals(3, clusterData.firstUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, clusterData.firstUrls.get(0));
        assertEquals(JUnitTestGURLs.URL_2, clusterData.firstUrls.get(1));
        assertEquals(JUnitTestGURLs.URL_3, clusterData.firstUrls.get(2));
    }

    @Test
    public void testFavicons_Four() {
        PropertyModel propertyModel = buildTestModel(mUrl1, mUrl2, mUrl3, mUrl4);
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(4, clusterData.totalCount);
        assertEquals(4, clusterData.firstUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, clusterData.firstUrls.get(0));
        assertEquals(JUnitTestGURLs.URL_2, clusterData.firstUrls.get(1));
        assertEquals(JUnitTestGURLs.URL_3, clusterData.firstUrls.get(2));
        assertEquals(JUnitTestGURLs.BLUE_1, clusterData.firstUrls.get(3));
    }

    @Test
    public void testFavicons_Five() {
        PropertyModel propertyModel = buildTestModel(mUrl1, mUrl2, mUrl3, mUrl4, mUrl5);
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(5, clusterData.totalCount);
        assertEquals(4, clusterData.firstUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, clusterData.firstUrls.get(0));
        assertEquals(JUnitTestGURLs.URL_2, clusterData.firstUrls.get(1));
        assertEquals(JUnitTestGURLs.URL_3, clusterData.firstUrls.get(2));
        assertEquals(JUnitTestGURLs.BLUE_1, clusterData.firstUrls.get(3));
    }

    @Test
    public void testNotShared() {
        PropertyModel propertyModel = buildTestModel(/* isShared= */ false, mUrl1);
        assertFalse(propertyModel.get(DISPLAY_AS_SHARED));
        assertNull(propertyModel.get(SHARED_IMAGE_TILES_VIEW));
    }

    @Test
    public void testCollaborationButOnlyOneUser() {
        mSharedGroupTestHelper.mockGetGroupData(COLLABORATION_ID1, GROUP_MEMBER1);
        PropertyModel propertyModel = buildTestModel(/* isShared= */ true, mUrl1);

        assertFalse(propertyModel.get(DISPLAY_AS_SHARED));
        assertNull(propertyModel.get(SHARED_IMAGE_TILES_VIEW));
    }

    @Test
    public void testShared() {
        mSharedGroupTestHelper.mockGetGroupData(COLLABORATION_ID1, GROUP_MEMBER1, GROUP_MEMBER2);
        PropertyModel propertyModel = buildTestModel(/* isShared= */ true, mUrl1);

        assertTrue(propertyModel.get(DISPLAY_AS_SHARED));
        assertNotNull(propertyModel.get(SHARED_IMAGE_TILES_VIEW));
    }

    @Test
    public void testDestroyable() {
        mSharedGroupTestHelper.mockGetGroupData(COLLABORATION_ID1, GROUP_MEMBER1, GROUP_MEMBER2);
        PropertyModel propertyModel = buildTestModel(/* isShared= */ true, mUrl1);

        assertTrue(propertyModel.get(DISPLAY_AS_SHARED));
        propertyModel.get(DESTROYABLE).destroy();
    }

    @Test
    public void testOpen_InCurrent() {
        when(mFetchGroupState.get()).thenReturn(GroupWindowState.IN_CURRENT);
        PropertyModel propertyModel = buildTestModel(/* isShared= */ true, mUrl1);

        propertyModel.get(OPEN_RUNNABLE).run();
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
        verify(mTabSwitcherPaneBase).requestOpenTabGroupDialog(mRootId);
    }

    @Test(expected = AssertionError.class)
    public void testOpen_InCurrent_RequestShowFails() {
        when(mTabSwitcherPaneBase.requestOpenTabGroupDialog(anyInt())).thenReturn(false);
        when(mFetchGroupState.get()).thenReturn(GroupWindowState.IN_CURRENT);
        PropertyModel propertyModel = buildTestModel(/* isShared= */ true, mUrl1);

        propertyModel.get(OPEN_RUNNABLE).run();
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
        verify(mTabSwitcherPaneBase).requestOpenTabGroupDialog(mRootId);
    }

    @Test
    public void testOpen_InCurrentClosing() {
        when(mFetchGroupState.get()).thenReturn(GroupWindowState.IN_CURRENT_CLOSING);
        PropertyModel propertyModel = buildTestModel(/* isShared= */ true, mUrl1);

        propertyModel.get(OPEN_RUNNABLE).run();
        verify(mTabModel).cancelTabClosure(mRootId);
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
        verify(mTabSwitcherPaneBase).requestOpenTabGroupDialog(mRootId);
    }

    @Test
    public void testOpen_InAnother() {
        when(mFetchGroupState.get()).thenReturn(GroupWindowState.IN_ANOTHER);
        PropertyModel propertyModel = buildTestModel(/* isShared= */ true, mUrl1);

        propertyModel.get(OPEN_RUNNABLE).run();
        verifyNoInteractions(mPaneManager);
        verifyNoInteractions(mTabSwitcherPaneBase);
    }

    @Test
    public void testOpen_Hidden() {
        doAnswer(invocationOnMock -> mSyncGroup.localId = new LocalTabGroupId(GROUP_ID1))
                .when(mTabGroupUiActionHandler)
                .openTabGroup(SYNC_GROUP_ID1);
        when(mFetchGroupState.get()).thenReturn(GroupWindowState.HIDDEN);
        PropertyModel propertyModel = buildTestModel(/* isShared= */ true, mUrl1);
        mSyncGroup.localId = null;

        propertyModel.get(OPEN_RUNNABLE).run();
        verify(mTabGroupUiActionHandler).openTabGroup(SYNC_GROUP_ID1);
        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
        verify(mTabSwitcherPaneBase).requestOpenTabGroupDialog(mRootId);
    }

    @Test
    public void testOpen_Hidden_SyncOpenFails() {
        when(mFetchGroupState.get()).thenReturn(GroupWindowState.HIDDEN);
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(null);
        PropertyModel propertyModel = buildTestModel(/* isShared= */ true, mUrl1);
        mSyncGroup.localId = null;

        propertyModel.get(OPEN_RUNNABLE).run();
        verify(mTabGroupUiActionHandler).openTabGroup(SYNC_GROUP_ID1);
        verifyNoInteractions(mPaneManager);
        verifyNoInteractions(mTabSwitcherPaneBase);
    }

    @Test
    public void testDelete_NotShared_InCurrent() {
        when(mFetchGroupState.get()).thenReturn(GroupWindowState.IN_CURRENT);
        PropertyModel propertyModel = buildTestModel(/* isShared= */ false, mUrl1);

        assertNotNull(propertyModel.get(DELETE_RUNNABLE));
        assertNull(propertyModel.get(LEAVE_RUNNABLE));
        propertyModel.get(DELETE_RUNNABLE).run();
        verify(mTabRemover).closeTabs(any(), eq(true));
    }

    @Test
    public void testDelete_NotShared_InAnother() {
        when(mFetchGroupState.get()).thenReturn(GroupWindowState.IN_ANOTHER);
        PropertyModel propertyModel = buildTestModel(/* isShared= */ false, mUrl1);

        assertNotNull(propertyModel.get(DELETE_RUNNABLE));
        assertNull(propertyModel.get(LEAVE_RUNNABLE));
        propertyModel.get(DELETE_RUNNABLE).run();
        verifyNoInteractions(mTabRemover);
        verify(mTabModel, never()).commitTabClosure(anyInt());
        verify(mTabGroupSyncService, never()).removeGroup((String) any());
    }

    @Test
    public void testDelete_NotShared_InCurrentClosing() {
        when(mFetchGroupState.get()).thenReturn(GroupWindowState.IN_CURRENT_CLOSING);
        PropertyModel propertyModel = buildTestModel(/* isShared= */ false, mUrl1);

        assertNotNull(propertyModel.get(DELETE_RUNNABLE));
        assertNull(propertyModel.get(LEAVE_RUNNABLE));
        propertyModel.get(DELETE_RUNNABLE).run();
        verify(mTabModel).commitTabClosure(mRootId);
        verify(mTabGroupSyncService).removeGroup(SYNC_GROUP_ID1);
    }

    @Test
    public void testDelete_NotShared_Hidden_ConfirmationPositive() {
        when(mFetchGroupState.get()).thenReturn(GroupWindowState.HIDDEN);
        PropertyModel propertyModel = buildTestModel(/* isShared= */ false, mUrl1);

        assertNotNull(propertyModel.get(DELETE_RUNNABLE));
        assertNull(propertyModel.get(LEAVE_RUNNABLE));
        propertyModel.get(DELETE_RUNNABLE).run();
        verify(mActionConfirmationManager).processDeleteGroupAttempt(mConfirmationCaptor.capture());

        mConfirmationCaptor.getValue().onResult(ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mTabGroupSyncService).removeGroup(SYNC_GROUP_ID1);
    }

    @Test
    public void testDelete_NotShared_Hidden_ConfirmationNegative() {
        when(mFetchGroupState.get()).thenReturn(GroupWindowState.HIDDEN);
        PropertyModel propertyModel = buildTestModel(/* isShared= */ false, mUrl1);

        assertNotNull(propertyModel.get(DELETE_RUNNABLE));
        assertNull(propertyModel.get(LEAVE_RUNNABLE));
        propertyModel.get(DELETE_RUNNABLE).run();
        verify(mActionConfirmationManager).processDeleteGroupAttempt(mConfirmationCaptor.capture());

        mConfirmationCaptor.getValue().onResult(ActionConfirmationResult.CONFIRMATION_NEGATIVE);
        verify(mTabGroupSyncService, never()).removeGroup(SYNC_GROUP_ID1);
    }

    @Test
    public void testDelete_Shared() {
        GroupData shareGroup =
                new GroupData(
                        COLLABORATION_ID1,
                        TITLE,
                        new GroupMember[] {GROUP_MEMBER1},
                        /* accessToken= */ null);
        when(mCollaborationService.getGroupData(eq(COLLABORATION_ID1))).thenReturn(shareGroup);
        when(mCollaborationService.getCurrentUserRoleForGroup(COLLABORATION_ID1))
                .thenReturn(MemberRole.OWNER);
        when(mFetchGroupState.get()).thenReturn(GroupWindowState.IN_CURRENT);
        PropertyModel propertyModel = buildTestModel(/* isShared= */ true, mUrl1);

        assertNotNull(propertyModel.get(DELETE_RUNNABLE));
        assertNull(propertyModel.get(LEAVE_RUNNABLE));
        propertyModel.get(DELETE_RUNNABLE).run();
        EitherGroupId eitherId = EitherGroupId.createSyncId(SYNC_GROUP_ID1);
        verify(mDataSharingTabManager).leaveOrDeleteFlow(eq(eitherId), anyInt());
    }

    @Test
    public void testLeave_Shared() {
        GroupData shareGroup =
                new GroupData(
                        COLLABORATION_ID1,
                        /* displayName= */ null,
                        new GroupMember[] {GROUP_MEMBER1, GROUP_MEMBER2},
                        /* accessToken= */ null);
        when(mCollaborationService.getGroupData(eq(COLLABORATION_ID1))).thenReturn(shareGroup);
        when(mCollaborationService.getCurrentUserRoleForGroup(COLLABORATION_ID1))
                .thenReturn(MemberRole.MEMBER);
        when(mFetchGroupState.get()).thenReturn(GroupWindowState.IN_CURRENT);
        PropertyModel propertyModel = buildTestModel(/* isShared= */ true, mUrl1, mUrl2);

        assertNull(propertyModel.get(DELETE_RUNNABLE));
        assertNotNull(propertyModel.get(LEAVE_RUNNABLE));
        propertyModel.get(LEAVE_RUNNABLE).run();
        EitherGroupId eitherId = EitherGroupId.createSyncId(SYNC_GROUP_ID1);
        verify(mDataSharingTabManager).leaveOrDeleteFlow(eq(eitherId), anyInt());
    }
}

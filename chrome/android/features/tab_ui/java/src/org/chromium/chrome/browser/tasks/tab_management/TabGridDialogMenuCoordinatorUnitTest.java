// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupOverflowMenuCoordinator.OnItemClickedCallback;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link TabGridDialogMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGridDialogMenuCoordinatorUnitTest {
    private static final int TAB_ID = 123;
    private static final String COLLABORATION_ID1 = "A";
    private static final String GAIA_ID1 = "Z";
    private static final String GAIA_ID2 = "Y";
    private static final String EMAIL = "fake@gmail.com";
    private static final Token TAB_GROUP_TOKEN = Token.createRandom();

    /** Overrides {@link #buildMenuActionItems(boolean, boolean)} to get access to calling it. */
    private static class TestMenuCoordinator extends TabGridDialogMenuCoordinator {
        public TestMenuCoordinator(
                Context context,
                View anchorView,
                OnItemClickedCallback onItemClicked,
                int tabId,
                boolean isIncognito,
                boolean shouldShowDeleteGroup,
                @Nullable TabModel tabModel,
                @Nullable IdentityManager identityManager,
                @Nullable TabGroupSyncService tabGroupSyncService,
                @Nullable DataSharingService dataSharingService) {
            super(
                    context,
                    anchorView,
                    onItemClicked,
                    tabId,
                    isIncognito,
                    shouldShowDeleteGroup,
                    () -> tabModel,
                    identityManager,
                    tabGroupSyncService,
                    dataSharingService);
        }

        @Override
        public ModelList buildMenuActionItems(boolean isIncognito, boolean shouldShowDeleteGroup) {
            return super.buildMenuActionItems(isIncognito, shouldShowDeleteGroup);
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private TabModel mTabModel;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private OnItemClickedCallback mOnItemClickedCallback;

    @Captor private ArgumentCaptor<Callback<GroupDataOrFailureOutcome>> mReadGroupCallbackCaptor;

    private Activity mActivity;
    private View mView;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);

        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.getTabGroupId()).thenReturn(TAB_GROUP_TOKEN);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        DataSharingServiceFactory.setForTesting(mDataSharingService);

        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab);
        when(mTab.getTabGroupId()).thenReturn(TAB_GROUP_TOKEN);
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.collaborationId = COLLABORATION_ID1;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
        CoreAccountInfo coreAccountInfo = CoreAccountInfo.createFromEmailAndGaiaId(EMAIL, GAIA_ID1);
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(coreAccountInfo);
    }

    private void onActivity(TestActivity activity) {
        mActivity = activity;
        mView = new View(activity);
    }

    @Test
    public void testGetCollaborationIdOrNull_NullTabModel() {
        TestMenuCoordinator testMenuCoordinator =
                new TestMenuCoordinator(
                        mActivity,
                        mView,
                        mOnItemClickedCallback,
                        TAB_ID,
                        /* isIncognito= */ false,
                        /* shouldShowDeleteGroup= */ true,
                        /* tabModel= */ null,
                        mIdentityManager,
                        mTabGroupSyncService,
                        mDataSharingService);
        ModelList modelList =
                testMenuCoordinator.buildMenuActionItems(
                        /* isIncognito= */ false, /* shouldShowDeleteGroup= */ true);

        assertEquals(4, modelList.size());
        PropertyModel propertyModel = modelList.get(3).model;
        assertEquals(R.id.delete_tab, propertyModel.get(ListMenuItemProperties.MENU_ITEM_ID));

        testMenuCoordinator.destroy();
    }

    @Test
    public void testGetCollaborationIdOrNull_NullTabGroupSyncService() {
        TestMenuCoordinator testMenuCoordinator =
                new TestMenuCoordinator(
                        mActivity,
                        mView,
                        mOnItemClickedCallback,
                        TAB_ID,
                        /* isIncognito= */ false,
                        /* shouldShowDeleteGroup= */ true,
                        mTabModel,
                        mIdentityManager,
                        /* tabGroupSyncService= */ null,
                        mDataSharingService);
        ModelList modelList =
                testMenuCoordinator.buildMenuActionItems(
                        /* isIncognito= */ false, /* shouldShowDeleteGroup= */ true);

        assertEquals(4, modelList.size());
        PropertyModel propertyModel = modelList.get(3).model;
        assertEquals(R.id.delete_tab, propertyModel.get(ListMenuItemProperties.MENU_ITEM_ID));

        testMenuCoordinator.destroy();
    }

    @Test
    public void testGetCollaborationIdOrNull_NullTab() {
        when(mTabModel.getTabById(anyInt())).thenReturn(null);
        TestMenuCoordinator testMenuCoordinator =
                new TestMenuCoordinator(
                        mActivity,
                        mView,
                        mOnItemClickedCallback,
                        TAB_ID,
                        /* isIncognito= */ false,
                        /* shouldShowDeleteGroup= */ true,
                        mTabModel,
                        mIdentityManager,
                        mTabGroupSyncService,
                        mDataSharingService);
        ModelList modelList =
                testMenuCoordinator.buildMenuActionItems(
                        /* isIncognito= */ false, /* shouldShowDeleteGroup= */ true);

        assertEquals(4, modelList.size());
        PropertyModel propertyModel = modelList.get(3).model;
        assertEquals(R.id.delete_tab, propertyModel.get(ListMenuItemProperties.MENU_ITEM_ID));

        testMenuCoordinator.destroy();
    }

    @Test
    public void testGetCollaborationIdOrNull_NullTabGroupId() {
        when(mTab.getTabGroupId()).thenReturn(null);
        TestMenuCoordinator testMenuCoordinator =
                new TestMenuCoordinator(
                        mActivity,
                        mView,
                        mOnItemClickedCallback,
                        TAB_ID,
                        /* isIncognito= */ false,
                        /* shouldShowDeleteGroup= */ true,
                        mTabModel,
                        mIdentityManager,
                        mTabGroupSyncService,
                        mDataSharingService);
        ModelList modelList =
                testMenuCoordinator.buildMenuActionItems(
                        /* isIncognito= */ false, /* shouldShowDeleteGroup= */ true);

        assertEquals(4, modelList.size());
        PropertyModel propertyModel = modelList.get(3).model;
        assertEquals(R.id.delete_tab, propertyModel.get(ListMenuItemProperties.MENU_ITEM_ID));

        testMenuCoordinator.destroy();
    }

    @Test
    public void testGetCollaborationIdOrNull_NullSavedTabGroup() {
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(null);
        TestMenuCoordinator testMenuCoordinator =
                new TestMenuCoordinator(
                        mActivity,
                        mView,
                        mOnItemClickedCallback,
                        TAB_ID,
                        /* isIncognito= */ false,
                        /* shouldShowDeleteGroup= */ true,
                        mTabModel,
                        mIdentityManager,
                        mTabGroupSyncService,
                        mDataSharingService);
        ModelList modelList =
                testMenuCoordinator.buildMenuActionItems(
                        /* isIncognito= */ false, /* shouldShowDeleteGroup= */ true);

        assertEquals(4, modelList.size());
        PropertyModel propertyModel = modelList.get(3).model;
        assertEquals(R.id.delete_tab, propertyModel.get(ListMenuItemProperties.MENU_ITEM_ID));

        testMenuCoordinator.destroy();
    }

    @Test
    public void testDeleteSharedGroup() {
        TestMenuCoordinator testMenuCoordinator =
                new TestMenuCoordinator(
                        mActivity,
                        mView,
                        mOnItemClickedCallback,
                        TAB_ID,
                        /* isIncognito= */ false,
                        /* shouldShowDeleteGroup= */ true,
                        mTabModel,
                        mIdentityManager,
                        mTabGroupSyncService,
                        mDataSharingService);
        ModelList modelList =
                testMenuCoordinator.buildMenuActionItems(
                        /* isIncognito= */ false, /* shouldShowDeleteGroup= */ true);

        assertEquals(3, modelList.size());

        verify(mDataSharingService)
                .readGroup(eq(COLLABORATION_ID1), mReadGroupCallbackCaptor.capture());
        GroupMember groupMember =
                new GroupMember(
                        GAIA_ID1,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.OWNER,
                        /* avatarUrl= */ null);
        GroupMember[] groupMemberArray = new GroupMember[] {groupMember};
        GroupData groupData =
                new GroupData(
                        COLLABORATION_ID1,
                        /* displayName= */ null,
                        groupMemberArray,
                        /* groupToken= */ null);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        mReadGroupCallbackCaptor.getValue().onResult(outcome);

        assertEquals(4, modelList.size());
        PropertyModel propertyModel = modelList.get(3).model;
        assertEquals(
                R.id.delete_shared_group, propertyModel.get(ListMenuItemProperties.MENU_ITEM_ID));

        testMenuCoordinator.destroy();
    }

    @Test
    public void testLeaveGroup() {
        TestMenuCoordinator testMenuCoordinator =
                new TestMenuCoordinator(
                        mActivity,
                        mView,
                        mOnItemClickedCallback,
                        TAB_ID,
                        /* isIncognito= */ false,
                        /* shouldShowDeleteGroup= */ true,
                        mTabModel,
                        mIdentityManager,
                        mTabGroupSyncService,
                        mDataSharingService);
        ModelList modelList =
                testMenuCoordinator.buildMenuActionItems(
                        /* isIncognito= */ false, /* shouldShowDeleteGroup= */ true);

        assertEquals(3, modelList.size());

        verify(mDataSharingService)
                .readGroup(eq(COLLABORATION_ID1), mReadGroupCallbackCaptor.capture());
        GroupMember groupMember1 =
                new GroupMember(
                        GAIA_ID1,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.MEMBER,
                        /* avatarUrl= */ null);
        GroupMember groupMember2 =
                new GroupMember(
                        GAIA_ID2,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.OWNER,
                        /* avatarUrl= */ null);
        GroupMember[] groupMemberArray = new GroupMember[] {groupMember1, groupMember2};
        GroupData groupData =
                new GroupData(
                        COLLABORATION_ID1,
                        /* displayName= */ null,
                        groupMemberArray,
                        /* groupToken= */ null);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        mReadGroupCallbackCaptor.getValue().onResult(outcome);

        assertEquals(4, modelList.size());
        PropertyModel propertyModel = modelList.get(3).model;
        assertEquals(R.id.leave_group, propertyModel.get(ListMenuItemProperties.MENU_ITEM_ID));

        testMenuCoordinator.destroy();
    }

    @Test
    public void testReadGroup_NullGroupData() {
        TestMenuCoordinator testMenuCoordinator =
                new TestMenuCoordinator(
                        mActivity,
                        mView,
                        mOnItemClickedCallback,
                        TAB_ID,
                        /* isIncognito= */ false,
                        /* shouldShowDeleteGroup= */ true,
                        mTabModel,
                        mIdentityManager,
                        mTabGroupSyncService,
                        mDataSharingService);
        ModelList modelList =
                testMenuCoordinator.buildMenuActionItems(
                        /* isIncognito= */ false, /* shouldShowDeleteGroup= */ true);

        assertEquals(3, modelList.size());

        verify(mDataSharingService)
                .readGroup(eq(COLLABORATION_ID1), mReadGroupCallbackCaptor.capture());
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(null, PeopleGroupActionFailure.UNKNOWN);
        mReadGroupCallbackCaptor.getValue().onResult(outcome);

        assertEquals(3, modelList.size());

        testMenuCoordinator.destroy();
    }

    @Test
    public void testReadGroup_NullCoreAccountInfo() {
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(null);
        TestMenuCoordinator testMenuCoordinator =
                new TestMenuCoordinator(
                        mActivity,
                        mView,
                        mOnItemClickedCallback,
                        TAB_ID,
                        /* isIncognito= */ false,
                        /* shouldShowDeleteGroup= */ true,
                        mTabModel,
                        mIdentityManager,
                        mTabGroupSyncService,
                        mDataSharingService);
        ModelList modelList =
                testMenuCoordinator.buildMenuActionItems(
                        /* isIncognito= */ false, /* shouldShowDeleteGroup= */ true);

        assertEquals(3, modelList.size());

        verify(mDataSharingService)
                .readGroup(eq(COLLABORATION_ID1), mReadGroupCallbackCaptor.capture());
        GroupMember groupMember =
                new GroupMember(
                        GAIA_ID1,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.OWNER,
                        /* avatarUrl= */ null);
        GroupMember[] groupMemberArray = new GroupMember[] {groupMember};
        GroupData groupData =
                new GroupData(
                        COLLABORATION_ID1,
                        /* displayName= */ null,
                        groupMemberArray,
                        /* groupToken= */ null);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        mReadGroupCallbackCaptor.getValue().onResult(outcome);

        assertEquals(3, modelList.size());

        testMenuCoordinator.destroy();
    }

    @Test
    public void testReadGroup_NoMatchingMember() {
        TestMenuCoordinator testMenuCoordinator =
                new TestMenuCoordinator(
                        mActivity,
                        mView,
                        mOnItemClickedCallback,
                        TAB_ID,
                        /* isIncognito= */ false,
                        /* shouldShowDeleteGroup= */ true,
                        mTabModel,
                        mIdentityManager,
                        mTabGroupSyncService,
                        mDataSharingService);
        ModelList modelList =
                testMenuCoordinator.buildMenuActionItems(
                        /* isIncognito= */ false, /* shouldShowDeleteGroup= */ true);

        assertEquals(3, modelList.size());

        verify(mDataSharingService)
                .readGroup(eq(COLLABORATION_ID1), mReadGroupCallbackCaptor.capture());
        GroupMember groupMember =
                new GroupMember(
                        GAIA_ID2,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.OWNER,
                        /* avatarUrl= */ null);
        GroupMember[] groupMemberArray = new GroupMember[] {groupMember};
        GroupData groupData =
                new GroupData(
                        COLLABORATION_ID1,
                        /* displayName= */ null,
                        groupMemberArray,
                        /* groupToken= */ null);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        mReadGroupCallbackCaptor.getValue().onResult(outcome);

        assertEquals(3, modelList.size());

        testMenuCoordinator.destroy();
    }
}

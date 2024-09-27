// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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

import java.util.List;

/** Unit tests for {@link TabGridDialogMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
public class TabGridDialogMenuCoordinatorUnitTest {
    private static final int TAB_ID = 123;
    private static final String COLLABORATION_ID1 = "A";
    private static final String GAIA_ID1 = "Z";
    private static final String GAIA_ID2 = "Y";
    private static final String EMAIL = "fake@gmail.com";
    private static final Token TAB_GROUP_TOKEN = Token.createRandom();

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
    @Captor private ArgumentCaptor<ModelList> mModelListCaptor;

    private TabGridDialogMenuCoordinator mMenuCoordinator;
    private Activity mActivity;
    private View mView;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);

        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.getTabGroupId()).thenReturn(TAB_GROUP_TOKEN);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
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

        mMenuCoordinator =
                spy(
                        new TabGridDialogMenuCoordinator(
                                mOnItemClickedCallback,
                                () -> mTabModel,
                                () -> TAB_ID,
                                /* isTabGroupSyncEnabled= */ true,
                                mIdentityManager,
                                mTabGroupSyncService,
                                mDataSharingService));
    }

    private void onActivity(TestActivity activity) {
        mActivity = activity;
        mView = new View(activity);
    }

    @Test
    public void testBuildMenuItems_NoCollaborationData() {
        ModelList modelList = new ModelList();
        mMenuCoordinator.buildMenuActionItems(
                modelList,
                /* isIncognito= */ false,
                /* isTabGroupSyncEnabled= */ true,
                /* hasCollaborationData= */ false);

        List<Integer> menuIds =
                List.of(
                        R.id.select_tabs,
                        R.id.edit_group_name,
                        R.id.edit_group_color,
                        R.id.close_tab,
                        R.id.delete_tab);
        assertListMenuItemsAre(modelList, menuIds);
    }

    @Test
    public void testBuildMenuItems_HasCollaborationData() {
        ModelList modelList = new ModelList();
        mMenuCoordinator.buildMenuActionItems(
                modelList,
                /* isIncognito= */ false,
                /* isTabGroupSyncEnabled= */ true,
                /* hasCollaborationData= */ true);

        List<Integer> menuIds =
                List.of(
                        R.id.select_tabs,
                        R.id.edit_group_name,
                        R.id.edit_group_color,
                        R.id.close_tab);
        assertListMenuItemsAre(modelList, menuIds);
    }

    @Test
    public void testBuildMenuItems_Incognito() {
        ModelList modelList = new ModelList();
        mMenuCoordinator.buildMenuActionItems(
                modelList,
                /* isIncognito= */ true,
                /* isTabGroupSyncEnabled= */ true,
                /* hasCollaborationData= */ false);

        List<Integer> menuIds =
                List.of(
                        R.id.select_tabs,
                        R.id.edit_group_name,
                        R.id.edit_group_color,
                        R.id.close_tab);
        assertListMenuItemsAre(modelList, menuIds);
    }

    @Test
    public void testBuildMenuItems_NoDelete() {
        ModelList modelList = new ModelList();
        mMenuCoordinator.buildMenuActionItems(
                modelList,
                /* isIncognito= */ false,
                /* isTabGroupSyncEnabled= */ false,
                /* hasCollaborationData= */ false);

        List<Integer> menuIds =
                List.of(
                        R.id.select_tabs,
                        R.id.edit_group_name,
                        R.id.edit_group_color,
                        R.id.close_tab);
        assertListMenuItemsAre(modelList, menuIds);
    }

    @Test
    public void testBuildCollaborationMenuItems_Unknown() {
        GroupMember groupMember =
                new GroupMember(
                        GAIA_ID2,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.OWNER,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupMember[] groupMemberArray = new GroupMember[] {groupMember};
        GroupData groupData =
                new GroupData(
                        COLLABORATION_ID1,
                        /* displayName= */ null,
                        groupMemberArray,
                        /* groupToken= */ null);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);

        ModelList modelList = new ModelList();
        mMenuCoordinator.buildCollaborationMenuItems(modelList, mIdentityManager, outcome);

        assertEquals(0, modelList.size());
    }

    @Test
    public void testBuildAllItems_Member() {
        GroupMember groupMember1 =
                new GroupMember(
                        GAIA_ID1,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.MEMBER,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupMember groupMember2 =
                new GroupMember(
                        GAIA_ID2,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.OWNER,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupMember[] groupMemberArray = new GroupMember[] {groupMember1, groupMember2};
        GroupData groupData =
                new GroupData(
                        COLLABORATION_ID1,
                        /* displayName= */ null,
                        groupMemberArray,
                        /* groupToken= */ null);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);

        View.OnClickListener clickListener = mMenuCoordinator.getOnClickListener();
        clickListener.onClick(mView);

        verify(mDataSharingService)
                .readGroup(eq(COLLABORATION_ID1), mReadGroupCallbackCaptor.capture());
        mReadGroupCallbackCaptor.getValue().onResult(outcome);

        verify(mMenuCoordinator).buildMenuActionItems(any(), eq(false), eq(true), eq(true));
        verify(mMenuCoordinator)
                .buildCollaborationMenuItems(mModelListCaptor.capture(), any(), any());

        List<Integer> menuIds =
                List.of(
                        R.id.select_tabs,
                        R.id.edit_group_name,
                        R.id.edit_group_color,
                        R.id.manage_sharing,
                        R.id.recent_activity,
                        R.id.close_tab,
                        R.id.leave_group);
        assertListMenuItemsAre(mModelListCaptor.getValue(), menuIds);

        mMenuCoordinator.dismissForTesting();
    }

    @Test
    public void testBuildAllItems_Owner() {
        GroupMember groupMember =
                new GroupMember(
                        GAIA_ID1,
                        /* displayName= */ null,
                        EMAIL,
                        MemberRole.OWNER,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupMember[] groupMemberArray = new GroupMember[] {groupMember};
        GroupData groupData =
                new GroupData(
                        COLLABORATION_ID1,
                        /* displayName= */ null,
                        groupMemberArray,
                        /* groupToken= */ null);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);

        View.OnClickListener clickListener = mMenuCoordinator.getOnClickListener();
        clickListener.onClick(mView);

        verify(mDataSharingService)
                .readGroup(eq(COLLABORATION_ID1), mReadGroupCallbackCaptor.capture());
        mReadGroupCallbackCaptor.getValue().onResult(outcome);

        verify(mMenuCoordinator).buildMenuActionItems(any(), eq(false), eq(true), eq(true));
        verify(mMenuCoordinator)
                .buildCollaborationMenuItems(mModelListCaptor.capture(), any(), any());

        List<Integer> menuIds =
                List.of(
                        R.id.select_tabs,
                        R.id.edit_group_name,
                        R.id.edit_group_color,
                        R.id.manage_sharing,
                        R.id.recent_activity,
                        R.id.close_tab,
                        R.id.delete_shared_group);
        assertListMenuItemsAre(mModelListCaptor.getValue(), menuIds);

        mMenuCoordinator.dismissForTesting();
    }

    private void assertListMenuItemsAre(ModelList modelList, List<Integer> menuIds) {
        assertEquals(menuIds.size(), modelList.size());
        for (int i = 0; i < menuIds.size(); i++) {
            PropertyModel propertyModel = modelList.get(i).model;
            assertEquals(
                    "Unexpected id for item " + i,
                    (int) menuIds.get(i),
                    (int) propertyModel.get(ListMenuItemProperties.MENU_ITEM_ID));
        }
    }
}

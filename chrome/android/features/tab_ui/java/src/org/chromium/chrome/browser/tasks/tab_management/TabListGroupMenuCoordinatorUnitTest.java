// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Rect;
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

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator.OnItemClickedCallback;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.RectProvider;

import java.util.List;

/** Unit tests for {@link TabListGroupMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
public class TabListGroupMenuCoordinatorUnitTest {
    private static final int TAB_ID = 123;
    private static final String COLLABORATION_ID1 = "A";
    private static final Token TAB_GROUP_TOKEN = Token.createRandom();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private TabModel mTabModel;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ServiceStatus mServiceStatus;
    @Mock private OnItemClickedCallback<Token> mOnItemClickedCallback;

    @Captor private ArgumentCaptor<ModelList> mModelListCaptor;

    private TabListGroupMenuCoordinator mMenuCoordinator;
    private Activity mActivity;
    private View mView;
    private SavedTabGroup mSavedTabGroup;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);

        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.getTabGroupId()).thenReturn(TAB_GROUP_TOKEN);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);
        when(mServiceStatus.isAllowedToCreate()).thenReturn(false);

        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab);
        when(mTab.getTabGroupId()).thenReturn(TAB_GROUP_TOKEN);
        mSavedTabGroup = new SavedTabGroup();
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(mSavedTabGroup);

        mMenuCoordinator =
                spy(
                        new TabListGroupMenuCoordinator(
                                mOnItemClickedCallback,
                                () -> mTabModel,
                                mTabGroupSyncService,
                                mCollaborationService,
                                mActivity));
    }

    private void onActivity(TestActivity activity) {
        mActivity = activity;
        mView = new View(activity);
    }

    private void setCollaborationState(boolean enabled) {
        mSavedTabGroup.collaborationId = enabled ? COLLABORATION_ID1 : null;
    }

    @Test
    public void testBuildMenuItems_WithDelete() {
        ModelList modelList = new ModelList();
        when(mServiceStatus.isAllowedToJoin()).thenReturn(false);

        mMenuCoordinator.buildMenuActionItems(modelList, TAB_GROUP_TOKEN);

        List<Integer> menuIds =
                List.of(
                        R.id.close_tab_group,
                        R.id.edit_group_name,
                        R.id.ungroup_tab,
                        R.id.delete_tab_group);
        assertListMenuItemsAre(modelList, menuIds);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
    public void testBuildMenuItems_WithIcons() {
        ModelList modelList = new ModelList();
        when(mServiceStatus.isAllowedToJoin()).thenReturn(false);

        RectProvider viewRectProvider = mock();
        when(viewRectProvider.getRect()).thenReturn(new Rect());

        mMenuCoordinator.showMenu(viewRectProvider, TAB_GROUP_TOKEN, /* focusable= */ true);
        mMenuCoordinator.destroyMenuForTesting();
        mMenuCoordinator.buildMenuActionItems(modelList, TAB_GROUP_TOKEN);

        for (int i = 0; i < modelList.size(); i++) {
            PropertyModel propertyModel = modelList.get(i).model;
            assertNotEquals(
                    Resources.ID_NULL, propertyModel.get(ListMenuItemProperties.START_ICON_ID));
        }
    }

    @Test
    public void testBuildMenuItems_NoDelete() {
        setCollaborationState(true);
        ModelList modelList = new ModelList();
        mMenuCoordinator.buildMenuActionItems(modelList, TAB_GROUP_TOKEN);

        List<Integer> menuIds =
                List.of(R.id.close_tab_group, R.id.edit_group_name, R.id.share_group);
        assertListMenuItemsAre(modelList, menuIds);

        modelList = new ModelList();
        mMenuCoordinator.setTabGroupSyncServiceForTesting(null);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(false);
        mMenuCoordinator.buildMenuActionItems(modelList, TAB_GROUP_TOKEN);

        menuIds = List.of(R.id.close_tab_group, R.id.edit_group_name, R.id.ungroup_tab);
        assertListMenuItemsAre(modelList, menuIds);

        modelList = new ModelList();
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        mMenuCoordinator.setTabGroupSyncServiceForTesting(mTabGroupSyncService);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(false);
        mMenuCoordinator.buildMenuActionItems(modelList, TAB_GROUP_TOKEN);

        assertListMenuItemsAre(modelList, menuIds);
    }

    @Test
    public void testBuildMenuItems_Share() {
        setCollaborationState(true);
        ModelList modelList = new ModelList();
        when(mServiceStatus.isAllowedToCreate()).thenReturn(false);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(false);
        mMenuCoordinator.buildMenuActionItems(modelList, TAB_GROUP_TOKEN);

        // Eligible for all menu items except share.
        List<Integer> menuIds =
                List.of(
                        R.id.close_tab_group,
                        R.id.edit_group_name,
                        R.id.ungroup_tab,
                        R.id.delete_tab_group);
        assertListMenuItemsAre(modelList, menuIds);

        modelList = new ModelList();
        when(mServiceStatus.isAllowedToCreate()).thenReturn(true);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(false);
        mMenuCoordinator.buildMenuActionItems(modelList, TAB_GROUP_TOKEN);

        // Eligible for all menu items.
        menuIds =
                List.of(
                        R.id.close_tab_group,
                        R.id.edit_group_name,
                        R.id.ungroup_tab,
                        R.id.share_group,
                        R.id.delete_tab_group);
        assertListMenuItemsAre(modelList, menuIds);

        modelList = new ModelList();
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        mMenuCoordinator.buildMenuActionItems(modelList, TAB_GROUP_TOKEN);

        // Incognito so is not synced so not deletable or shareable.
        menuIds = List.of(R.id.close_tab_group, R.id.edit_group_name, R.id.ungroup_tab);
        assertListMenuItemsAre(modelList, menuIds);

        modelList = new ModelList();
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);
        mMenuCoordinator.buildMenuActionItems(modelList, TAB_GROUP_TOKEN);

        // Already shared and delete depends on collaboration service readback.
        menuIds = List.of(R.id.close_tab_group, R.id.edit_group_name, R.id.share_group);
        assertListMenuItemsAre(modelList, menuIds);
    }

    @Test
    public void testBuildCollaborationMenuItems_Unknown() {
        setCollaborationState(true);
        ModelList modelList = new ModelList();
        mMenuCoordinator.buildCollaborationMenuItems(modelList, MemberRole.UNKNOWN);

        assertEquals(0, modelList.size());
    }

    @Test
    public void testBuildAllItems_Member() {
        setCollaborationState(true);
        when(mCollaborationService.getCurrentUserRoleForGroup(COLLABORATION_ID1))
                .thenReturn(MemberRole.MEMBER);

        mMenuCoordinator.getTabActionListener().run(mView, TAB_ID, /* triggeringMotion= */ null);

        verify(mMenuCoordinator).buildMenuActionItems(any(), eq(TAB_GROUP_TOKEN));
        verify(mMenuCoordinator)
                .buildCollaborationMenuItems(mModelListCaptor.capture(), eq(MemberRole.MEMBER));

        List<Integer> menuIds =
                List.of(
                        R.id.close_tab_group,
                        R.id.edit_group_name,
                        R.id.share_group,
                        R.id.leave_group);
        assertListMenuItemsAre(mModelListCaptor.getValue(), menuIds);

        mMenuCoordinator.dismiss();
    }

    @Test
    public void testBuildAllItems_Owner() {
        setCollaborationState(true);
        when(mCollaborationService.getCurrentUserRoleForGroup(COLLABORATION_ID1))
                .thenReturn(MemberRole.OWNER);

        mMenuCoordinator.getTabActionListener().run(mView, TAB_ID, /* triggeringMotion= */ null);

        verify(mMenuCoordinator).buildMenuActionItems(any(), eq(TAB_GROUP_TOKEN));
        verify(mMenuCoordinator)
                .buildCollaborationMenuItems(mModelListCaptor.capture(), eq(MemberRole.OWNER));

        List<Integer> menuIds =
                List.of(
                        R.id.close_tab_group,
                        R.id.edit_group_name,
                        R.id.share_group,
                        R.id.delete_shared_group);
        assertListMenuItemsAre(mModelListCaptor.getValue(), menuIds);

        mMenuCoordinator.dismiss();
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

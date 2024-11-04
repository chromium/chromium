// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;

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
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupOverflowMenuCoordinator.OnItemClickedCallback;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.BasicListMenu.ListMenuItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link TabGroupContextMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.TAB_STRIP_GROUP_CONTEXT_MENU})
public class TabGroupContextMenuCoordinatorUnitTest {
    private static final String GAIA_ID = "Z";
    private static final String EMAIL = "fake@gmail.com";
    private static final String COLLABORATION_ID = "A";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private TabGroupContextMenuCoordinator mTabGroupContextMenuCoordinator;
    private OnItemClickedCallback mOnItemClickedCallback;
    private int mTabId;
    private MockTabModel mTabModel;
    @Mock private View mMenuView;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private Profile mProfile;
    @Mock private ActionConfirmationManager mActionConfirmationManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabCreator mTabCreator;
    @Captor private ArgumentCaptor<Callback<Integer>> mActionConfirmationResultCaptor;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingTabManager mDataSharingTabManager;
    @Mock private WeakReference<Activity> mWeakReferenceActivity;
    @Mock private DataSharingService mDataSharingService;
    @Mock private IdentityManager mIdentityManager;
    @Mock private GroupDataOrFailureOutcome mGroupDataOrFailureOutcome;
    @Mock private Callback<Boolean> mCallback;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        LayoutInflater inflater = LayoutInflater.from(mActivity);
        mMenuView = inflater.inflate(R.layout.tab_strip_group_menu_layout, null);
        when(mWindowAndroid.getKeyboardDelegate()).thenReturn(mKeyboardVisibilityDelegate);
        when(mWindowAndroid.getActivity()).thenReturn(mWeakReferenceActivity);
        when(mWeakReferenceActivity.get()).thenReturn(mActivity);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabModel.isIncognito()).thenReturn(false);
        when(mProfile.isOffTheRecord()).thenReturn(true);
        mTabId = 1;
        mOnItemClickedCallback =
                TabGroupContextMenuCoordinator.getMenuItemClickedCallback(
                        mActivity,
                        mTabGroupModelFilter,
                        mActionConfirmationManager,
                        mModalDialogManager,
                        mTabCreator,
                        mDataSharingTabManager,
                        mCallback,
                        /* isTabGroupSyncEnabled= */ true);
        mTabGroupContextMenuCoordinator =
                TabGroupContextMenuCoordinator.createContextMenuCoordinator(
                        mTabModel,
                        mTabGroupModelFilter,
                        mActionConfirmationManager,
                        mModalDialogManager,
                        mTabCreator,
                        mWindowAndroid,
                        mDataSharingTabManager,
                        mCallback);

        // Set groupRootId to bypass showMenu() call.
        mTabGroupContextMenuCoordinator.setGroupRootIdForTesting(mTabId);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    @Feature("Tab Strip Group Context Menu")
    public void testListMenuItems() {
        // Build custom view first to setup menu view.
        mTabGroupContextMenuCoordinator.buildCustomView(mMenuView, /* isIncognito= */ false);

        ModelList modelList = new ModelList();
        mTabGroupContextMenuCoordinator.buildMenuActionItems(
                modelList,
                /* isIncognito= */ false,
                /* shouldShowDeleteGroup= */ true,
                /* hasCollaborationData= */ false);

        // Assert: verify number of items in the model list.
        assertEquals("Number of items in the list menu is incorrect", 7, modelList.size());

        // Assert: verify divider and normal menu items.
        verifyNormalListItems(modelList, 4);

        // Assert: verify share group menu item.
        assertEquals(
                R.id.share_group, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // Assert: verify divider and delete group menu item.
        assertEquals(ListMenuItemType.DIVIDER, modelList.get(5).type);
        assertEquals(
                R.id.delete_tab_group,
                modelList.get(6).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testListMenuItems_Incognito() {
        // Build custom view first to setup menu view.
        mTabGroupContextMenuCoordinator.buildCustomView(mMenuView, /* isIncognito= */ false);

        ModelList modelList = new ModelList();
        mTabGroupContextMenuCoordinator.buildMenuActionItems(
                modelList,
                /* isIncognito= */ true,
                /* shouldShowDeleteGroup= */ false,
                /* hasCollaborationData= */ false);

        // Assert: verify number of items in the model list.
        assertEquals("Number of items in the list menu is incorrect", 4, modelList.size());

        // Assert: verify normal menu items.
        verifyNormalListItems(modelList, 3);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DATA_SHARING)
    @Feature("Tab Strip Group Context Menu")
    public void testListMenuItems_DataShareDisabled() {
        // Build custom view first to setup menu view.
        mTabGroupContextMenuCoordinator.buildCustomView(mMenuView, /* isIncognito= */ false);

        ModelList modelList = new ModelList();
        mTabGroupContextMenuCoordinator.buildMenuActionItems(
                modelList,
                /* isIncognito= */ false,
                /* shouldShowDeleteGroup= */ true,
                /* hasCollaborationData= */ false);

        // Assert: verify number of items in the model list.
        assertEquals("Number of items in the list menu is incorrect", 6, modelList.size());

        // Assert: verify share group option does not show.
        for (int i = 0; i < modelList.size(); i++) {
            if (modelList.get(i).model.containsKey(ListMenuItemProperties.MENU_ITEM_ID)) {
                assertNotEquals(
                        R.id.share_group,
                        modelList.get(i).model.get(ListMenuItemProperties.MENU_ITEM_ID));
            }
        }
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testCustomMenuItems() {
        // Build custom view.
        mTabGroupContextMenuCoordinator.buildCustomView(mMenuView, /* isIncognito= */ false);

        // Verify text input layout.
        EditText groupTitleEditText =
                mTabGroupContextMenuCoordinator.getGroupTitleEditTextForTesting();
        assertNotNull(groupTitleEditText);

        // Verify color picker.
        ColorPickerCoordinator colorPickerCoordinator =
                mTabGroupContextMenuCoordinator.getColorPickerCoordinatorForTesting();
        assertNotNull(colorPickerCoordinator);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    @Feature("Tab Strip Group Context Menu")
    public void testCollaborationMenuItems_Owner() {
        ModelList modelList = new ModelList();

        // Build regular menu views.
        mTabGroupContextMenuCoordinator.buildCustomView(mMenuView, /* isIncognito= */ false);
        mTabGroupContextMenuCoordinator.buildMenuActionItems(
                modelList,
                /* isIncognito= */ false,
                /* shouldShowDeleteGroup= */ true,
                /* hasCollaborationData= */ true);

        // Build collaboration view.
        GroupDataOrFailureOutcome outcome = setUpSharedGroup(MemberRole.OWNER);
        mTabGroupContextMenuCoordinator.buildCollaborationMenuItems(
                modelList, mIdentityManager, outcome);

        // Assert: verify number of items in the model list.
        assertEquals("Number of items in the list menu is incorrect", 8, modelList.size());

        // Assert: verify normal menu items.
        verifyNormalListItems(modelList, 5);

        // Assert: verify collaboration menu items.
        verifyCollaborationListItems(modelList, MemberRole.OWNER);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    @Feature("Tab Strip Group Context Menu")
    public void testCollaborationMenuItems_Member() {
        ModelList modelList = new ModelList();

        // Build regular menu views.
        mTabGroupContextMenuCoordinator.buildCustomView(mMenuView, /* isIncognito= */ false);
        mTabGroupContextMenuCoordinator.buildMenuActionItems(
                modelList,
                /* isIncognito= */ false,
                /* shouldShowDeleteGroup= */ true,
                /* hasCollaborationData= */ true);

        // Build collaboration view.
        GroupDataOrFailureOutcome outcome = setUpSharedGroup(MemberRole.MEMBER);
        mTabGroupContextMenuCoordinator.buildCollaborationMenuItems(
                modelList, mIdentityManager, outcome);

        // Assert: verify number of items in the model list.
        assertEquals("Number of items in the list menu is incorrect", 8, modelList.size());

        // Assert: verify normal menu items.
        verifyNormalListItems(modelList, 5);

        // Assert: verify collaboration menu items.
        verifyCollaborationListItems(modelList, MemberRole.MEMBER);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuItemClicked_Ungroup() {
        // Initialize.
        setUpTabGroupModelFilter();

        // Verify tab group is ungrouped.
        mOnItemClickedCallback.onClick(R.id.ungroup_tab, mTabId, /* collaborationId= */ null);
        verify(mActionConfirmationManager)
                .processUngroupAttempt(mActionConfirmationResultCaptor.capture());
        mActionConfirmationResultCaptor
                .getValue()
                .onResult(ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mTabGroupModelFilter).moveTabOutOfGroupInDirection(mTabId, /* trailing= */ true);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testItemClicked_CloseGroup() {
        // Initialize.
        List<Tab> tabsInGroup = setUpTabGroupModelFilter();

        // Verify tab group closed.
        mOnItemClickedCallback.onClick(R.id.close_tab_group, mTabId, /* collaborationId= */ null);
        verify(mTabGroupModelFilter)
                .closeTabs(
                        argThat(
                                params ->
                                        params.tabs.get(0) == tabsInGroup.get(0)
                                                && params.allowUndo
                                                && params.hideTabGroups));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuItemClicked_DeleteGroup() {
        // Initialize.
        List<Tab> tabsInGroup = setUpTabGroupModelFilter();

        // Verify tab group deleted.
        mOnItemClickedCallback.onClick(R.id.delete_tab_group, mTabId, /* collaborationId= */ null);
        verify(mActionConfirmationManager)
                .processDeleteGroupAttempt(mActionConfirmationResultCaptor.capture());
        mActionConfirmationResultCaptor
                .getValue()
                .onResult(ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mTabGroupModelFilter)
                .closeTabs(
                        argThat(
                                params ->
                                        params.tabs.get(0) == tabsInGroup.get(0)
                                                && !params.allowUndo
                                                && !params.hideTabGroups));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuItemClicked_NewTabInGroup() {
        // Initialize.
        List<Tab> tabsInGroup = setUpTabGroupModelFilter();

        // Verify new tab opened in group.
        mOnItemClickedCallback.onClick(
                R.id.open_new_tab_in_group, mTabId, /* collaborationId= */ null);
        verify(mTabCreator)
                .createNewTab(any(), eq(TabLaunchType.FROM_TAB_GROUP_UI), eq(tabsInGroup.get(0)));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testUpdateGroupTitleOnKeyboardHide() {
        // Initialize
        setUpTabGroupModelFilter();
        mTabGroupContextMenuCoordinator.buildCustomView(mMenuView, /* isIncognito= */ false);

        // Verify default group title.
        EditText groupTitleEditText =
                mTabGroupContextMenuCoordinator.getGroupTitleEditTextForTesting();
        assertEquals("1 tab", groupTitleEditText.getText().toString());

        // Update group title by flipping keyboard visibility to hide.
        String newTitle = "new title";
        groupTitleEditText.setText(newTitle);
        KeyboardVisibilityDelegate.KeyboardVisibilityListener keyboardVisibilityListener =
                mTabGroupContextMenuCoordinator.getKeyboardVisibilityListenerForTesting();
        keyboardVisibilityListener.keyboardVisibilityChanged(false);

        // Verify the group title is updated.
        verify(mTabGroupModelFilter).setTabGroupTitle(mTabId, newTitle);

        // Remove the custom title set by the user by clearing the edit box.
        newTitle = "";
        groupTitleEditText.setText(newTitle);
        keyboardVisibilityListener.keyboardVisibilityChanged(false);

        // Verify the previous title is deleted and is default to "N tabs"
        verify(mTabGroupModelFilter).deleteTabGroupTitle(mTabId);
        assertEquals("1 tab", groupTitleEditText.getText().toString());
    }

    private List<Tab> setUpTabGroupModelFilter() {
        mTabModel.addTab(mTabId);
        Tab tab = mTabModel.getTabById(mTabId);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.isTabInTabGroup(tab)).thenReturn(true);
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(eq(mTabId))).thenReturn(1);
        List<Tab> tabsInGroup = Arrays.asList(tab);
        when(mTabGroupModelFilter.getRelatedTabListForRootId(eq(mTabId))).thenReturn(tabsInGroup);
        when(mTabGroupModelFilter.getRelatedTabList(eq(mTabId))).thenReturn(tabsInGroup);
        return tabsInGroup;
    }

    private void verifyNormalListItems(ModelList modelList, int closeGroupPosition) {
        assertEquals(ListMenuItemType.DIVIDER, modelList.get(0).type);
        assertEquals(
                R.id.open_new_tab_in_group,
                modelList.get(1).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                R.id.ungroup_tab, modelList.get(2).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        assertEquals(
                R.id.close_tab_group,
                modelList.get(closeGroupPosition).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    private void verifyCollaborationListItems(ModelList modelList, @MemberRole int memberRole) {
        assertEquals(
                R.id.manage_sharing,
                modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                R.id.recent_activity,
                modelList.get(4).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(ListMenuItemType.DIVIDER, modelList.get(6).type);

        // Verify delete group or leave group depending on the member role.
        if (memberRole == MemberRole.OWNER) {
            assertEquals(
                    R.id.delete_shared_group,
                    modelList.get(7).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        } else if (memberRole == MemberRole.MEMBER) {
            assertEquals(
                    R.id.leave_group,
                    modelList.get(7).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        }
    }

    private GroupDataOrFailureOutcome setUpSharedGroup(@MemberRole int memberRole) {
        GroupMember groupMember =
                new GroupMember(
                        GAIA_ID,
                        /* displayName= */ null,
                        EMAIL,
                        memberRole,
                        /* avatarUrl= */ null,
                        /* givenName= */ null);
        GroupMember[] groupMemberArray = new GroupMember[] {groupMember};
        GroupData groupData =
                new GroupData(
                        COLLABORATION_ID,
                        /* displayName= */ null,
                        groupMemberArray,
                        /* groupToken= */ null);
        GroupDataOrFailureOutcome outcome =
                new GroupDataOrFailureOutcome(groupData, PeopleGroupActionFailure.UNKNOWN);
        CoreAccountInfo coreAccountInfo = CoreAccountInfo.createFromEmailAndGaiaId(EMAIL, GAIA_ID);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(coreAccountInfo);
        return outcome;
    }
}

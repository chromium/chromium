// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP;
import static org.chromium.chrome.browser.multiwindow.InstanceInfo.Type.CURRENT;
import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType.ACTIVE;

import android.app.Activity;
import android.os.SystemClock;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.EditText;

import androidx.annotation.IdRes;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator.OnItemClickedCallback;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.util.motion.MotionEventTestUtils;
import org.chromium.components.browser_ui.widget.list_view.FakeListViewTouchTracker;
import org.chromium.components.browser_ui.widget.list_view.ListViewTouchTracker;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListSectionDividerProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.RectProvider;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.List;
import java.util.function.BiConsumer;

/** Unit tests for {@link TabGroupContextMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.DATA_SHARING})
public class TabGroupContextMenuCoordinatorUnitTest {
    private static final int TAB_ID = 1;
    private static final Token TAB_GROUP_ID = new Token(3L, 4L);
    private static final String COLLABORATION_ID = "CollaborationId";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private TabGroupContextMenuCoordinator mTabGroupContextMenuCoordinator;
    private OnItemClickedCallback<Token> mOnItemClickedCallback;
    private MockTabModel mTabModel;
    private View mMenuView;
    private final SavedTabGroup mSavedTabGroup = new SavedTabGroup();

    // Tab state
    @Mock private TabRemover mTabRemover;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private TabCreator mTabCreator;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;

    // Share state
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ServiceStatus mServiceStatus;
    @Mock private DataSharingTabManager mDataSharingTabManager;

    // Window state
    private static final GURL EXAMPLE_URL = new GURL("https://example.com");
    private static final int INSTANCE_ID_1 = 5;
    private static final int INSTANCE_ID_2 = 6;
    private static final String WINDOW_TITLE_1 = "Window Title 1";
    private static final String WINDOW_TITLE_2 = "Window Title 2";
    private static final int TASK_ID = 7;
    private static final int NUM_TABS = 1;
    private static final int NUM_INCOGNITO_TABS = 0;
    private static final long LAST_ACCESSED_TIME = 100L;
    private static final InstanceInfo INSTANCE_INFO_1 =
            new InstanceInfo(
                    INSTANCE_ID_1,
                    TASK_ID,
                    CURRENT,
                    EXAMPLE_URL.toString(),
                    WINDOW_TITLE_1,
                    /* customTitle= */ null,
                    NUM_TABS,
                    NUM_INCOGNITO_TABS,
                    /* isIncognitoSelected= */ false,
                    LAST_ACCESSED_TIME,
                    /* closedByUser= */ false);

    private static final InstanceInfo INSTANCE_INFO_2 =
            new InstanceInfo(
                    INSTANCE_ID_2,
                    TASK_ID,
                    CURRENT,
                    EXAMPLE_URL.toString(),
                    WINDOW_TITLE_2,
                    /* customTitle= */ null,
                    NUM_TABS,
                    NUM_INCOGNITO_TABS,
                    /* isIncognitoSelected= */ false,
                    LAST_ACCESSED_TIME,
                    /* closedByUser= */ false);

    // Other dependencies
    @Mock private Profile mProfile;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private WeakReference<Activity> mWeakReferenceActivity;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private BiConsumer<Token, Boolean> mReorderFunction;
    private Activity mActivity;

    @Before
    public void setUp() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        when(mServiceStatus.isAllowedToCreate()).thenReturn(true);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);

        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity = activity;
        activity.setTheme(R.style.Theme_Chromium_Activity);
        LayoutInflater inflater = LayoutInflater.from(activity);
        mMenuView = inflater.inflate(R.layout.tab_strip_group_menu_layout, null);
        when(mWindowAndroid.getKeyboardDelegate()).thenReturn(mKeyboardVisibilityDelegate);
        when(mWindowAndroid.getActivity()).thenReturn(mWeakReferenceActivity);
        when(mWeakReferenceActivity.get()).thenReturn(activity);
        mTabModel = spy(new MockTabModel(mProfile, null));
        mTabModel.addTab(0);
        mTabModel.setIndex(0, TabSelectionType.FROM_NEW);
        when(mTabModel.isIncognito()).thenReturn(false);
        mTabModel.setTabRemoverForTesting(mTabRemover);
        mTabModel.setTabCreatorForTesting(mTabCreator);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mProfile.isOffTheRecord()).thenReturn(true);
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(INSTANCE_ID_1);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2));
        mSavedTabGroup.collaborationId = COLLABORATION_ID;
        mOnItemClickedCallback =
                TabGroupContextMenuCoordinator.getMenuItemClickedCallback(
                        activity,
                        () -> mTabModel,
                        mTabGroupModelFilter,
                        mMultiInstanceManager,
                        mDataSharingTabManager);
        mTabGroupContextMenuCoordinator =
                TabGroupContextMenuCoordinator.createContextMenuCoordinator(
                        mTabModel,
                        mTabGroupModelFilter,
                        mMultiInstanceManager,
                        mWindowAndroid,
                        mDataSharingTabManager,
                        mReorderFunction);

        // Set group ids manually to bypass showMenu() call.
        mTabGroupContextMenuCoordinator.setGroupDataForTesting(TAB_GROUP_ID);
    }

    @After
    public void tearDown() {
        mTabGroupContextMenuCoordinator.destroyMenuForTesting();
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems() {
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        mTabGroupContextMenuCoordinator.setTabGroupSyncServiceForTesting(mTabGroupSyncService);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(null);

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();

        // Assert: verify number of items in the model list.
        assertEquals("Number of items in the list menu is incorrect", 8, modelList.size());

        // Assert: verify divider and normal menu items.
        verifyNormalListItems(modelList, 4);

        // Assert: verify share group menu item.
        assertEquals(
                R.id.share_group, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // Assert: verify divider and delete group menu item.
        verifyDivider(modelList.get(6));
        assertEquals(
                R.id.delete_tab_group,
                modelList.get(7).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testListMenuItems_Incognito() {
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();

        // Assert: verify number of items in the model list.
        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // Assert: verify normal menu items.
        verifyNormalListItems(modelList, 3);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DATA_SHARING)
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    @Feature("Tab Strip Group Context Menu")
    public void testListMenuItems_DataShareDisabled() {
        when(mServiceStatus.isAllowedToCreate()).thenReturn(false);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        mTabGroupContextMenuCoordinator.setTabGroupSyncServiceForTesting(mTabGroupSyncService);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(null);

        // Build custom view first to setup menu view.
        mTabGroupContextMenuCoordinator.buildCustomView(mMenuView, /* isIncognito= */ false);
        ModelList modelList = new ModelList();
        mTabGroupContextMenuCoordinator.buildMenuActionItems(modelList, TAB_GROUP_ID);

        // Assert: verify number of items in the model list.
        assertEquals("Number of items in the list menu is incorrect", 7, modelList.size());

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
    @DisableFeatures(ChromeFeatureList.DATA_SHARING)
    @Feature("Tab Strip Group Context Menu")
    public void testListMenuItems_belowApi31() {
        // Build custom view first to setup menu view.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        mTabGroupContextMenuCoordinator.buildCustomView(mMenuView, /* isIncognito= */ false);
        ModelList modelList = new ModelList();
        mTabGroupContextMenuCoordinator.buildMenuActionItems(modelList, TAB_GROUP_ID);

        // Assert: verify move group option does not show.
        for (int i = 0; i < modelList.size(); i++) {
            if (modelList.get(i).model.containsKey(ListMenuItemProperties.MENU_ITEM_ID)) {
                assertNotEquals(
                        R.id.move_to_other_window_menu_id,
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
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testCollaborationMenuItems_Owner() {
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        mTabGroupContextMenuCoordinator.setTabGroupSyncServiceForTesting(mTabGroupSyncService);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(mSavedTabGroup);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);
        when(mCollaborationService.getCurrentUserRoleForGroup(COLLABORATION_ID))
                .thenReturn(MemberRole.OWNER);
        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();

        // Assert: verify number of items in the model list.
        assertEquals("Number of items in the list menu is incorrect", 8, modelList.size());

        // Assert: verify collaboration menu items; shared group should not have the option to
        // ungroup.
        verifyCollaborationListItems(modelList, MemberRole.OWNER);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testCollaborationMenuItems_Member() {
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        mTabGroupContextMenuCoordinator.setTabGroupSyncServiceForTesting(mTabGroupSyncService);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(mSavedTabGroup);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);
        when(mCollaborationService.getCurrentUserRoleForGroup(COLLABORATION_ID))
                .thenReturn(MemberRole.MEMBER);
        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();

        // Assert: verify number of items in the model list.
        assertEquals("Number of items in the list menu is incorrect", 8, modelList.size());

        // Assert: verify collaboration menu items; shared group should not have the option to
        // ungroup.
        verifyCollaborationListItems(modelList, MemberRole.MEMBER);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuItemClicked_Ungroup() {
        // Initialize.
        setUpTabGroupModelFilter();

        // Verify tab group is ungrouped.
        mOnItemClickedCallback.onClick(
                R.id.ungroup_tab,
                TAB_GROUP_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabUngrouper)
                .ungroupTabGroup(TAB_GROUP_ID, /* trailing= */ false, /* allowDialog= */ true);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuItemClicked_CloseGroup_NullListViewTouchTracker() {
        testItemClicked_CloseOrDeleteGroup(
                R.id.close_tab_group,
                /* listViewTouchTracker= */ null,
                /* shouldAllowUndo= */ true,
                /* shouldHideTabGroups= */ true);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuItemClicked_CloseGroup_ClickWithTouch() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker listViewTouchTracker = new FakeListViewTouchTracker();
        listViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createTouchMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        testItemClicked_CloseOrDeleteGroup(
                R.id.close_tab_group,
                listViewTouchTracker,
                /* shouldAllowUndo= */ true,
                /* shouldHideTabGroups= */ true);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuItemClicked_CloseGroup_ClickWithMouse() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker listViewTouchTracker = new FakeListViewTouchTracker();
        listViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createMouseMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        testItemClicked_CloseOrDeleteGroup(
                R.id.close_tab_group,
                listViewTouchTracker,
                /* shouldAllowUndo= */ false,
                /* shouldHideTabGroups= */ true);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuItemClicked_DeleteGroup_NullListViewTouchTracker() {
        testItemClicked_CloseOrDeleteGroup(
                R.id.delete_tab_group,
                /* listViewTouchTracker= */ null,
                /* shouldAllowUndo= */ true,
                /* shouldHideTabGroups= */ false);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuItemClicked_DeleteGroup_ClickWithTouch() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker listViewTouchTracker = new FakeListViewTouchTracker();
        listViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createTouchMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        testItemClicked_CloseOrDeleteGroup(
                R.id.delete_tab_group,
                listViewTouchTracker,
                /* shouldAllowUndo= */ true,
                /* shouldHideTabGroups= */ false);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuItemClicked_DeleteGroup_ClickWithMouse() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker listViewTouchTracker = new FakeListViewTouchTracker();
        listViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createMouseMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        testItemClicked_CloseOrDeleteGroup(
                R.id.delete_tab_group,
                listViewTouchTracker,
                /* shouldAllowUndo= */ false,
                /* shouldHideTabGroups= */ false);
    }

    private void testItemClicked_CloseOrDeleteGroup(
            @IdRes int menuId,
            @Nullable ListViewTouchTracker listViewTouchTracker,
            boolean shouldAllowUndo,
            boolean shouldHideTabGroups) {
        assertTrue(menuId == R.id.close_tab_group || menuId == R.id.delete_tab_group);

        // Initialize.
        List<Tab> tabsInGroup = setUpTabGroupModelFilter();

        // Verify tab group closed.
        mOnItemClickedCallback.onClick(
                menuId, TAB_GROUP_ID, /* collaborationId= */ null, listViewTouchTracker);
        verify(mTabRemover)
                .closeTabs(
                        argThat(
                                params ->
                                        params.tabs.get(0) == tabsInGroup.get(0)
                                                && (params.allowUndo == shouldAllowUndo)
                                                && (params.hideTabGroups == shouldHideTabGroups)),
                        /* allowDialog= */ eq(true),
                        /* listener= */ any());
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuItemClicked_NewTabInGroup() {
        // Initialize.
        List<Tab> tabsInGroup = setUpTabGroupModelFilter();

        // Verify new tab opened in group.
        mOnItemClickedCallback.onClick(
                R.id.open_new_tab_in_group,
                TAB_GROUP_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabCreator)
                .createNewTab(any(), eq(TabLaunchType.FROM_TAB_GROUP_UI), eq(tabsInGroup.get(0)));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuItemClicked_MoveGroup() {
        // Initialize.
        setUpTabGroupModelFilter();

        // Fake a click on the move group action.
        mOnItemClickedCallback.onClick(
                R.id.move_to_other_window_menu_id,
                TAB_GROUP_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);

        // Verify.
        verify(mMultiInstanceManager)
                .moveTabGroupToOtherWindow(
                        any(TabGroupMetadata.class), eq(NewWindowAppSource.MENU));
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
        verify(mTabGroupModelFilter).setTabGroupTitle(eq(TAB_GROUP_ID), eq(newTitle));

        // Remove the custom title set by the user by clearing the edit box.
        newTitle = "";
        groupTitleEditText.setText(newTitle);
        keyboardVisibilityListener.keyboardVisibilityChanged(false);

        // Verify the previous title is deleted and is default to "N tabs"
        verify(mTabGroupModelFilter).deleteTabGroupTitle(TAB_GROUP_ID);
        assertEquals("1 tab", groupTitleEditText.getText().toString());
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveToNewWindow() {
        setUpTabGroupModelFilter();
        MultiWindowUtils.setInstanceCountForTesting(1);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE)).thenReturn(List.of(INSTANCE_INFO_1));
        var modelList = new ModelList();
        mTabGroupContextMenuCoordinator.configureMenuItemsForTesting(modelList, TAB_GROUP_ID);

        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                4,
                R.plurals.move_group_to_another_window_context_menu_item,
                List.of(),
                mActivity);

        StripLayoutContextMenuCoordinatorTestUtils.clickMoveToNewWindow(modelList, 4, mMenuView);

        verify(mMultiInstanceManager, times(1))
                .moveTabGroupToNewWindow(any(TabGroupMetadata.class), eq(NewWindowAppSource.MENU));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveToWindow() {
        setUpTabGroupModelFilter();
        MultiWindowUtils.setInstanceCountForTesting(2);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2));
        var modelList = new ModelList();
        mTabGroupContextMenuCoordinator.configureMenuItemsForTesting(modelList, TAB_GROUP_ID);

        StripLayoutContextMenuCoordinatorTestUtils.clickMoveToWindowRow(
                modelList, 4, WINDOW_TITLE_2, mMenuView);

        verify(mMultiInstanceManager, times(1))
                .moveTabGroupToWindow(
                        eq(INSTANCE_INFO_2),
                        any(TabGroupMetadata.class),
                        eq(TabList.INVALID_TAB_INDEX),
                        eq(NewWindowAppSource.MENU));
    }

    private List<Tab> setUpTabGroupModelFilter() {
        Tab tab = mTabModel.addTab(TAB_ID);
        tab.setTabGroupId(TAB_GROUP_ID);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.getTabUngrouper()).thenReturn(mTabUngrouper);
        when(mTabGroupModelFilter.isTabInTabGroup(tab)).thenReturn(true);
        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabGroupModelFilter.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB_ID);
        when(mTabGroupModelFilter.getTabCountForGroup(eq(TAB_GROUP_ID))).thenReturn(1);
        List<Tab> tabsInGroup = Arrays.asList(tab);
        when(mTabGroupModelFilter.getTabsInGroup(eq(TAB_GROUP_ID))).thenReturn(tabsInGroup);
        when(mTabGroupModelFilter.getRelatedTabList(eq(TAB_ID))).thenReturn(tabsInGroup);
        return tabsInGroup;
    }

    @SuppressWarnings("DirectInvocationOnMock")
    private void verifyNormalListItems(ModelList modelList, int closeGroupPosition) {
        verifyDivider(modelList.get(0));
        assertEquals(
                R.id.open_new_tab_in_group,
                modelList.get(1).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                R.id.ungroup_tab, modelList.get(2).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                R.id.close_tab_group,
                modelList.get(closeGroupPosition).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                closeGroupPosition + 1,
                R.plurals.move_group_to_another_window_context_menu_item,
                List.of(WINDOW_TITLE_2),
                mActivity);
    }

    @SuppressWarnings("DirectInvocationOnMock")
    private void verifyCollaborationListItems(ModelList modelList, @MemberRole int memberRole) {
        verifyDivider(modelList.get(0));
        assertEquals(
                R.id.open_new_tab_in_group,
                modelList.get(1).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(0, modelList.get(1).model.get(ListMenuItemProperties.START_ICON_ID));
        assertEquals(
                R.id.manage_sharing,
                modelList.get(2).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(0, modelList.get(2).model.get(ListMenuItemProperties.START_ICON_ID));
        assertEquals(
                R.id.recent_activity,
                modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(0, modelList.get(3).model.get(ListMenuItemProperties.START_ICON_ID));
        assertEquals(
                R.id.close_tab_group,
                modelList.get(4).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(0, modelList.get(4).model.get(ListMenuItemProperties.START_ICON_ID));
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                5,
                R.plurals.move_group_to_another_window_context_menu_item,
                List.of(WINDOW_TITLE_2),
                mActivity);
        verifyDivider(modelList.get(6));

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
        assertEquals(0, modelList.get(7).model.get(ListMenuItemProperties.START_ICON_ID));
    }

    private void verifyDivider(ListItem item) {
        assertEquals(ListItemType.DIVIDER, item.type);
        assertEquals(
                "Expected divider item to not have customization",
                0,
                item.model.get(ListSectionDividerProperties.LEFT_PADDING_DIMEN_ID));
        assertEquals(
                "Expected divider item to not have customization",
                0,
                item.model.get(ListSectionDividerProperties.RIGHT_PADDING_DIMEN_ID));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testListMenuItems_moveGroupItems_accessibilityOn() {
        setUpReorderingMocks();

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();

        // 8 original items + 2 new items = 10
        assertEquals("Number of items in the list menu is incorrect", 8, modelList.size());

        ListItem moveLeftItem = modelList.get(6);
        assertEquals(
                "Move toward start item has wrong title",
                mActivity.getString(R.string.move_tab_group_left),
                moveLeftItem.model.get(ListMenuItemProperties.TITLE));

        ListItem moveRightItem = modelList.get(7);
        assertEquals(
                "Move toward end item has wrong title",
                mActivity.getString(R.string.move_tab_group_right),
                moveRightItem.model.get(ListMenuItemProperties.TITLE));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveGroupLeft() {
        setUpReorderingMocks();

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();
        ListItem moveLeftItem = modelList.get(6);
        moveLeftItem.model.get(ListMenuItemProperties.CLICK_LISTENER).onClick(mMenuView);

        verify(mReorderFunction).accept(TAB_GROUP_ID, true);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveGroupRight() {
        setUpReorderingMocks();

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();
        ListItem moveRightItem = modelList.get(7);
        moveRightItem.model.get(ListMenuItemProperties.CLICK_LISTENER).onClick(mMenuView);

        verify(mReorderFunction).accept(TAB_GROUP_ID, false);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testListMenuItems_moveGroupItems_accessibilityOn_RTL() {
        LocalizationUtils.setRtlForTesting(true);
        setUpReorderingMocks();

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();

        assertEquals("Number of items in the list menu is incorrect", 8, modelList.size());

        ListItem moveStartItem = modelList.get(6);
        assertEquals(
                "Move toward start item has wrong title",
                mActivity.getString(R.string.move_tab_group_right),
                moveStartItem.model.get(ListMenuItemProperties.TITLE));

        ListItem moveEndItem = modelList.get(7);
        assertEquals(
                "Move toward start item has wrong title",
                mActivity.getString(R.string.move_tab_group_left),
                moveEndItem.model.get(ListMenuItemProperties.TITLE));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveGroupLeft_RTL() {
        LocalizationUtils.setRtlForTesting(true);
        setUpReorderingMocks();

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();
        ListItem moveLeftItem = modelList.get(6);
        moveLeftItem.model.get(ListMenuItemProperties.CLICK_LISTENER).onClick(mMenuView);

        verify(mReorderFunction).accept(TAB_GROUP_ID, false);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveGroupRight_RTL() {
        LocalizationUtils.setRtlForTesting(true);
        setUpReorderingMocks();

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();
        ListItem moveRightItem = modelList.get(7);
        moveRightItem.model.get(ListMenuItemProperties.CLICK_LISTENER).onClick(mMenuView);

        verify(mReorderFunction).accept(TAB_GROUP_ID, true);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveGroupLeft_firstGroup() {
        mTabGroupContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        List<Tab> tabsInGroup = setUpReorderingMocks();
        when(mTabModel.indexOf(tabsInGroup.get(0))).thenReturn(0);

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();

        for (ListItem listItem : modelList) {
            if (!listItem.model.containsKey(ListMenuItemProperties.TITLE)) continue;
            assertNotEquals(
                    "Did not expect any item to have 'Move group left' title",
                    mActivity.getString(R.string.move_tab_group_left),
                    listItem.model.get(ListMenuItemProperties.TITLE));
        }
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveGroupLeft_firstGroup_RTL() {
        LocalizationUtils.setRtlForTesting(true);
        List<Tab> tabsInGroup = setUpReorderingMocks();
        when(mTabModel.indexOf(tabsInGroup.get(0))).thenReturn(0);

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();

        // In RTL, moving toward the start is "Move right". This option should not be available for
        // the first group.
        for (ListItem listItem : modelList) {
            if (!listItem.model.containsKey(ListMenuItemProperties.TITLE)) continue;
            assertNotEquals(
                    "Did not expect any item to have 'Move group right' title",
                    mActivity.getString(R.string.move_tab_group_right),
                    listItem.model.get(ListMenuItemProperties.TITLE));
        }
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveGroupRight_lastGroup() {
        mTabGroupContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        List<Tab> tabsInGroup = setUpReorderingMocks();
        when(mTabModel.indexOf(tabsInGroup.get(tabsInGroup.size() - 1))).thenReturn(4);
        when(mTabModel.getCount()).thenReturn(5);

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();

        for (ListItem listItem : modelList) {
            if (!listItem.model.containsKey(ListMenuItemProperties.TITLE)) continue;
            assertNotEquals(
                    "Did not expect any item to have 'Move group right' title",
                    mActivity.getString(R.string.move_tab_group_right),
                    listItem.model.get(ListMenuItemProperties.TITLE));
        }
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveGroupRight_lastGroup_RTL() {
        LocalizationUtils.setRtlForTesting(true);
        List<Tab> tabsInGroup = setUpReorderingMocks();
        when(mTabModel.indexOf(tabsInGroup.get(tabsInGroup.size() - 1))).thenReturn(4);
        when(mTabModel.getCount()).thenReturn(5);

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();

        // In RTL, moving toward the end is "Move left". This option should not be available for
        // the last group.
        for (ListItem listItem : modelList) {
            if (!listItem.model.containsKey(ListMenuItemProperties.TITLE)) continue;
            assertNotEquals(
                    "Did not expect any item to have 'Move group left' title",
                    mActivity.getString(R.string.move_tab_group_left),
                    listItem.model.get(ListMenuItemProperties.TITLE));
        }
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveGroupLeft_itemToLeftIsPinned() {
        mTabGroupContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        setUpReorderingMocks();

        Tab tab = mock(Tab.class);
        List<Tab> tabsInGroup = Arrays.asList(tab);
        when(mTabGroupModelFilter.getTabsInGroup(eq(TAB_GROUP_ID))).thenReturn(tabsInGroup);

        when(mTabModel.indexOf(tab)).thenReturn(1);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();

        for (ListItem listItem : modelList) {
            if (!listItem.model.containsKey(ListMenuItemProperties.TITLE)) continue;
            assertNotEquals(
                    "Did not expect any item to have 'Move group left' title",
                    mActivity.getString(R.string.move_tab_group_left),
                    listItem.model.get(ListMenuItemProperties.TITLE));
        }
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    @EnableFeatures(SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveGroupLeft_pinnedTabExistsFurtherLeft() {
        mTabGroupContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        List<Tab> tabsInGroup = setUpReorderingMocks();
        when(mTabGroupModelFilter.getTabsInGroup(eq(TAB_GROUP_ID))).thenReturn(tabsInGroup);

        when(mTabModel.indexOf(tabsInGroup.get(0))).thenReturn(2);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();
        ListItem moveLeftItem = modelList.get(6);
        moveLeftItem.model.get(ListMenuItemProperties.CLICK_LISTENER).onClick(mMenuView);

        verify(mReorderFunction).accept(TAB_GROUP_ID, true);
    }

    private List<Tab> setUpReorderingMocks() {
        mTabGroupContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(null);
        List<Tab> tabsInGroup = setUpTabGroupModelFilter();
        when(mTabModel.getCount()).thenReturn(5);
        when(mTabModel.indexOf(tabsInGroup.get(0))).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);
        return tabsInGroup;
    }
}

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
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.multiwindow.InstanceInfo.Type.CURRENT;
import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType.ACTIVE;
import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType.OFF_THE_RECORD;
import static org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils.UNSET_TAB_GROUP_TITLE;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_ITEMS;

import android.app.Activity;
import android.os.SystemClock;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.ListAdapter;
import android.widget.ListView;

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

import org.chromium.base.MathUtils;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
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
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator.OnItemClickedCallback;
import org.chromium.chrome.browser.tasks.tab_management.color_picker.ColorPickerContainer;
import org.chromium.chrome.browser.tasks.tab_management.color_picker.ColorPickerCoordinator;
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
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
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
@EnableFeatures({
    ChromeFeatureList.DATA_SHARING,
})
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
    private static final String INCOGNITO_WINDOW_TITLE = "Incognito Window";
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
                    /* closureTime= */ 0);

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
                    /* closureTime= */ 0);

    private static final InstanceInfo INSTANCE_INFO_INCOGNITO =
            new InstanceInfo(
                    INSTANCE_ID_2,
                    TASK_ID,
                    CURRENT,
                    EXAMPLE_URL.toString(),
                    INCOGNITO_WINDOW_TITLE,
                    /* customTitle= */ null,
                    NUM_TABS,
                    NUM_INCOGNITO_TABS,
                    /* isIncognitoSelected= */ true,
                    LAST_ACCESSED_TIME,
                    /* closureTime= */ 0);

    // Other dependencies
    @Mock private Profile mProfile;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private WeakReference<Activity> mWeakReferenceActivity;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private MultiInstanceOrchestrator mMultiInstanceOrchestrator;
    @Mock private BiConsumer<Token, Boolean> mReorderFunction;
    private Activity mActivity;
    private List<Tab> mTabsInGroup;
    private SettableNonNullObservableSupplier<Integer> mTotalTabCountSupplier;

    @Before
    public void setUp() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        when(mServiceStatus.isAllowedToCreate()).thenReturn(true);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(mMultiInstanceOrchestrator);

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
        // Non-incognito by default.
        when(mTabModel.isIncognito()).thenReturn(false);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        mTabModel.setTabRemoverForTesting(mTabRemover);
        mTabModel.setTabCreatorForTesting(mTabCreator);
        mTotalTabCountSupplier = ObservableSuppliers.createNonNull(3);
        when(mTabModel.getTabCountSupplier()).thenReturn(mTotalTabCountSupplier);
        mTabsInGroup = setUpTabGroupModelFilter();
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
    public void testListMenuItems_Incognito() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE)).thenReturn(List.of(INSTANCE_INFO_1));
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();

        // Assert: verify number of items in the model list.
        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // Assert: verify normal menu items.
        verifyNormalListItems(modelList, 3, /* isIncognito= */ true, List.of());
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testListMenuItems_Incognito_multipleWindows_IncognitoOnlyWindows() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        MultiWindowUtils.setInstanceCountForTesting(2);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE | OFF_THE_RECORD))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_INCOGNITO));

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();

        // Assert: verify number of items in the model list.
        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // Assert: verify normal menu items.
        // Instance 1 should be filtered out because it is the current instance.
        // Only the incognito window should be available as a destination (the non-incognito windows
        // should be filtered out).
        verifyNormalListItems(
                modelList, 3, /* isIncognito= */ true, List.of(INCOGNITO_WINDOW_TITLE));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testListMenuItems_Incognito_multipleWindows_MixedIncognitoWindows() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(false);
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        MultiWindowUtils.setInstanceCountForTesting(2);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2, INSTANCE_INFO_INCOGNITO));

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();

        // Assert: verify number of items in the model list.
        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // Assert: verify normal menu items.
        // All windows except the current window should be shown.
        verifyNormalListItems(
                modelList,
                3,
                /* isIncognito= */ true,
                List.of(WINDOW_TITLE_2, INCOGNITO_WINDOW_TITLE));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DATA_SHARING)
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

        // Verify tab group closed.
        mOnItemClickedCallback.onClick(
                menuId, TAB_GROUP_ID, /* collaborationId= */ null, listViewTouchTracker);
        verify(mTabRemover)
                .closeTabs(
                        argThat(
                                params ->
                                        params.tabs.get(0) == mTabsInGroup.get(0)
                                                && (params.allowUndo == shouldAllowUndo)
                                                && (params.hideTabGroups == shouldHideTabGroups)),
                        /* allowDialog= */ eq(true),
                        /* listener= */ any());
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuItemClicked_NewTabInGroup() {
        // Verify new tab opened in group.
        mOnItemClickedCallback.onClick(
                R.id.open_new_tab_in_group,
                TAB_GROUP_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabCreator)
                .createNewTab(any(), eq(TabLaunchType.FROM_TAB_GROUP_UI), eq(mTabsInGroup.get(0)));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuItemClicked_MoveGroup() {
        // Fake a click on the move group action.
        mOnItemClickedCallback.onClick(
                R.id.move_to_other_window_menu_id,
                TAB_GROUP_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);

        // Verify.
        verify(mMultiInstanceOrchestrator)
                .moveTabGroupToOtherWindow(
                        any(TabGroupMetadata.class), eq(NewWindowAppSource.MENU));
        verify(mMultiInstanceManager).closeChromeWindowIfEmpty(INSTANCE_ID_1);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testUpdateGroupTitleOnKeyboardHide() {
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
    public void testMoveToNewWindow() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE)).thenReturn(List.of(INSTANCE_INFO_1));
        var modelList = new ModelList();
        mTabGroupContextMenuCoordinator.configureMenuItemsForTesting(modelList, TAB_GROUP_ID);

        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                5,
                R.plurals.move_group_to_another_window_context_menu_item,
                List.of(),
                mActivity);

        StripLayoutContextMenuCoordinatorTestUtils.clickMoveToNewWindow(
                modelList, 5, mOnItemClickedCallback, TAB_GROUP_ID, COLLABORATION_ID);

        verify(mMultiInstanceOrchestrator)
                .moveTabGroupToOtherWindow(
                        any(TabGroupMetadata.class), eq(NewWindowAppSource.MENU));
        verify(mMultiInstanceManager).closeChromeWindowIfEmpty(INSTANCE_ID_1);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMoveToWindow() {
        MultiWindowUtils.setInstanceCountForTesting(2);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2));
        var modelList = new ModelList();
        mTabGroupContextMenuCoordinator.configureMenuItemsForTesting(modelList, TAB_GROUP_ID);

        StripLayoutContextMenuCoordinatorTestUtils.clickMoveToWindowRow(
                modelList, 5, WINDOW_TITLE_2, mMenuView);

        verify(mMultiInstanceOrchestrator)
                .moveTabGroupToWindowByIdChecked(
                        eq(INSTANCE_ID_2),
                        any(TabGroupMetadata.class),
                        eq(TabList.INVALID_TAB_INDEX),
                        eq(true));
        verify(mMultiInstanceManager).closeChromeWindowIfEmpty(INSTANCE_ID_1);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMoveToWindowItemHidden_WhenOnlyOneWindowAndAllTabsMoving() {
        // Set instance count to 1.
        MultiWindowUtils.setInstanceCountForTesting(1);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE)).thenReturn(List.of(INSTANCE_INFO_1));
        // Set total tab count to be equal to the group tab count (1).
        mTotalTabCountSupplier.set(1);

        var modelList = new ModelList();
        mTabGroupContextMenuCoordinator.configureMenuItemsForTesting(modelList, TAB_GROUP_ID);

        // Verify that move_to_other_window_menu_id is NOT in the model list.
        for (int i = 0; i < modelList.size(); i++) {
            if (modelList.get(i).model.containsKey(ListMenuItemProperties.MENU_ITEM_ID)) {
                assertNotEquals(
                        "Move to another window item should be hidden.",
                        R.id.move_to_other_window_menu_id,
                        modelList.get(i).model.get(ListMenuItemProperties.MENU_ITEM_ID));
            }
        }
    }

    private List<Tab> setUpTabGroupModelFilter() {
        MockTab tab = mTabModel.addTab(TAB_ID);
        tab.setTabGroupId(TAB_GROUP_ID);
        tab.setUrl(EXAMPLE_URL);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.getTabUngrouper()).thenReturn(mTabUngrouper);
        when(mTabGroupModelFilter.isTabInTabGroup(tab)).thenReturn(true);
        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabGroupModelFilter.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(UNSET_TAB_GROUP_TITLE);
        when(mTabGroupModelFilter.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB_ID);
        when(mTabGroupModelFilter.getTabCountForGroup(eq(TAB_GROUP_ID))).thenReturn(1);
        List<Tab> tabsInGroup = Arrays.asList(tab);
        when(mTabGroupModelFilter.getTabsInGroup(eq(TAB_GROUP_ID))).thenReturn(tabsInGroup);
        when(mTabModel.getTabsInGroup(eq(TAB_GROUP_ID))).thenReturn(tabsInGroup);
        when(mTabGroupModelFilter.getRelatedTabList(eq(TAB_ID))).thenReturn(tabsInGroup);
        when(mTabModel.getRelatedTabList(eq(TAB_ID))).thenReturn(tabsInGroup);
        return tabsInGroup;
    }

    @SuppressWarnings("DirectInvocationOnMock")
    private void verifyNormalListItems(ModelList modelList, int closeGroupPosition) {
        verifyNormalListItems(modelList, closeGroupPosition, false, List.of(WINDOW_TITLE_2));
    }

    @SuppressWarnings("DirectInvocationOnMock")
    private void verifyNormalListItems(
            ModelList modelList,
            int closeGroupPosition,
            boolean isIncognito,
            List<String> expectedWindowTitles) {
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
                expectedWindowTitles,
                mActivity,
                isIncognito);
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
    public void testListMenuItems_moveGroupItems_incognito() {
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        setUpReorderingMocks();

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();

        // Indices in incognito:
        // Index 0: Divider
        // Index 1: Open new tab in group
        // Index 2: Ungroup tab
        // Index 3: Close tab group
        // Index 4: Move to window (submenu)
        // Index 5: Move left
        // Index 6: Move right

        ListItem moveLeftItem = modelList.get(5);
        assertEquals(
                "Expected text appearance ID to be set to"
                        + " R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light"
                        + " in incognito",
                R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light,
                moveLeftItem.model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));
        assertEquals(
                "Expected icon tint to be set to R.color.default_icon_color_light_tint_list in"
                        + " incognito",
                R.color.default_icon_color_light_tint_list,
                moveLeftItem.model.get(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID));

        ListItem moveRightItem = modelList.get(6);
        assertEquals(
                "Expected text appearance ID to be set to"
                        + " R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light"
                        + " in incognito",
                R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light,
                moveRightItem.model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));
        assertEquals(
                "Expected icon tint to be set to R.color.default_icon_color_light_tint_list in"
                        + " incognito",
                R.color.default_icon_color_light_tint_list,
                moveRightItem.model.get(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMoveGroupLeft_itemToLeftIsPinned() {
        mTabGroupContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        setUpReorderingMocks();
        when(mTabModel.indexOf(mTabsInGroup.get(0))).thenReturn(1);
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
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(null);
        when(mTabModel.getCount()).thenReturn(5);
        when(mTabModel.indexOf(mTabsInGroup.get(0))).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);
        return mTabsInGroup;
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMoveToWindow_NonEmptyCustomWindowTitle() {
        final InstanceInfo emptyTitleInstance =
                new InstanceInfo(
                        INSTANCE_ID_2,
                        TASK_ID,
                        CURRENT,
                        EXAMPLE_URL.toString(),
                        /* title= */ "Example",
                        /* customTitle= */ "My window",
                        NUM_TABS,
                        NUM_INCOGNITO_TABS,
                        /* isIncognitoSelected= */ false,
                        LAST_ACCESSED_TIME,
                        /* closureTime= */ 0);

        MultiWindowUtils.setInstanceCountForTesting(2);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, emptyTitleInstance));
        var modelList = new ModelList();
        mTabGroupContextMenuCoordinator.configureMenuItemsForTesting(modelList, TAB_GROUP_ID);

        ListItem moveToWindowItem = modelList.get(5);
        assertNotNull(moveToWindowItem);

        var subMenu = moveToWindowItem.model.get(SUBMENU_ITEMS);
        assertEquals("Submenu should have 2 items", 2, subMenu.size());

        ListItem otherWindowItem = subMenu.get(1);
        assertEquals(
                "The title for the other window is incorrect.",
                "My window",
                otherWindowItem.model.get(TITLE));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMoveToWindow_EmptyCustomWindowTitle() {
        final InstanceInfo emptyTitleInstance =
                new InstanceInfo(
                        INSTANCE_ID_2,
                        TASK_ID,
                        CURRENT,
                        EXAMPLE_URL.toString(),
                        /* title= */ "Example",
                        /* customTitle= */ null,
                        NUM_TABS,
                        NUM_INCOGNITO_TABS,
                        /* isIncognitoSelected= */ false,
                        LAST_ACCESSED_TIME,
                        /* closureTime= */ 0);

        MultiWindowUtils.setInstanceCountForTesting(2);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, emptyTitleInstance));
        var modelList = new ModelList();
        mTabGroupContextMenuCoordinator.configureMenuItemsForTesting(modelList, TAB_GROUP_ID);

        ListItem moveToWindowItem = modelList.get(5);
        assertNotNull(moveToWindowItem);

        var subMenu = moveToWindowItem.model.get(SUBMENU_ITEMS);
        assertEquals("Submenu should have 2 items", 2, subMenu.size());

        ListItem otherWindowItem = subMenu.get(1);
        assertEquals(
                "The title for the other window is incorrect.",
                "Example",
                otherWindowItem.model.get(TITLE));
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuWidthAndColorPicker_MainMenu() {
        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);

        // 1. Verify visibility of title editor and color picker.
        View contentView = mTabGroupContextMenuCoordinator.getContentViewForTesting();
        assertNotNull(contentView);
        assertEquals(
                "Title editor should be visible",
                View.VISIBLE,
                contentView.findViewById(R.id.tab_group_title).getVisibility());
        assertEquals(
                "Color picker should be visible",
                View.VISIBLE,
                contentView.findViewById(R.id.color_picker_container).getVisibility());

        // 2. Verify menu width.
        verifyMenuWidthAndColorPicker(contentView);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuWidthAndVisibility_SubMenu() {
        HierarchicalMenuController.setDrillDownOverrideValueForTesting(true);
        MultiWindowUtils.setInstanceCountForTesting(2);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2));

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();

        // Click on "Move group to another window" to open a submenu.
        int moveToIndex = -1;
        for (int i = 0; i < modelList.size(); i++) {
            if (modelList.get(i).model.containsKey(SUBMENU_ITEMS)) {
                moveToIndex = i;
                break;
            }
        }
        assertTrue("Move to window item not found", moveToIndex != -1);
        ListItem moveItem = modelList.get(moveToIndex);
        moveItem.model.get(ListMenuItemProperties.CLICK_LISTENER).onClick(null);

        // 1. Verify visibility of title editor and color picker are GONE in submenu.
        View contentView = mTabGroupContextMenuCoordinator.getContentViewForTesting();
        assertEquals(
                "Title editor should be hidden in submenu",
                View.GONE,
                contentView.findViewById(R.id.tab_group_title).getVisibility());
        assertEquals(
                "Color picker should be hidden in submenu",
                View.GONE,
                contentView.findViewById(R.id.color_picker_container).getVisibility());

        // 2. Verify submenu width is updated (should be <= maxWidth).
        int maxWidthPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_strip_group_context_menu_max_width);

        ListView listView = contentView.findViewById(R.id.tab_group_action_menu_list);
        ViewGroup container = (ViewGroup) listView.getParent().getParent();
        int width = container.getLayoutParams().width;
        assertTrue("Submenu width " + width + " should be <= " + maxWidthPx, width <= maxWidthPx);
    }

    @Test
    @Feature("Tab Strip Group Context Menu")
    public void testMenuRestoration_NavigateBack() {
        HierarchicalMenuController.setDrillDownOverrideValueForTesting(true);
        MultiWindowUtils.setInstanceCountForTesting(2);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2));

        mTabGroupContextMenuCoordinator.showMenu(new RectProvider(), TAB_GROUP_ID);
        ModelList modelList = mTabGroupContextMenuCoordinator.getModelListForTesting();

        // Go to submenu.
        int moveToIndex = -1;
        for (int i = 0; i < modelList.size(); i++) {
            if (modelList.get(i).model.containsKey(SUBMENU_ITEMS)) {
                moveToIndex = i;
                break;
            }
        }
        assertTrue("Move to window item not found", moveToIndex != -1);
        ListItem moveItem = modelList.get(moveToIndex);
        moveItem.model.get(ListMenuItemProperties.CLICK_LISTENER).onClick(null);

        // Go back (by clicking the same item which now acts as a back button in hierarchical menu).
        // Wait, in HierarchicalMenuController, clicking the submenu header usually goes back.
        // Let's find the submenu header and click it.
        ModelList subMenuModelList = mTabGroupContextMenuCoordinator.getModelListForTesting();
        assertEquals(ListItemType.SUBMENU_HEADER, subMenuModelList.get(0).type);
        subMenuModelList.get(0).model.get(ListMenuItemProperties.CLICK_LISTENER).onClick(null);

        // 1. Verify restoration of title editor and color picker.
        View contentView = mTabGroupContextMenuCoordinator.getContentViewForTesting();
        assertEquals(
                "Title editor should be restored",
                View.VISIBLE,
                contentView.findViewById(R.id.tab_group_title).getVisibility());
        assertEquals(
                "Color picker should be restored",
                View.VISIBLE,
                contentView.findViewById(R.id.color_picker_container).getVisibility());

        // 2. Verify menu width.
        verifyMenuWidthAndColorPicker(contentView);
    }

    private void verifyMenuWidthAndColorPicker(View contentView) {
        ListView listView = contentView.findViewById(R.id.tab_group_action_menu_list);
        // Hierarchy: ScrollView -> LinearLayout (container) -> FrameLayout -> ListView
        ViewGroup container = (ViewGroup) listView.getParent().getParent();
        int width = container.getLayoutParams().width;

        ColorPickerContainer colorPicker = container.findViewById(R.id.color_picker_container);

        int minWidthPx = mActivity.getResources().getDimensionPixelSize(R.dimen.list_menu_width);
        int maxWidthPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_strip_group_context_menu_max_width);

        int expectedWidth;
        int singleRowWidth = colorPicker.getSingleRowWidth();
        if (singleRowWidth < maxWidthPx) {
            expectedWidth = singleRowWidth;
        } else {
            expectedWidth = colorPicker.getDoubleRowWidth();
        }

        // Also considers list item widths.
        ListAdapter listAdapter = listView.getAdapter();
        int maxItemWidth = 0;
        for (int i = 0; i < listAdapter.getCount(); i++) {
            View listItem = listAdapter.getView(i, null, listView);
            listItem.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
            maxItemWidth = Math.max(maxItemWidth, listItem.getMeasuredWidth());
        }

        expectedWidth = Math.max(expectedWidth, maxItemWidth);
        expectedWidth =
                MathUtils.clamp(
                        expectedWidth + listView.getPaddingLeft() + listView.getPaddingRight(),
                        minWidthPx,
                        maxWidthPx);

        assertEquals("Menu width is incorrect", expectedWidth, width);
    }
}

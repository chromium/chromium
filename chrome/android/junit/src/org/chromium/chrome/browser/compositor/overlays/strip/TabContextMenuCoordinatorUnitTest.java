// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.multiwindow.InstanceInfo.Type.CURRENT;
import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType.ACTIVE;
import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType.OFF_THE_RECORD;
import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType.REGULAR;
import static org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin.TAB_STRIP_CONTEXT_MENU;
import static org.chromium.ui.listmenu.ListItemType.DIVIDER;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM;
import static org.chromium.ui.listmenu.ListItemType.SUBMENU_HEADER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.START_ICON_DRAWABLE;
import static org.chromium.ui.listmenu.ListMenuItemProperties.START_ICON_WIDTH;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE_ID;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_PROVIDER;
import static org.chromium.ui.listmenu.ListSectionDividerProperties.COLOR_ID;

import android.annotation.StringRes;
import android.app.Activity;
import android.graphics.Rect;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ListView;

import androidx.annotation.PluralsRes;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator.AnchorInfo;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.send_tab_to_self.EntryPointDisplayReason;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridge;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridgeJni;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils.TabGroupCreationCallback;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator.OnItemClickedCallback;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncherSupplier;
import org.chromium.components.browser_ui.util.motion.MotionEventTestUtils;
import org.chromium.components.browser_ui.widget.list_view.FakeListViewTouchTracker;
import org.chromium.components.browser_ui.widget.list_view.ListViewTouchTracker;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.RectProvider;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Set;
import java.util.function.BiConsumer;

/** Unit tests for {@link TabContextMenuCoordinator}. */

// =========================================================================
// FILE STRUCTURE & TESTING GUIDELINES
// =========================================================================
//
// This test class is divided into three main sections:
//
// 1. Menu item order tests:
//    - These tests verify that the context menu items appear in the correct
//      and expected order under various conditions (e.g., standard,
//      incognito, single vs. multiple tabs selected).
//    - They only assert the pure state and ordering of the items, and do not
//      test callback behaviors, modifications, or conditional absences.
//
// 2. Menu item behavior tests:
//    - These tests verify the callbacks and interactive behavior of menu items.
//    - This includes asserting whether certain items should be absent/present
//      under specific conditions, submenu verification, or dynamic updates
//      to menu items (e.g., pin/unpin state changes).
//
// 3. Utility methods:
//    - Private helper and setup methods shared across the test suite to mock
//      tabs, populate models, and build menu states.
//
// =========================================================================
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.DATA_SHARING,
    ChromeFeatureList.ANDROID_CONTEXT_MENU_NEW_ACTIONS,
    ChromeFeatureList.ANDROID_CONTEXT_MENU_DISABLED_MENU_ITEMS
})
@DisableFeatures({TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS})
public class TabContextMenuCoordinatorUnitTest {
    private static final int TAB_ID = 1;
    private static final int TAB_OUTSIDE_OF_GROUP_ID = 2;
    private static final int NON_URL_TAB_ID = 3;
    private static final int TAB_ID_2 = 4;
    private static final Token TAB_GROUP_ID = Token.createRandom();
    private static final String TAB_GROUP_ID_STRING = TAB_GROUP_ID.toString();
    private static final String TAB_GROUP_TITLE = "Tab Group Title";
    private static final int TAB_GROUP_INDICATOR_COLOR_ID = 8;
    private static final String COLLABORATION_ID = "CollaborationId";
    private static final GURL EXAMPLE_URL = new GURL("https://example.com");
    private static final GURL CHROME_SCHEME_URL = new GURL("chrome://history");
    private static final GURL CHROME_NATIVE_URL = new GURL("chrome-native://newtab");
    private static final int INSTANCE_ID_1 = 5;
    private static final int INSTANCE_ID_2 = 6;
    private static final int INSTANCE_ID_3 = 7;
    private static final String WINDOW_TITLE_1 = "Window Title 1";
    private static final String WINDOW_TITLE_2 = "Window Title 2";
    private static final String INCOGNITO_WINDOW_TITLE = "Incognito Window";
    private static final int TASK_ID = 8;
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
                    INSTANCE_ID_3,
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

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private TabContextMenuCoordinator mTabContextMenuCoordinator;
    private OnItemClickedCallback<AnchorInfo> mOnItemClickedCallback;
    private MockTabModel mTabModel;
    private final LocalTabGroupId mLocalId = new LocalTabGroupId(TAB_GROUP_ID);
    private final SavedTabGroup mSavedTabGroup = new SavedTabGroup();
    private final SavedTabGroupTab mSavedTabGroupTab = new SavedTabGroupTab();
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabList mTabList;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTabOutsideOfGroup;
    @Mock private Tab mNonUrlTab;
    @Mock private TabRemover mTabRemover;
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private SendTabToSelfAndroidBridge.Natives mSendTabToSelfAndroidBridgeNatives;
    @Mock private Profile mProfile;
    @Mock private TabGroupListBottomSheetCoordinator mBottomSheetCoordinator;
    @Mock private TabGroupCreationCallback mTabGroupCreationCallback;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private MultiInstanceOrchestrator mMultiInstanceOrchestrator;
    @Mock private ShareDelegate mShareDelegate;
    @Mock private TabBookmarker mTabBookmarker;
    @Mock private TabCreator mTabCreator;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private ActivityResultTracker mActivityResultTracker;
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ServiceStatus mServiceStatus;
    @Mock private WeakReference<Activity> mWeakReferenceActivity;
    @Mock private View mView;
    @Mock private WebContents mWebContents;
    @Mock private Tab mChromeSchemeTabWithWebContents;
    @Mock private Tab mChromeSchemeTabWithoutWebContents;
    @Mock private Tab mChromeNativeSchemeTabWithWebContents;
    @Mock private Tab mChromeNativeSchemeTabWithoutWebContents;
    @Mock private BiConsumer<AnchorInfo, Boolean> mReorderFunction;
    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;

    private Activity mActivity;
    private SettableNonNullObservableSupplier<Integer> mTotalTabCountSupplier;

    @Before
    public void setUp() {
        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mSendTabToSelfAndroidBridgeNatives);
        when(mSendTabToSelfAndroidBridgeNatives.getEntryPointDisplayReason(any(), any()))
                .thenReturn(EntryPointDisplayReason.OFFER_FEATURE);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(mMultiInstanceOrchestrator);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        when(mServiceStatus.isAllowedToCreate()).thenReturn(true);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);

        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        when(mWindowAndroid.getKeyboardDelegate()).thenReturn(mKeyboardVisibilityDelegate);
        when(mWindowAndroid.getActivity()).thenReturn(mWeakReferenceActivity);
        when(mWeakReferenceActivity.get()).thenReturn(mActivity);
        List<Tab> tabList = List.of(mTab1, mTabOutsideOfGroup);
        when(mTabList.iterator()).thenAnswer(invocation -> tabList.iterator());
        when(mTabList.getCount()).thenReturn(2);
        when(mTabList.getTabAtChecked(0)).thenReturn(mTab1);
        when(mTabList.getTabAtChecked(1)).thenReturn(mTabOutsideOfGroup);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab1);
        when(mTabModel.getTabById(TAB_ID_2)).thenReturn(mTab2);
        when(mTabModel.getTabById(TAB_OUTSIDE_OF_GROUP_ID)).thenReturn(mTabOutsideOfGroup);
        when(mTabModel.getTabById(NON_URL_TAB_ID)).thenReturn(mNonUrlTab);
        when(mTab1.getId()).thenReturn(TAB_ID);
        when(mTab2.getId()).thenReturn(TAB_ID_2);
        when(mTabOutsideOfGroup.getId()).thenReturn(TAB_OUTSIDE_OF_GROUP_ID);
        when(mNonUrlTab.getId()).thenReturn(NON_URL_TAB_ID);
        when(mTabModel.getComprehensiveModel()).thenReturn(mTabList);
        mTabModel.setTabRemoverForTesting(mTabRemover);
        mTabModel.setTabCreatorForTesting(mTabCreator);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTab1.getUrl()).thenReturn(EXAMPLE_URL);
        when(mTab2.getUrl()).thenReturn(EXAMPLE_URL);
        when(mTabOutsideOfGroup.getTabGroupId()).thenReturn(null);
        when(mTabOutsideOfGroup.getUrl()).thenReturn(EXAMPLE_URL);
        when(mNonUrlTab.getTabGroupId()).thenReturn(null);
        when(mNonUrlTab.getUrl()).thenReturn(CHROME_SCHEME_URL);
        when(mTabWindowManager.findWindowIdForTabGroup(TAB_GROUP_ID)).thenReturn(INSTANCE_ID_1);
        when(mTabWindowManager.getTabModelSelectorById(INSTANCE_ID_1))
                .thenReturn(mTabModelSelector);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getTabUngrouper()).thenReturn(mTabUngrouper);
        when(mTabModel.getAllTabGroupIds()).thenReturn(Set.of(TAB_GROUP_ID));
        when(mTabModel.getTabCountForGroup(TAB_GROUP_ID)).thenReturn(1);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(Collections.singletonList(mTab1));
        when(mTabModel.getTabGroupColor(TAB_GROUP_ID)).thenReturn(TAB_GROUP_INDICATOR_COLOR_ID);
        when(mTabModel.getTabGroupColorWithFallback(TAB_GROUP_ID))
                .thenReturn(TAB_GROUP_INDICATOR_COLOR_ID);
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(TAB_GROUP_TITLE);
        mTotalTabCountSupplier = ObservableSuppliers.createNonNull(3);
        when(mTabModel.getTabCountSupplier()).thenReturn(mTotalTabCountSupplier);
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(INSTANCE_ID_1);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(Collections.singletonList(INSTANCE_INFO_1));
        // Create persisted instance state.
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 3,
                /* numInactive= */ 0,
                SupportedProfileType.MIXED,
                /* startId= */ INSTANCE_ID_1);

        // Mute related setup.
        when(mTab1.getWebContents()).thenReturn(mWebContents);
        when(mTab2.getWebContents()).thenReturn(null);
        when(mChromeSchemeTabWithWebContents.getUrl()).thenReturn(CHROME_SCHEME_URL);
        when(mChromeSchemeTabWithWebContents.getWebContents()).thenReturn(mWebContents);
        when(mChromeSchemeTabWithoutWebContents.getUrl()).thenReturn(CHROME_SCHEME_URL);
        when(mChromeSchemeTabWithoutWebContents.getWebContents()).thenReturn(null);
        when(mChromeNativeSchemeTabWithWebContents.getUrl()).thenReturn(CHROME_NATIVE_URL);
        when(mChromeNativeSchemeTabWithWebContents.getWebContents()).thenReturn(mWebContents);
        when(mChromeNativeSchemeTabWithoutWebContents.getUrl()).thenReturn(CHROME_NATIVE_URL);
        when(mChromeNativeSchemeTabWithoutWebContents.getWebContents()).thenReturn(null);

        mSavedTabGroupTab.localId = TAB_ID;
        mSavedTabGroupTab.url = EXAMPLE_URL;
        mSavedTabGroup.savedTabs = Arrays.asList(mSavedTabGroupTab);
        mSavedTabGroup.collaborationId = COLLABORATION_ID;
        mSavedTabGroup.localId = mLocalId;
        mSavedTabGroup.title = TAB_GROUP_TITLE;
        mSavedTabGroup.color = TAB_GROUP_INDICATOR_COLOR_ID;
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {TAB_GROUP_ID_STRING});
        when(mTabGroupSyncService.getGroup(TAB_GROUP_ID_STRING)).thenReturn(mSavedTabGroup);
        setupWithIncognito(/* incognito= */ false); // Most tests will run not in incognito mode
        initializeCoordinator();
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.VERTICAL_TABS_ENABLED);
    }

    private void setupWithIncognito(boolean incognito) {
        when(mTabModel.isIncognito()).thenReturn(incognito);
        when(mTabModel.isIncognitoBranded()).thenReturn(incognito);
        when(mProfile.isOffTheRecord()).thenReturn(incognito);
        if (incognito) TabGroupSyncServiceFactory.setForTesting(null);
    }

    private void initializeCoordinator() {
        mOnItemClickedCallback =
                TabContextMenuCoordinator.getMenuItemClickedCallback(
                        () -> mTabModel,
                        mBottomSheetCoordinator,
                        mTabGroupCreationCallback,
                        mMultiInstanceManager,
                        ObservableSuppliers.createMonotonic(mShareDelegate),
                        () -> mTabBookmarker,
                        mWindowAndroid,
                        mActivity,
                        mSnackbarManager,
                        mActivityResultTracker,
                        mModalDialogManager);
        mTabContextMenuCoordinator =
                TabContextMenuCoordinator.createContextMenuCoordinator(
                        () -> mTabModel,
                        mBottomSheetCoordinator,
                        mTabGroupCreationCallback,
                        mMultiInstanceManager,
                        ObservableSuppliers.createMonotonic(mShareDelegate),
                        mWindowAndroid,
                        mActivity,
                        () -> mTabBookmarker,
                        mReorderFunction,
                        mSnackbarManager,
                        mActivityResultTracker,
                        mModalDialogManager);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testAnchorWidth() {
        StripLayoutContextMenuCoordinatorTestUtils.testAnchorWidth(
                mWeakReferenceActivity, mTabContextMenuCoordinator::getMenuWidth);
    }

    @Test
    public void testAnchor_offset() {
        StripLayoutContextMenuCoordinatorTestUtils.testAnchor_offset(
                (rectProvider) ->
                        mTabContextMenuCoordinator.showMenu(
                                rectProvider,
                                new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID))),
                mTabContextMenuCoordinator::destroyMenuForTesting);
    }

    @Test
    public void testAnchor_offset_incognito() {
        setupWithIncognito(/* incognito= */ true);
        StripLayoutContextMenuCoordinatorTestUtils.testAnchor_offset_incognito(
                (rectProvider) ->
                        mTabContextMenuCoordinator.showMenu(
                                rectProvider,
                                new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID))),
                mTabContextMenuCoordinator::destroyMenuForTesting);
    }

    // --------------------------------------------------------------//
    // ------------------ MENU ITEM ORDER TESTS ---------------------//
    // --------------------------------------------------------------//

    @Test
    @Feature("Tab Strip Context Menu")
    public void testListMenuItems_tabInGroup() {
        mTabModel.addTab(
                mTab1,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mTabOutsideOfGroup,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        assertEquals("Number of items in the list menu is incorrect", 12, modelList.size());

        // List item 1
        assertEquals(
                R.id.new_tab_to_the_right_menu_id,
                modelList.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 2
        var addToGroupItem = modelList.get(1);
        assertEquals(
                "Expected 'Add to group' item to have no submenu when the anchor tab is in the only"
                        + " existing group",
                MENU_ITEM,
                addToGroupItem.type);
        assertEquals(
                "Expected title to be 'Add to new group'",
                mActivity
                        .getResources()
                        .getQuantityString(R.plurals.add_tab_to_new_group_menu_item, 1),
                addToGroupItem.model.get(TITLE));

        // List item 3
        assertEquals(
                mActivity
                        .getResources()
                        .getQuantityString(R.plurals.remove_tabs_from_group_menu_item, 1),
                modelList.get(2).model.get(TITLE));
        assertEquals(
                R.id.remove_from_tab_group,
                modelList.get(2).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 4
        assertEquals(DIVIDER, modelList.get(3).type);
        assertEquals(
                "Expected divider to have have COLOR_ID unset when not in incognito mode",
                0,
                modelList.get(3).model.get(COLOR_ID));

        // List item 5
        assertEquals(
                mActivity.getResources().getString(R.string.duplicate_tab_menu_item),
                modelList.get(4).model.get(TITLE));
        assertEquals(
                R.id.duplicate_tab_menu_id,
                modelList.get(4).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 6
        assertEquals(
                mActivity.getResources().getQuantityString(R.plurals.pin_tabs_menu_item, 1),
                modelList.get(5).model.get(TITLE));
        assertEquals(
                R.id.pin_tab_menu_id,
                modelList.get(5).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 7
        assertEquals(
                R.id.mute_site_menu_id,
                modelList.get(6).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 8
        assertEquals(
                R.id.add_tab_to_reading_list_menu_id,
                modelList.get(7).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 9
        assertEquals(
                R.id.send_to_your_devices_menu_id,
                modelList.get(8).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 10
        assertEquals(R.string.close, modelList.get(9).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(9).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 11
        assertEquals(
                R.id.close_other_tabs_menu_id,
                modelList.get(10).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 12
        assertEquals(
                R.id.close_tabs_to_the_right_menu_id,
                modelList.get(11).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_tabInGroup_multipleTabs() {
        mTabModel.addTab(
                mTab1,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mNonUrlTab,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mTabOutsideOfGroup,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, List.of(TAB_ID, NON_URL_TAB_ID)));

        assertEquals("Number of items in the list menu is incorrect", 11, modelList.size());

        // List item 1
        assertEquals(
                R.id.new_tab_to_the_right_menu_id,
                modelList.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 2
        var addToGroupItem = modelList.get(1);
        assertEquals(
                "Expected 'Add to group' item to have no submenu when the anchor tab is in the only"
                        + " existing group",
                MENU_ITEM,
                addToGroupItem.type);
        assertEquals(
                "Expected title to be 'Add to new group'",
                mActivity
                        .getResources()
                        .getQuantityString(R.plurals.add_tab_to_new_group_menu_item, 2),
                addToGroupItem.model.get(TITLE));

        // List item 3
        assertEquals(
                mActivity
                        .getResources()
                        .getQuantityString(R.plurals.remove_tabs_from_group_menu_item, 2),
                modelList.get(2).model.get(TITLE));
        assertEquals(
                R.id.remove_from_tab_group,
                modelList.get(2).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 4
        assertEquals(DIVIDER, modelList.get(3).type);
        assertEquals(
                "Expected divider to have have COLOR_ID unset when not in incognito mode",
                0,
                modelList.get(3).model.get(COLOR_ID));

        // List item 5
        assertEquals(
                mActivity.getResources().getString(R.string.duplicate_tab_menu_item),
                modelList.get(4).model.get(TITLE));
        assertEquals(
                R.id.duplicate_tab_menu_id,
                modelList.get(4).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 6
        assertEquals(
                mActivity.getResources().getQuantityString(R.plurals.pin_tabs_menu_item, 2),
                modelList.get(5).model.get(TITLE));
        assertEquals(
                R.id.pin_tab_menu_id,
                modelList.get(5).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 7
        assertEquals(
                R.id.mute_site_menu_id,
                modelList.get(6).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 8
        assertEquals(
                R.id.add_tab_to_reading_list_menu_id,
                modelList.get(7).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 9
        assertEquals(R.string.close, modelList.get(8).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(8).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 10
        assertEquals(
                R.id.close_other_tabs_menu_id,
                modelList.get(9).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 11
        assertEquals(
                R.id.close_tabs_to_the_right_menu_id,
                modelList.get(10).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_tabOutsideOfGroup() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        mTabModel.addTab(
                mTabOutsideOfGroup,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mTab1,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 12, modelList.size());

        // List item 1
        assertEquals(
                R.id.new_tab_to_the_right_menu_id,
                modelList.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 2
        verifyAddToGroupSubmenuForTabOutsideOfGroup(
                modelList, TAB_GROUP_TITLE, 1, /* isIncognito= */ false);

        // List item 3
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList, 2, R.plurals.move_tab_to_another_window, List.of(), mActivity);

        // List item 4
        assertEquals(DIVIDER, modelList.get(3).type);

        // List item 5
        assertEquals(
                mActivity.getResources().getString(R.string.duplicate_tab_menu_item),
                modelList.get(4).model.get(TITLE));
        assertEquals(
                R.id.duplicate_tab_menu_id,
                modelList.get(4).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 6
        assertEquals(
                mActivity.getResources().getQuantityString(R.plurals.pin_tabs_menu_item, 1),
                modelList.get(5).model.get(TITLE));
        assertEquals(
                R.id.pin_tab_menu_id,
                modelList.get(5).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 7
        assertEquals(
                R.id.mute_site_menu_id,
                modelList.get(6).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 8
        assertEquals(
                R.id.add_tab_to_reading_list_menu_id,
                modelList.get(7).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 9
        assertEquals(
                R.id.send_to_your_devices_menu_id,
                modelList.get(8).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 10
        assertEquals(R.string.close, modelList.get(9).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(9).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 11
        assertEquals(
                R.id.close_other_tabs_menu_id,
                modelList.get(10).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 12
        assertEquals(
                R.id.close_tabs_to_the_right_menu_id,
                modelList.get(11).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_tabOutsideOfGroup_multipleTabs() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        mTabModel.addTab(
                mTabOutsideOfGroup,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mTab1,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mNonUrlTab,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        List.of(TAB_OUTSIDE_OF_GROUP_ID, TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 11, modelList.size());

        // List item 1
        assertEquals(
                R.id.new_tab_to_the_right_menu_id,
                modelList.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 2
        verifyAddToGroupSubmenuForTabOutsideOfGroup(
                modelList, TAB_GROUP_TITLE, 2, /* isIncognito= */ false);

        // List item 3
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList, 2, R.plurals.move_tabs_to_another_window, List.of(), mActivity);

        // List item 4
        assertEquals(DIVIDER, modelList.get(3).type);

        // List item 5
        assertEquals(
                mActivity.getResources().getString(R.string.duplicate_tab_menu_item),
                modelList.get(4).model.get(TITLE));
        assertEquals(
                R.id.duplicate_tab_menu_id,
                modelList.get(4).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 6
        assertEquals(
                mActivity.getResources().getQuantityString(R.plurals.pin_tabs_menu_item, 2),
                modelList.get(5).model.get(TITLE));
        assertEquals(
                R.id.pin_tab_menu_id,
                modelList.get(5).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 7
        assertEquals(
                R.id.mute_site_menu_id,
                modelList.get(6).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 8
        assertEquals(
                R.id.add_tab_to_reading_list_menu_id,
                modelList.get(7).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 9
        assertEquals(R.string.close, modelList.get(8).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(8).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 10
        assertEquals(
                R.id.close_other_tabs_menu_id,
                modelList.get(9).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 11
        assertEquals(
                R.id.close_tabs_to_the_right_menu_id,
                modelList.get(10).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_incognito() {
        setupWithIncognito(/* incognito= */ true);
        initializeCoordinator();
        mTabModel.addTab(
                mTabOutsideOfGroup,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mTab1,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 10, modelList.size());

        // List item 1
        assertEquals(
                R.id.new_tab_to_the_right_menu_id,
                modelList.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                "Expected text appearance ID to be set to"
                    + " R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light in"
                    + " incognito",
                R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light,
                modelList.get(0).model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));

        // List item 2
        verifyAddToGroupSubmenuForTabOutsideOfGroup(
                modelList, TAB_GROUP_TITLE, 1, /* isIncognito= */ true);
        assertEquals(
                "Expected text appearance ID to be set to"
                        + " R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light"
                        + " in incognito for submenu parent",
                R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light,
                modelList.get(1).model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));

        // List item 3
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                2,
                R.plurals.move_tab_to_another_window,
                List.of(),
                mActivity,
                /* isIncognito= */ true);

        // List item 4
        assertEquals(DIVIDER, modelList.get(3).type);
        assertEquals(
                "Expected divider to have COLOR_ID set to R.color.divider_color_light in"
                        + " incognito mode",
                R.color.divider_color_light,
                modelList.get(3).model.get(COLOR_ID));

        // List item 5
        assertEquals(
                mActivity.getResources().getString(R.string.duplicate_tab_menu_item),
                modelList.get(4).model.get(TITLE));
        assertEquals(
                R.id.duplicate_tab_menu_id,
                modelList.get(4).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                "Expected text appearance ID to be set to"
                    + " R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light in"
                    + " incognito",
                R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light,
                modelList.get(4).model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));

        // List item 6
        assertEquals(
                mActivity.getResources().getQuantityString(R.plurals.pin_tabs_menu_item, 1),
                modelList.get(5).model.get(TITLE));
        assertEquals(
                R.id.pin_tab_menu_id,
                modelList.get(5).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 7
        assertEquals(
                R.id.mute_site_menu_id,
                modelList.get(6).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 8
        assertEquals(R.string.close, modelList.get(7).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(7).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                "Expected text appearance ID to be set to"
                    + " R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light in"
                    + " incognito",
                R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light,
                modelList.get(7).model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));

        // List item 9
        assertEquals(
                R.id.close_other_tabs_menu_id,
                modelList.get(8).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 10
        assertEquals(
                R.id.close_tabs_to_the_right_menu_id,
                modelList.get(9).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_incognito_multipleTabs() {
        setupWithIncognito(/* incognito= */ true);
        initializeCoordinator();
        mTabModel.addTab(
                mTabOutsideOfGroup,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mTab1,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mNonUrlTab,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        List.of(TAB_OUTSIDE_OF_GROUP_ID, TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 10, modelList.size());

        // List item 1
        assertEquals(
                R.id.new_tab_to_the_right_menu_id,
                modelList.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                "Expected text appearance ID to be set to"
                    + " R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light in"
                    + " incognito",
                R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light,
                modelList.get(0).model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));

        // List item 2
        verifyAddToGroupSubmenuForTabOutsideOfGroup(
                modelList, TAB_GROUP_TITLE, 2, /* isIncognito= */ true);

        // List item 3
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                2,
                R.plurals.move_tabs_to_another_window,
                List.of(),
                mActivity,
                /* isIncognito= */ true);

        // List item 4
        assertEquals(DIVIDER, modelList.get(3).type);
        assertEquals(
                "Expected divider to have COLOR_ID set to R.color.divider_color_light in"
                        + " incognito mode",
                R.color.divider_color_light,
                modelList.get(3).model.get(COLOR_ID));

        // List item 5
        assertEquals(
                mActivity.getResources().getString(R.string.duplicate_tab_menu_item),
                modelList.get(4).model.get(TITLE));
        assertEquals(
                R.id.duplicate_tab_menu_id,
                modelList.get(4).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                "Expected text appearance ID to be set to"
                    + " R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light in"
                    + " incognito",
                R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light,
                modelList.get(4).model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));

        // List item 6
        assertEquals(
                mActivity.getResources().getQuantityString(R.plurals.pin_tabs_menu_item, 2),
                modelList.get(5).model.get(TITLE));
        assertEquals(
                R.id.pin_tab_menu_id,
                modelList.get(5).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 7
        assertEquals(
                R.id.mute_site_menu_id,
                modelList.get(6).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 8
        assertEquals(R.string.close, modelList.get(7).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(7).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                "Expected text appearance ID to be set to"
                    + " R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light in"
                    + " incognito",
                R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light,
                modelList.get(7).model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));

        // List item 9
        assertEquals(
                R.id.close_other_tabs_menu_id,
                modelList.get(8).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 10
        assertEquals(
                R.id.close_tabs_to_the_right_menu_id,
                modelList.get(9).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    // --------------------------------------------------------------//
    // ------------------ MENU ITEM BEHAVIOR TESTS ------------------//
    // --------------------------------------------------------------//

    @Test
    @Feature("Tab Strip Context Menu")
    public void testAddToNewTabGroup() {
        mOnItemClickedCallback.onClick(
                R.id.add_to_new_tab_group,
                new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)),
                COLLABORATION_ID,
                /* listViewTouchTracker= */ null);
        verify(mTabModel, times(1)).createSingleTabGroup(mTab1);
        verify(mTabGroupCreationCallback, times(1)).onTabGroupCreated(TAB_GROUP_ID);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testAddToNewTabGroup_multipleTabs() {
        mOnItemClickedCallback.onClick(
                R.id.add_to_new_tab_group,
                new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)),
                COLLABORATION_ID,
                /* listViewTouchTracker= */ null);
        verify(mTabModel, times(1))
                .mergeListOfTabsToGroup(
                        eq(List.of(mTab1, mTab2)),
                        eq(mTab1),
                        eq(TabGroupMergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP));
        verify(mTabGroupCreationCallback, times(1)).onTabGroupCreated(TAB_GROUP_ID);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @SuppressWarnings("DirectInvocationOnMock")
    public void testAddToGroupSubmenu_fallbackTabGroupName() {
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn("");
        MultiWindowUtils.setInstanceCountForTesting(1);
        mSavedTabGroup.title = "";
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        verifyAddToGroupSubmenuForTabOutsideOfGroup(
                modelList, "1 tab", 1, /* isIncognito= */ false);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @SuppressWarnings("DirectInvocationOnMock")
    public void testAddToGroupSubmenu_fallbackTabGroupName_incognito() {
        setupWithIncognito(true);
        initializeCoordinator();
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn("");
        MultiWindowUtils.setInstanceCountForTesting(1);
        mSavedTabGroup.title = "";
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, "1 tab", 1, /* isIncognito= */ true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testAddToGroup_showsAsAddToNewGroupWhenNoGroupsExist() {
        // No groups
        when(mTabModel.getAllTabGroupIds()).thenReturn(Collections.emptySet());
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[0]);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        ListItem addToGroupItem = findItemByMenuId(modelList, R.id.add_to_new_tab_group);
        assertNotNull(addToGroupItem);
        assertEquals("Should be a regular menu item", MENU_ITEM, addToGroupItem.type);
        assertEquals(
                "Title should be 'Add tab to new group'",
                mActivity
                        .getResources()
                        .getQuantityString(R.plurals.add_tab_to_new_group_menu_item, 1),
                addToGroupItem.model.get(TITLE));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testSubmenuSelection() {
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));
        mTabContextMenuCoordinator.showMenu(
                new RectProvider(new Rect(0, 0, 100, 100)),
                new AnchorInfo(TAB_ID, List.of(TAB_ID)));

        // Click into "Add to group" submenu.
        ListItem addToGroupItem =
                findItemByTitle(
                        modelList,
                        mActivity
                                .getResources()
                                .getQuantityString(R.plurals.add_tab_to_group_menu_item, 1));
        assertNotNull("Add to group item should be present", addToGroupItem);
        addToGroupItem.model.get(CLICK_LISTENER).onClick(mView);

        RobolectricUtil.runAllBackgroundAndUi();
        // Verify that the top item of the submenu is selected.
        ListView listView =
                mTabContextMenuCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.tab_group_action_menu_list);
        assertEquals(
                "Expected 1st item to be selected after navigating into submenu",
                0,
                listView.getSelectedItemPosition());

        // Click back to parent menu.
        ListItem headerItem = findItemByType(modelList, SUBMENU_HEADER);
        assertNotNull("Submenu header should be present", headerItem);
        headerItem.model.get(CLICK_LISTENER).onClick(mView);
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify that the top item of the parent menu is selected.
        assertEquals(
                "Expected 1st item to be selected after navigating out of submenu",
                0,
                listView.getSelectedItemPosition());

        mTabContextMenuCoordinator.destroyMenuForTesting();
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testRemoveFromGroup() {
        mOnItemClickedCallback.onClick(
                R.id.remove_from_tab_group,
                new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)),
                COLLABORATION_ID,
                /* listViewTouchTracker= */ null);
        verify(mTabUngrouper, times(1)).ungroupTabs(Collections.singletonList(mTab1), true, true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveToNewWindow() {
        var modelList = new ModelList();
        mTabModel.addTab(
                mTabOutsideOfGroup,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));
        ListItem moveToWindowItem =
                findItemByPluralsId(modelList, R.plurals.move_tab_to_another_window);
        assertNotNull(moveToWindowItem);
        int moveIndex = modelList.indexOf(moveToWindowItem);
        StripLayoutContextMenuCoordinatorTestUtils.clickMoveToNewWindow(
                modelList,
                moveIndex,
                mOnItemClickedCallback,
                new AnchorInfo(TAB_OUTSIDE_OF_GROUP_ID, List.of(TAB_OUTSIDE_OF_GROUP_ID)),
                COLLABORATION_ID);
        verify(mMultiInstanceOrchestrator)
                .moveTabsToOtherWindow(
                        Collections.singletonList(mTabOutsideOfGroup), NewWindowAppSource.MENU);
        verify(mMultiInstanceManager).closeChromeWindowIfEmpty(INSTANCE_ID_1);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveToWindow() {
        MultiWindowUtils.setInstanceCountForTesting(2);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2));
        var modelList = new ModelList();
        mTabModel.addTab(
                mTabOutsideOfGroup,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        ListItem moveToWindowItem =
                findItemByPluralsId(modelList, R.plurals.move_tab_to_another_window);
        assertNotNull(moveToWindowItem);
        int moveIndex = modelList.indexOf(moveToWindowItem);
        StripLayoutContextMenuCoordinatorTestUtils.clickMoveToWindowRow(
                modelList, moveIndex, WINDOW_TITLE_2, mView);

        verify(mMultiInstanceOrchestrator)
                .moveTabsToWindowByIdChecked(
                        INSTANCE_ID_2,
                        Collections.singletonList(mTabOutsideOfGroup),
                        /* destTabIndex= */ TabList.INVALID_TAB_INDEX,
                        /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                        /* bringToFront= */ true);
        verify(mMultiInstanceManager).closeChromeWindowIfEmpty(INSTANCE_ID_1);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @SuppressWarnings("DirectInvocationOnMock")
    public void testMoveToWindowSubmenu_multipleWindows() {
        MultiWindowUtils.setInstanceCountForTesting(3);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2));

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        ListItem moveItem = findItemByPluralsId(modelList, R.plurals.move_tab_to_another_window);
        assertNotNull(moveItem);
        int moveIndex = modelList.indexOf(moveItem);

        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                moveIndex,
                R.plurals.move_tab_to_another_window,
                Collections.singletonList(WINDOW_TITLE_2),
                mActivity);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveToWindowSubmenu_multipleWindows_preventMoveToNewWindow() {
        int instanceType = ACTIVE;
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            instanceType |= REGULAR;
        }
        MultiWindowUtils.setInstanceCountForTesting(2);
        when(mMultiInstanceManager.getInstanceInfo(instanceType))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2));

        // Set total tab count to be equal to the move tab count (1).
        mTotalTabCountSupplier.set(1);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        ListItem moveItem = findItemByPluralsId(modelList, R.plurals.move_tab_to_another_window);
        assertNotNull(moveItem);
        int moveIndex = modelList.indexOf(moveItem);

        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                moveIndex,
                R.plurals.move_tab_to_another_window,
                Collections.singletonList(WINDOW_TITLE_2),
                mActivity,
                /* isIncognito= */ false,
                /* expectNewWindow= */ false);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveToWindowSubmenu_incognito_multipleWindows() {
        setupWithIncognito(/* incognito= */ true);
        initializeCoordinator();
        MultiWindowUtils.setInstanceCountForTesting(3);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_INCOGNITO));

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        ListItem moveItem = findItemByPluralsId(modelList, R.plurals.move_tab_to_another_window);
        assertNotNull(moveItem);
        int moveIndex = modelList.indexOf(moveItem);

        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                moveIndex,
                R.plurals.move_tab_to_another_window,
                Collections.singletonList(INCOGNITO_WINDOW_TITLE),
                mActivity,
                /* isIncognito= */ true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveToWindowSubmenu_incognito_filtersNonIncognitoWindows() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        setupWithIncognito(/* incognito= */ true);
        initializeCoordinator();
        MultiWindowUtils.setInstanceCountForTesting(3);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE | OFF_THE_RECORD))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_INCOGNITO));

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        ListItem moveItem = findItemByPluralsId(modelList, R.plurals.move_tab_to_another_window);
        assertNotNull(moveItem);
        int moveIndex = modelList.indexOf(moveItem);

        // Current window (INSTANCE_INFO_1) is filtered because it is the current window.
        // INSTANCE_INFO_INCOGNITO should be shown.
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                moveIndex,
                R.plurals.move_tab_to_another_window,
                Collections.singletonList(INCOGNITO_WINDOW_TITLE),
                mActivity,
                /* isIncognito= */ true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveToWindowSubmenu_regular_filtersIncognitoWindows() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        setupWithIncognito(/* incognito= */ false);
        initializeCoordinator();
        MultiWindowUtils.setInstanceCountForTesting(3);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE | REGULAR))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2));

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        ListItem moveItem = findItemByPluralsId(modelList, R.plurals.move_tab_to_another_window);
        assertNotNull(moveItem);
        int moveIndex = modelList.indexOf(moveItem);

        // Current window (INSTANCE_INFO_1) is filtered because it is the current window.
        // INSTANCE_INFO_2 should be shown.
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                moveIndex,
                R.plurals.move_tab_to_another_window,
                Collections.singletonList(WINDOW_TITLE_2),
                mActivity,
                /* isIncognito= */ false);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveToWindowSubmenu_incognito_allowsMixedWindows() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(false);
        setupWithIncognito(/* incognito= */ true);
        initializeCoordinator();
        MultiWindowUtils.setInstanceCountForTesting(3);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2, INSTANCE_INFO_INCOGNITO));

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        ListItem moveItem = findItemByPluralsId(modelList, R.plurals.move_tab_to_another_window);
        assertNotNull(moveItem);
        int moveIndex = modelList.indexOf(moveItem);

        // Current window (INSTANCE_INFO_1) is filtered because it is the current window.
        // Other windows should not be filtered out since they can accommodate both incognito and
        // regular tabs.
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                moveIndex,
                R.plurals.move_tab_to_another_window,
                List.of(WINDOW_TITLE_2, INCOGNITO_WINDOW_TITLE),
                mActivity,
                /* isIncognito= */ true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveToWindowSubmenu_regular_allowsMixedWindows() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(false);
        setupWithIncognito(/* incognito= */ false);
        initializeCoordinator();
        MultiWindowUtils.setInstanceCountForTesting(3);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2, INSTANCE_INFO_INCOGNITO));

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        ListItem moveItem = findItemByPluralsId(modelList, R.plurals.move_tab_to_another_window);
        assertNotNull(moveItem);
        int moveIndex = modelList.indexOf(moveItem);

        // Current window (INSTANCE_INFO_1) is filtered because it is the current window.
        // Other windows should not be filtered out since they can accommodate both incognito and
        // regular tabs.
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                moveIndex,
                R.plurals.move_tab_to_another_window,
                List.of(WINDOW_TITLE_2, INCOGNITO_WINDOW_TITLE),
                mActivity,
                /* isIncognito= */ false);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @SuppressWarnings("DirectInvocationOnMock")
    public void testMoveToWindowSubmenu_multipleTabs_multipleWindows() {
        MultiWindowUtils.setInstanceCountForTesting(3);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2));

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        List.of(TAB_OUTSIDE_OF_GROUP_ID, TAB_OUTSIDE_OF_GROUP_ID)));

        ListItem moveItem = findItemByPluralsId(modelList, R.plurals.move_tabs_to_another_window);
        assertNotNull(moveItem);
        int moveIndex = modelList.indexOf(moveItem);

        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                moveIndex,
                R.plurals.move_tabs_to_another_window,
                Collections.singletonList(WINDOW_TITLE_2),
                mActivity);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @SuppressWarnings("DirectInvocationOnMock")
    public void testMoveToWindowSubmenu_disabledBelowApi31() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        assertNull(findItemByPluralsId(modelList, R.plurals.move_tab_to_another_window));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @SuppressWarnings("DirectInvocationOnMock")
    public void testMoveToWindowSubmenu_multipleTabs_disabledBelowApi31() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        List.of(TAB_OUTSIDE_OF_GROUP_ID, TAB_OUTSIDE_OF_GROUP_ID)));

        assertNull(findItemByPluralsId(modelList, R.plurals.move_tabs_to_another_window));
    }

    @Test
    @Feature("Tab Strip Context Menu")
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
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));
        ListItem moveToWindowItem =
                findItemByPluralsId(modelList, R.plurals.move_tab_to_another_window);
        assertNotNull(moveToWindowItem);

        var subMenu = moveToWindowItem.model.get(SUBMENU_PROVIDER).get();
        assertEquals("Submenu should have 2 items", 2, subMenu.size());

        ListItem otherWindowItem = subMenu.get(1);
        assertEquals(
                "The title for the other window is incorrect.",
                "My window",
                otherWindowItem.model.get(TITLE));
    }

    @Test
    @Feature("Tab Strip Context Menu")
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
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));
        ListItem moveToWindowItem =
                findItemByPluralsId(modelList, R.plurals.move_tab_to_another_window);
        assertNotNull(moveToWindowItem);

        var subMenu = moveToWindowItem.model.get(SUBMENU_PROVIDER).get();
        assertEquals("Submenu should have 2 items", 2, subMenu.size());

        ListItem otherWindowItem = subMenu.get(1);
        assertEquals(
                "The title for the other window is incorrect.",
                "Example",
                otherWindowItem.model.get(TITLE));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveToWindow_oneInstance_allTabsSelected_singleTab() {
        MultiWindowUtils.setInstanceCountForTesting(1);

        mTabModel.addTab(
                mTabOutsideOfGroup,
                -1,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        assertEquals("Tab model should have one tab.", 1, mTabModel.getCount());

        // Set total tab count to be equal to move tab count (1).
        mTotalTabCountSupplier.set(1);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        // With only one window and all tabs selected, the "move to window" option should not show.
        assertNull(findItemByPluralsId(modelList, R.plurals.move_tab_to_another_window));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveToWindow_oneInstance_allTabsSelected_multipleTabs() {
        MultiWindowUtils.setInstanceCountForTesting(1);

        mTabModel.addTab(
                mTabOutsideOfGroup,
                -1,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mTab2, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        assertEquals("Tab model should have two tabs.", 2, mTabModel.getCount());

        // Set total tab count to be equal to move tab count (2).
        mTotalTabCountSupplier.set(2);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID, List.of(TAB_OUTSIDE_OF_GROUP_ID, TAB_ID_2)));

        // With only one window and all tabs selected, the "move to window" option should not show.
        assertNull(findItemByPluralsId(modelList, R.plurals.move_tabs_to_another_window));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveToWindowSubmenu_hasSubmenuWhenMultipleWindows() {
        MultiWindowUtils.setInstanceCountForTesting(2);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2));
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        ListItem moveItem = findItemByPluralsId(modelList, R.plurals.move_tab_to_another_window);
        assertNotNull(moveItem);
        int moveIndex = modelList.indexOf(moveItem);

        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                moveIndex,
                R.plurals.move_tab_to_another_window,
                List.of(WINDOW_TITLE_2),
                mActivity);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveTabLeft() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        String moveLeftTitle =
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 1);
        ListItem moveLeftItem = findItemByTitle(modelList, moveLeftTitle);
        assertNotNull("Move left item should be present", moveLeftItem);
        moveLeftItem.model.get(CLICK_LISTENER).onClick(mView);

        verify(mReorderFunction, times(1)).accept(new AnchorInfo(TAB_ID, List.of(TAB_ID)), true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveTabLeft_firstTab() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        assertNull(
                "Did not expect any item to have 'Move left' title",
                findItemByTitle(
                        modelList,
                        mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 1)));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveTabRight() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        String moveRightTitle =
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_right, 1);
        ListItem moveRightItem = findItemByTitle(modelList, moveRightTitle);
        assertNotNull("Move right item should be present", moveRightItem);
        moveRightItem.model.get(CLICK_LISTENER).onClick(mView);

        verify(mReorderFunction, times(1)).accept(new AnchorInfo(TAB_ID, List.of(TAB_ID)), false);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveTabRight_lastTab() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(2);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        assertNull(
                "Did not expect any item to have 'Move right' title",
                findItemByTitle(
                        modelList,
                        mActivity.getResources().getQuantityString(R.plurals.move_tabs_right, 1)));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveTabStart_RTL() {
        LocalizationUtils.setRtlForTesting(true);
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        String moveRightTitle =
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_right, 1);
        ListItem moveRightItem = findItemByTitle(modelList, moveRightTitle);
        assertNotNull("Move right item should be present", moveRightItem);
        moveRightItem.model.get(CLICK_LISTENER).onClick(mView);
        verify(mReorderFunction, times(1)).accept(new AnchorInfo(TAB_ID, List.of(TAB_ID)), false);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveTabStart_firstTab_RTL() {
        LocalizationUtils.setRtlForTesting(true);
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        // In RTL, moving toward the start is "Move right". This option should not be available for
        // the first tab.
        assertNull(
                "Did not expect any item to have 'Move right' title",
                findItemByTitle(
                        modelList,
                        mActivity.getResources().getQuantityString(R.plurals.move_tabs_right, 1)));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveTabEnd_RTL() {
        LocalizationUtils.setRtlForTesting(true);
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        String moveLeftTitle =
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 1);
        ListItem moveLeftItem = findItemByTitle(modelList, moveLeftTitle);
        assertNotNull("Move left item should be present", moveLeftItem);
        moveLeftItem.model.get(CLICK_LISTENER).onClick(mView);

        verify(mReorderFunction, times(1)).accept(new AnchorInfo(TAB_ID, List.of(TAB_ID)), true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveTabEnd_lastTab_RTL() {
        LocalizationUtils.setRtlForTesting(true);
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(2);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        // In RTL, moving toward the end is "Move left". This option should not be available for
        // the last tab.
        assertNull(
                "Did not expect any item to have 'Move left' title",
                findItemByTitle(
                        modelList,
                        mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 1)));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveTabLeft_firstUnpinnedTab() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        assertNull(
                "Expected no 'Move left' title if tab to the left is pinned",
                findItemByTitle(
                        modelList,
                        mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 1)));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveTabRight_pinnedTab() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTab1.getIsPinned()).thenReturn(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        assertNull(
                "Did not expect pinned tab menu to have 'Move left' title",
                findItemByTitle(
                        modelList,
                        mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 1)));
        assertNull(
                "Did not expect pinned tab menu to have 'Move right' title",
                findItemByTitle(
                        modelList,
                        mActivity.getResources().getQuantityString(R.plurals.move_tabs_right, 1)));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveTabLeft_unpinnedTab() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(2);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        String moveLeftTitle =
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 1);
        ListItem moveLeftItem = findItemByTitle(modelList, moveLeftTitle);
        assertNotNull("Move left item should be present", moveLeftItem);
        moveLeftItem.model.get(CLICK_LISTENER).onClick(mView);

        verify(mReorderFunction, times(1)).accept(new AnchorInfo(TAB_ID, List.of(TAB_ID)), true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveTabsLeft() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.indexOf(mTab2)).thenReturn(2);
        when(mTabModel.getCount()).thenReturn(4);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)));

        String moveLeftTitle =
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 2);
        ListItem moveLeftItem = findItemByTitle(modelList, moveLeftTitle);
        assertNotNull("Move left item should be present", moveLeftItem);
        moveLeftItem.model.get(CLICK_LISTENER).onClick(mView);

        verify(mReorderFunction, times(1))
                .accept(new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)), true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveTabsRight() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.indexOf(mTab2)).thenReturn(2);
        when(mTabModel.getCount()).thenReturn(4);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)));

        String moveRightTitle =
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_right, 2);
        ListItem moveRightItem = findItemByTitle(modelList, moveRightTitle);
        assertNotNull("Move right item should be present", moveRightItem);
        moveRightItem.model.get(CLICK_LISTENER).onClick(mView);

        verify(mReorderFunction, times(1))
                .accept(new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)), false);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testAccessibilityMoveOptions_visibleForSingleTab() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);

        var modelList = new ModelList();
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        String expectedMoveStartTitle =
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 1);
        ListItem moveStartItem = findItemByTitle(modelList, expectedMoveStartTitle);
        assertNotNull("Move toward start item should be visible", moveStartItem);

        String expectedMoveEndTitle =
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_right, 1);
        ListItem moveEndItem = findItemByTitle(modelList, expectedMoveEndTitle);
        assertNotNull("Move toward end item should be visible", moveEndItem);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testAccessibilityMoveOptions_visibleForSingleTab_RTL() {
        LocalizationUtils.setRtlForTesting(true);
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);

        var modelList = new ModelList();
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        String expectedMoveStartTitle =
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_right, 1);
        ListItem moveStartItem = findItemByTitle(modelList, expectedMoveStartTitle);
        assertNotNull("Move toward start item should be visible", moveStartItem);

        String expectedMoveEndTitle =
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 1);
        ListItem moveEndItem = findItemByTitle(modelList, expectedMoveEndTitle);
        assertNotNull("Move toward end item should be visible", moveEndItem);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testAccessibilityMoveOptions_incognitoAppearance() {
        setupWithIncognito(/* incognito= */ true);
        initializeCoordinator();
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);

        var modelList = new ModelList();
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        String expectedMoveStartTitle =
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 1);
        ListItem moveStartItem = findItemByTitle(modelList, expectedMoveStartTitle);
        assertNotNull(moveStartItem);
        assertEquals(
                "Expected text appearance ID to be set to"
                        + " R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light"
                        + " in incognito",
                R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light,
                moveStartItem.model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));
        assertEquals(
                "Expected icon tint to be set to R.color.default_icon_color_light_tint_list in"
                        + " incognito",
                R.color.default_icon_color_light_tint_list,
                moveStartItem.model.get(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID));

        String expectedMoveEndTitle =
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_right, 1);
        ListItem moveEndItem = findItemByTitle(modelList, expectedMoveEndTitle);
        assertNotNull(moveEndItem);
        assertEquals(
                "Expected text appearance ID to be set to"
                        + " R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light"
                        + " in incognito",
                R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light,
                moveEndItem.model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));
        assertEquals(
                "Expected icon tint to be set to R.color.default_icon_color_light_tint_list in"
                        + " incognito",
                R.color.default_icon_color_light_tint_list,
                moveEndItem.model.get(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testAccessibilityMoveOptions_visibleForMultipleTabs() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);

        var modelList = new ModelList();
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.indexOf(mTab2)).thenReturn(2);
        when(mTabModel.getCount()).thenReturn(4);
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)));

        String expectedMoveStartTitle =
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 2);
        ListItem moveStartItem = findItemByTitle(modelList, expectedMoveStartTitle);
        assertNotNull("Move toward start item should be visible", moveStartItem);

        String expectedMoveEndTitle =
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_right, 2);
        ListItem moveEndItem = findItemByTitle(modelList, expectedMoveEndTitle);
        assertNotNull("Move toward end item should be visible", moveEndItem);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @DisableFeatures(ChromeFeatureList.ANDROID_CONTEXT_MENU_NEW_ACTIONS)
    public void testShareUrl() {
        mOnItemClickedCallback.onClick(
                R.id.share_tab,
                new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)),
                COLLABORATION_ID,
                /* listViewTouchTracker= */ null);
        verify(mShareDelegate, times(1)).share(mTab1, false, TAB_STRIP_CONTEXT_MENU);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @SuppressWarnings("DirectInvocationOnMock")
    @DisableFeatures(ChromeFeatureList.ANDROID_CONTEXT_MENU_NEW_ACTIONS)
    public void testShareTab_hiddenForNonShareableUrl() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(NON_URL_TAB_ID, Collections.singletonList(NON_URL_TAB_ID)));

        assertNull(findItemByMenuId(modelList, R.id.share_tab));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testAddTabToReadingList() {
        mOnItemClickedCallback.onClick(
                R.id.add_tab_to_reading_list_menu_id,
                new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)),
                COLLABORATION_ID,
                /* listViewTouchTracker= */ null);
        verify(mTabBookmarker, times(1)).addToReadingList(Collections.singletonList(mTab1));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testAddMultipleTabsToReadingList() {
        mOnItemClickedCallback.onClick(
                R.id.add_tab_to_reading_list_menu_id,
                new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)),
                COLLABORATION_ID,
                /* listViewTouchTracker= */ null);
        verify(mTabBookmarker, times(1)).addToReadingList(List.of(mTab1, mTab2));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testAddTabToReadingList_HiddenInIncognito() {
        setupWithIncognito(/* incognito= */ true);
        initializeCoordinator();
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        assertNull(
                "Reading List action should be hidden in Incognito Mode",
                findItemByMenuId(modelList, R.id.add_tab_to_reading_list_menu_id));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testDuplicateTab_singleTab() {
        mOnItemClickedCallback.onClick(
                R.id.duplicate_tab_menu_id,
                new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)),
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabModel, times(1)).duplicateTab(mTab1);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testDuplicateTabs_multipleTabs() {
        doReturn(null).when(mTabModel).duplicateTab(any());
        mOnItemClickedCallback.onClick(
                R.id.duplicate_tab_menu_id,
                new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)),
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabModel, times(1)).duplicateTab(mTab1);
        verify(mTabModel, times(1)).duplicateTab(mTab2);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testSendToYourDevices() {
        SendTabToSelfCoordinator mockSttsCoordinator = Mockito.mock(SendTabToSelfCoordinator.class);
        TabContextMenuCoordinator.setSendTabToSelfCreatorForTesting(
                (context,
                        window,
                        url,
                        title,
                        bsc,
                        profile,
                        dl,
                        tabSupplier,
                        act,
                        launcher,
                        tracker,
                        mdm,
                        sm) -> {
                    assertEquals(EXAMPLE_URL.getSpec(), url);
                    assertEquals(mTab1, tabSupplier.get());
                    return mockSttsCoordinator;
                });

        // Mock BottomSheetController retrieval
        BottomSheetController mockBottomSheetController = Mockito.mock(BottomSheetController.class);
        BottomSheetControllerProvider.setInstanceForTesting(mockBottomSheetController);

        // Mock UnownedUserDataHost and bind DeviceLockActivityLauncher
        UnownedUserDataHost unownedUserDataHost = new UnownedUserDataHost();
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(unownedUserDataHost);
        DeviceLockActivityLauncher mockDeviceLockActivityLauncher =
                Mockito.mock(DeviceLockActivityLauncher.class);
        DeviceLockActivityLauncherSupplier.attach(
                unownedUserDataHost,
                ObservableSuppliers.createMonotonic(mockDeviceLockActivityLauncher));

        // Click on the menu action item
        mOnItemClickedCallback.onClick(
                R.id.send_to_your_devices_menu_id,
                new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)),
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);

        verify(mockSttsCoordinator, times(1)).show();
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testSendToYourDevicesMenuItem_isNotShownWhenDisplayReasonNull() {
        when(mSendTabToSelfAndroidBridgeNatives.getEntryPointDisplayReason(any(), any()))
                .thenReturn(null);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        assertNull(
                "Send to Your Devices menu item should not be present when displayReason is null",
                findItemByMenuId(modelList, R.id.send_to_your_devices_menu_id));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @SuppressWarnings("DirectInvocationOnMock")
    public void testUnpinTabOption_visibleForPinnedTab() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        var modelList = new ModelList();

        // Pin tab to show unpin option.
        when(mTabOutsideOfGroup.getIsPinned()).thenReturn(true);
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        ListItem unpinItem = findItemByMenuId(modelList, R.id.unpin_tab_menu_id);
        assertNotNull(unpinItem);
        assertEquals(
                mActivity.getResources().getQuantityString(R.plurals.unpin_tabs_menu_item, 1),
                unpinItem.model.get(TITLE));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @SuppressWarnings("DirectInvocationOnMock")
    public void testUnpinTabOption_visibleForMultiplePinnedTabs() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        var modelList = new ModelList();

        // Pin tab to show unpin option.
        when(mTabOutsideOfGroup.getIsPinned()).thenReturn(true);
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        List.of(TAB_OUTSIDE_OF_GROUP_ID, TAB_OUTSIDE_OF_GROUP_ID)));

        ListItem unpinItem = findItemByMenuId(modelList, R.id.unpin_tab_menu_id);
        assertNotNull(unpinItem);
        assertEquals(
                mActivity.getResources().getQuantityString(R.plurals.unpin_tabs_menu_item, 2),
                unpinItem.model.get(TITLE));
    }

    @Test
    public void testMuteSite_singleTab() {
        when(mTabModel.isMuted(mTab1)).thenReturn(false);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, List.of(TAB_ID)));

        ListItem muteItem = findItemByMenuId(modelList, R.id.mute_site_menu_id);
        assertNotNull(muteItem);
        assertEquals(
                mActivity.getResources().getQuantityString(R.plurals.mute_sites_menu_item, 1),
                muteItem.model.get(TITLE));

        mOnItemClickedCallback.onClick(
                R.id.mute_site_menu_id, new AnchorInfo(TAB_ID, List.of(TAB_ID)), null, null);
        verify(mTabModel).setMuteSetting(List.of(mTab1), true);
    }

    @Test
    public void testUnmuteSite_singleTab() {
        when(mTabModel.isMuted(mTab1)).thenReturn(true);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, List.of(TAB_ID)));

        ListItem unmuteItem = findItemByMenuId(modelList, R.id.unmute_site_menu_id);
        assertNotNull(unmuteItem);
        assertEquals(
                mActivity.getResources().getQuantityString(R.plurals.unmute_sites_menu_item, 1),
                unmuteItem.model.get(TITLE));

        mOnItemClickedCallback.onClick(
                R.id.unmute_site_menu_id, new AnchorInfo(TAB_ID, List.of(TAB_ID)), null, null);
        verify(mTabModel).setMuteSetting(List.of(mTab1), false);
    }

    @Test
    public void testMuteSite_multipleTabs() {
        when(mTabModel.isMuted(mTab1)).thenReturn(false);
        when(mTabModel.isMuted(mTab2)).thenReturn(false);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)));

        ListItem muteItem = findItemByMenuId(modelList, R.id.mute_site_menu_id);
        assertNotNull(muteItem);
        assertEquals(
                mActivity.getResources().getQuantityString(R.plurals.mute_sites_menu_item, 2),
                muteItem.model.get(TITLE));

        mOnItemClickedCallback.onClick(
                R.id.mute_site_menu_id,
                new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)),
                null,
                null);
        verify(mTabModel).setMuteSetting(List.of(mTab1, mTab2), true);
    }

    @Test
    public void testUnmuteSite_multipleTabs() {
        when(mTabModel.isMuted(mTab1)).thenReturn(true);
        when(mTabModel.isMuted(mTab2)).thenReturn(true);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)));

        ListItem unmuteItem = findItemByMenuId(modelList, R.id.unmute_site_menu_id);
        assertNotNull(unmuteItem);
        assertEquals(
                mActivity.getResources().getQuantityString(R.plurals.unmute_sites_menu_item, 2),
                unmuteItem.model.get(TITLE));

        mOnItemClickedCallback.onClick(
                R.id.unmute_site_menu_id,
                new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)),
                null,
                null);
        verify(mTabModel).setMuteSetting(List.of(mTab1, mTab2), false);
    }

    @Test
    public void testMuteSite_multipleTabs_mixedState() {
        when(mTabModel.isMuted(mTab1)).thenReturn(true);
        when(mTabModel.isMuted(mTab2)).thenReturn(false);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)));

        ListItem muteItem = findItemByMenuId(modelList, R.id.mute_site_menu_id);
        assertNotNull(muteItem);
        assertEquals(
                mActivity.getResources().getQuantityString(R.plurals.mute_sites_menu_item, 2),
                muteItem.model.get(TITLE));

        mOnItemClickedCallback.onClick(
                R.id.mute_site_menu_id,
                new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)),
                null,
                null);
        verify(mTabModel).setMuteSetting(List.of(mTab1, mTab2), true);
    }

    @Test
    public void testAreAllTabsMuted_earlyReturn() {
        List<Tab> tabs = List.of(mTab1, mTab2);

        when(mTabModel.isMuted(mTab1)).thenReturn(false);
        when(mTabModel.isMuted(mTab2)).thenReturn(true);

        assertFalse(
                "Should return false as the first tab is not muted.",
                mTabContextMenuCoordinator.areAllTabsMuted(tabs));

        // Verify that the check stopped after finding the unmuted tab.
        verify(mTabModel).isMuted(mTab1);
        verify(mTabModel, never()).isMuted(mTab2);
    }

    @Test
    public void testAreAllTabsMuted_IgnoreInvalidTabs() {
        List<Tab> tabs =
                List.of(
                        mTab1,
                        mChromeSchemeTabWithWebContents,
                        mChromeSchemeTabWithoutWebContents,
                        mChromeNativeSchemeTabWithWebContents,
                        mChromeNativeSchemeTabWithoutWebContents,
                        mTab2);

        // Scenario 1: All valid tabs are muted. Invalid tabs have various mute states but
        // should be ignored.
        when(mTabModel.isMuted(mTab1)).thenReturn(true);
        when(mTabModel.isMuted(mTab2)).thenReturn(true);
        when(mTabModel.isMuted(mChromeSchemeTabWithWebContents)).thenReturn(true);
        when(mTabModel.isMuted(mChromeNativeSchemeTabWithWebContents)).thenReturn(true);

        // These shouldn't be called, but we set them to false to be sure they are ignored.
        when(mTabModel.isMuted(mChromeSchemeTabWithoutWebContents)).thenReturn(false);
        when(mTabModel.isMuted(mChromeNativeSchemeTabWithoutWebContents)).thenReturn(false);

        assertTrue(
                "Should return true as all valid tabs are muted, and invalid tabs are ignored.",
                mTabContextMenuCoordinator.areAllTabsMuted(tabs));

        // Verify isMuted is called only for valid tabs.
        verify(mTabModel, times(1)).isMuted(mTab1);
        verify(mTabModel, times(1)).isMuted(mTab2);
        verify(mTabModel, times(1)).isMuted(mChromeSchemeTabWithWebContents);
        verify(mTabModel, times(1)).isMuted(mChromeNativeSchemeTabWithWebContents);
        verify(mTabModel, never()).isMuted(mChromeSchemeTabWithoutWebContents);
        verify(mTabModel, never()).isMuted(mChromeNativeSchemeTabWithoutWebContents);

        Mockito.clearInvocations(mTabModel);

        // Scenario 2: One of the valid tabs is not muted.
        when(mTabModel.isMuted(mTab2)).thenReturn(false);

        assertFalse(
                "Should return false as one of the valid tabs is not muted.",
                mTabContextMenuCoordinator.areAllTabsMuted(tabs));

        // Verify isMuted is called only for valid tabs.
        verify(mTabModel, times(1)).isMuted(mTab1);
        verify(mTabModel, times(1)).isMuted(mTab2);
        verify(mTabModel, times(1)).isMuted(mChromeSchemeTabWithWebContents);
        verify(mTabModel, times(1)).isMuted(mChromeNativeSchemeTabWithWebContents);
        verify(mTabModel, never()).isMuted(mChromeNativeSchemeTabWithoutWebContents);
        verify(mTabModel, never()).isMuted(mChromeSchemeTabWithoutWebContents);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testCloseTab_nullListViewTouchTracker() {
        testCloseTab(/* listViewTouchTracker= */ null, /* shouldAllowUndo= */ true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testCloseTab_clickWithTouch() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker listViewTouchTracker = new FakeListViewTouchTracker();
        listViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createTouchMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        testCloseTab(listViewTouchTracker, /* shouldAllowUndo= */ true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testCloseTab_clickWithMouse() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker listViewTouchTracker = new FakeListViewTouchTracker();
        listViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createMouseMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        testCloseTab(listViewTouchTracker, /* shouldAllowUndo= */ false);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_CONTEXT_MENU_NEW_ACTIONS)
    public void testCloseAllTabs() {
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        ListItem closeAllTabsItem = findItemByMenuId(modelList, R.id.close_all_tabs_menu_id);
        assertNotNull(closeAllTabsItem);
        assertEquals(R.string.menu_close_all_tabs, closeAllTabsItem.model.get(TITLE_ID));

        mOnItemClickedCallback.onClick(
                R.id.close_all_tabs_menu_id,
                new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)),
                COLLABORATION_ID,
                /* listViewTouchTracker= */ null);
        verify(mTabRemover)
                .closeTabs(
                        TabClosureParams.closeAllTabs()
                                .hideTabGroups(true)
                                .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                                .build(),
                        /* allowDialog= */ true);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_CONTEXT_MENU_NEW_ACTIONS)
    public void testCloseAllIncognitoTabs() {
        setupWithIncognito(/* incognito= */ true);
        initializeCoordinator();
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        ListItem closeAllTabsItem =
                findItemByMenuId(modelList, R.id.close_all_incognito_tabs_menu_id);
        assertNotNull(closeAllTabsItem);
        assertEquals(R.string.menu_close_all_incognito_tabs, closeAllTabsItem.model.get(TITLE_ID));

        mOnItemClickedCallback.onClick(
                R.id.close_all_incognito_tabs_menu_id,
                new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)),
                COLLABORATION_ID,
                /* listViewTouchTracker= */ null);
        verify(mTabRemover)
                .closeTabs(
                        TabClosureParams.closeAllTabs()
                                .hideTabGroups(true)
                                .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                                .build(),
                        /* allowDialog= */ true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testCloseOtherTabs_singleTab_hidden() {
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        ListItem closeOtherItem = findItemByMenuId(modelList, R.id.close_other_tabs_menu_id);
        assertNull(closeOtherItem);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testCloseOtherTabs_singleTab() {
        mTabModel.addTab(
                mTab1, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mTab2, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mTabOutsideOfGroup,
                -1,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);

        mOnItemClickedCallback.onClick(
                R.id.close_other_tabs_menu_id,
                new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)),
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);

        verify(mTabRemover)
                .closeTabs(
                        TabClosureParams.closeTabs(List.of(mTab2, mTabOutsideOfGroup))
                                .hideTabGroups(true)
                                .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                                .build(),
                        /* allowDialog= */ true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testCloseOtherTabs_multipleTabs_hidden() {
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)));

        ListItem closeOtherItem = findItemByMenuId(modelList, R.id.close_other_tabs_menu_id);
        assertNull(closeOtherItem);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testCloseOtherTabs_multipleTabs() {
        mTabModel.addTab(
                mTab1, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mTab2, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mTabOutsideOfGroup,
                -1,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);

        mOnItemClickedCallback.onClick(
                R.id.close_other_tabs_menu_id,
                new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)),
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);

        verify(mTabRemover)
                .closeTabs(
                        TabClosureParams.closeTabs(List.of(mTabOutsideOfGroup))
                                .hideTabGroups(true)
                                .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                                .build(),
                        /* allowDialog= */ true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testCloseTabsToTheRight_singleTab_hiddenForLastTab() {
        mTabModel.addTab(
                mTab1, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        assertNull(findItemByMenuId(modelList, R.id.close_tabs_to_the_right_menu_id));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testCloseTabsToTheRight_singleTab() {
        mTabModel.addTab(
                mTab1, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mTab2, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mTabOutsideOfGroup,
                -1,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);

        mOnItemClickedCallback.onClick(
                R.id.close_tabs_to_the_right_menu_id,
                new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)),
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);

        verify(mTabRemover)
                .closeTabs(
                        TabClosureParams.closeTabs(List.of(mTab2, mTabOutsideOfGroup))
                                .hideTabGroups(true)
                                .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                                .build(),
                        /* allowDialog= */ true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testCloseTabsToTheRight_multipleTabs() {
        mTabModel.addTab(
                mTab1, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mTab2, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mTabOutsideOfGroup,
                -1,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);

        mOnItemClickedCallback.onClick(
                R.id.close_tabs_to_the_right_menu_id,
                new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)),
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);

        verify(mTabRemover)
                .closeTabs(
                        TabClosureParams.closeTabs(List.of(mTabOutsideOfGroup))
                                .hideTabGroups(true)
                                .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                                .build(),
                        /* allowDialog= */ true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    @Config(qualifiers = "sw600dp")
    public void testListMenuItems_verticalTabsEligible() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false);

        mTabModel.addTab(
                mTab1,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(
                mTabOutsideOfGroup,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        assertEquals(ListItemType.DIVIDER, modelList.get(9).type);
        ListItem verticalTabsItem = modelList.get(10);
        assertEquals(ListItemType.MENU_ITEM, verticalTabsItem.type);
        assertEquals(
                R.id.show_tabs_vertically_menu_id,
                verticalTabsItem.model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(R.string.show_tabs_vertically, verticalTabsItem.model.get(TITLE_ID));
        assertEquals(ListItemType.DIVIDER, modelList.get(11).type);
    }

    // --------------------------------------------------------------//
    // ----------------------  UTILITY METHODS ----------------------//
    // --------------------------------------------------------------//

    private void verifyAddToGroupSubmenuForTabOutsideOfGroup(
            ModelList modelList,
            String expectedTabGroupName,
            int expectedTabCount,
            boolean isIncognito) {
        int modelListSizeBeforeNav = modelList.size();
        ListItem addToGroupItem =
                findItemByTitle(
                        modelList,
                        mActivity
                                .getResources()
                                .getQuantityString(
                                        R.plurals.add_tab_to_group_menu_item, expectedTabCount));
        assertNotNull("Add to group item should be present", addToGroupItem);
        assertTrue("Expected 'Add to group' item to be enabled", addToGroupItem.model.get(ENABLED));
        var subMenu = addToGroupItem.model.get(SUBMENU_PROVIDER).get();
        assertNotNull("Submenu should be present", subMenu);
        assertEquals(
                "Submenu should have 2 items, but was " + getDebugString(subMenu),
                2,
                subMenu.size());
        addToGroupItem.model.get(CLICK_LISTENER).onClick(mView);
        assertEquals(
                "Expected 3 items to be displayed, but was " + getDebugString(modelList),
                3,
                modelList.size());
        ListItem headerItem = findItemByType(modelList, SUBMENU_HEADER);
        assertNotNull("Submenu back header should be present", headerItem);
        assertEquals(
                "Expected submenu back header to have the same text as submenu parent item",
                mActivity
                        .getResources()
                        .getQuantityString(R.plurals.add_tab_to_group_menu_item, expectedTabCount),
                headerItem.model.get(TITLE));
        assertTrue("Expected back header to be enabled", headerItem.model.get(ENABLED));
        ListItem newGroupItem = findItemByTitleId(modelList, R.string.create_new_group_row_title);
        assertNotNull("New group item should be present in submenu", newGroupItem);
        assertEquals(
                "Expected submenu item for creating a new group to have MENU_ITEM type",
                MENU_ITEM,
                newGroupItem.type);
        assertTrue("Expected New Group item to be enabled", newGroupItem.model.get(ENABLED));

        ListItem tabGroupRowItem = findItemByTitle(modelList, expectedTabGroupName);
        assertNotNull("Tab group row item should be present in submenu", tabGroupRowItem);
        assertEquals(
                "Expected submenu child for existing group to have MENU_ITEM type",
                MENU_ITEM,
                tabGroupRowItem.type);
        PropertyModel tabGroupRowModel = tabGroupRowItem.model;
        assertEquals(
                "Expected 3rd submenu child to contain the tab group identifier",
                expectedTabGroupName,
                tabGroupRowModel.get(TITLE));
        assertEquals(
                "Expected 3rd submenu child to have correct icon width",
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_group_nested_menu_color_icon_size),
                tabGroupRowModel.get(START_ICON_WIDTH));
        InsetDrawable insetDrawable = (InsetDrawable) tabGroupRowModel.get(START_ICON_DRAWABLE);
        GradientDrawable drawable = (GradientDrawable) insetDrawable.getDrawable();
        assertEquals(
                "Expected circle to have correct color",
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                        mActivity, TAB_GROUP_INDICATOR_COLOR_ID, isIncognito),
                drawable.getColor().getDefaultColor());
        assertTrue("Expected tab group row to be enabled", tabGroupRowModel.get(ENABLED));
        headerItem.model.get(CLICK_LISTENER).onClick(mView);
        assertEquals(
                "Expected to navigate back to parent menu",
                modelListSizeBeforeNav,
                modelList.size());
    }

    private void testCloseTab(
            @Nullable ListViewTouchTracker listViewTouchTracker, boolean shouldAllowUndo) {
        mOnItemClickedCallback.onClick(
                R.id.close_tab,
                new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)),
                COLLABORATION_ID,
                listViewTouchTracker);
        verify(mTabRemover, times(1))
                .closeTabs(
                        TabClosureParams.closeTabs(Collections.singletonList(mTab1))
                                .allowUndo(shouldAllowUndo)
                                .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                                .build(),
                        /* allowDialog= */ true);
    }

    private @Nullable ListItem findItemByMenuId(ModelList modelList, int menuId) {
        for (int i = 0; i < modelList.size(); i++) {
            ListItem item = modelList.get(i);
            if (item.type == MENU_ITEM) {
                if (item.model.get(ListMenuItemProperties.MENU_ITEM_ID) == menuId) {
                    return item;
                }
            }
        }
        return null;
    }

    private @Nullable ListItem findItemByType(ModelList modelList, int type) {
        for (int i = 0; i < modelList.size(); i++) {
            ListItem item = modelList.get(i);
            if (item.type == type) {
                return item;
            }
        }
        return null;
    }

    private @Nullable ListItem findItemByTitle(ModelList modelList, String title) {
        for (int i = 0; i < modelList.size(); i++) {
            ListItem item = modelList.get(i);
            if (item.model.containsKey(ListMenuItemProperties.TITLE)
                    && title.equals(item.model.get(ListMenuItemProperties.TITLE))) {
                return item;
            }
        }
        return null;
    }

    private @Nullable ListItem findItemByTitleId(ModelList modelList, @StringRes int titleId) {
        for (int i = 0; i < modelList.size(); i++) {
            ListItem item = modelList.get(i);
            if (item.model.containsKey(ListMenuItemProperties.TITLE_ID)
                    && item.model.get(ListMenuItemProperties.TITLE_ID) == titleId) {
                return item;
            }
        }
        return null;
    }

    private @Nullable ListItem findItemByPluralsId(
            ModelList modelList, @PluralsRes int pluralsRes) {
        String expectedTitleZero = mActivity.getResources().getQuantityString(pluralsRes, 0);
        String expectedTitleOne = mActivity.getResources().getQuantityString(pluralsRes, 1);
        String expectedTitleTwo = mActivity.getResources().getQuantityString(pluralsRes, 2);
        for (int i = 0; i < modelList.size(); i++) {
            ListItem item = modelList.get(i);
            if (item.model.containsKey(TITLE)) {
                CharSequence title = item.model.get(TITLE);
                if (title != null
                        && (expectedTitleZero.contentEquals(title)
                                || expectedTitleOne.contentEquals(title)
                                || expectedTitleTwo.contentEquals(title))) {
                    return item;
                }
            }
        }
        return null;
    }

    private static String getDebugString(ModelList modelList) {
        StringBuilder modelListContents = new StringBuilder();
        for (int i = 0; i < modelList.size(); i++) {
            modelListContents.append(modelList.get(i).type);
            modelListContents.append(" ");
            modelListContents.append(
                    PropertyModel.getFromModelOrDefault(modelList.get(i).model, TITLE, null));
            if (i < modelList.size() - 1) modelListContents.append(", ");
        }
        return modelListContents.toString();
    }

    private static String getDebugString(List<ListItem> items) {
        StringBuilder modelListContents = new StringBuilder();
        for (int i = 0; i < items.size(); i++) {
            modelListContents.append(items.get(i).type);
            modelListContents.append(" ");
            modelListContents.append(
                    PropertyModel.getFromModelOrDefault(items.get(i).model, TITLE, null));
            if (i < items.size() - 1) modelListContents.append(", ");
        }
        return modelListContents.toString();
    }
}

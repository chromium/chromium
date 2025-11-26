// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.multiwindow.InstanceInfo.Type.CURRENT;
import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType.ACTIVE;
import static org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin.TAB_STRIP_CONTEXT_MENU;
import static org.chromium.ui.listmenu.ListItemType.DIVIDER;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM;
import static org.chromium.ui.listmenu.ListItemType.SUBMENU_HEADER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.START_ICON_DRAWABLE;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE_ID;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_ITEMS;
import static org.chromium.ui.listmenu.ListSectionDividerProperties.COLOR_ID;

import android.app.Activity;
import android.graphics.Rect;
import android.graphics.drawable.GradientDrawable;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ListView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator.AnchorInfo;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils.TabGroupCreationCallback;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator.OnItemClickedCallback;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
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
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuItemProperties;
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
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.DATA_SHARING})
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
    @Mock private TabList mTabList;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTabOutsideOfGroup;
    @Mock private Tab mNonUrlTab;
    @Mock private TabRemover mTabRemover;
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private Profile mProfile;
    @Mock private TabGroupListBottomSheetCoordinator mBottomSheetCoordinator;
    @Mock private TabGroupCreationCallback mTabGroupCreationCallback;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private ShareDelegate mShareDelegate;
    @Mock private TabCreator mTabCreator;
    @Mock private WindowAndroid mWindowAndroid;
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
    private Activity mActivity;

    @Before
    public void setUp() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
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
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        when(mTabGroupModelFilterProvider.getTabGroupModelFilter(false))
                .thenReturn(mTabGroupModelFilter);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.getTabUngrouper()).thenReturn(mTabUngrouper);
        when(mTabGroupModelFilter.getAllTabGroupIds()).thenReturn(Set.of(TAB_GROUP_ID));
        when(mTabGroupModelFilter.getTabCountForGroup(TAB_GROUP_ID)).thenReturn(1);
        when(mTabGroupModelFilter.getTabsInGroup(TAB_GROUP_ID))
                .thenReturn(Collections.singletonList(mTab1));
        when(mTabGroupModelFilter.getTabGroupColor(TAB_GROUP_ID))
                .thenReturn(TAB_GROUP_INDICATOR_COLOR_ID);
        when(mTabGroupModelFilter.getTabGroupColorWithFallback(TAB_GROUP_ID))
                .thenReturn(TAB_GROUP_INDICATOR_COLOR_ID);
        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabGroupModelFilter.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(TAB_GROUP_TITLE);
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(INSTANCE_ID_1);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(Collections.singletonList(INSTANCE_INFO_1));

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
                        mTabGroupModelFilter,
                        mBottomSheetCoordinator,
                        mMultiInstanceManager,
                        () -> mShareDelegate);
        mTabContextMenuCoordinator =
                TabContextMenuCoordinator.createContextMenuCoordinator(
                        () -> mTabModel,
                        mTabGroupModelFilter,
                        mBottomSheetCoordinator,
                        mTabGroupCreationCallback,
                        mMultiInstanceManager,
                        () -> mShareDelegate,
                        mWindowAndroid,
                        mActivity,
                        mReorderFunction);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_tabInGroup() {
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        var addToGroupItem = modelList.get(0);
        var subMenu = addToGroupItem.model.get(SUBMENU_ITEMS);
        assertNotNull("Submenu should be present", subMenu);
        assertEquals(
                "Submenu should have 1 item, but was " + getDebugString(subMenu),
                1,
                subMenu.size());

        addToGroupItem.model.get(CLICK_LISTENER).onClick(mView);
        assertEquals(
                "Expected 2 items to be displayed, but was " + getDebugString(modelList),
                2,
                modelList.size());
        ListItem headerItem = modelList.get(0);
        assertEquals(
                "Expected 1st submenu item to be a back header", SUBMENU_HEADER, headerItem.type);
        assertEquals(
                "Expected submenu back header to have the same text as submenu parent item",
                mActivity.getResources().getQuantityString(R.plurals.add_tab_to_group_menu_item, 1),
                headerItem.model.get(TITLE));
        assertEquals(
                "Expected 2nd submenu item to have MENU_ITEM type",
                MENU_ITEM,
                modelList.get(1).type);
        assertEquals(
                "Expected 2nd submenu item to be New Group",
                R.string.create_new_group_row_title,
                modelList.get(1).model.get(TITLE_ID));
        headerItem.model.get(CLICK_LISTENER).onClick(mView);
        assertEquals("Expected to navigate back to parent menu", 5, modelList.size());

        // List item 2
        assertEquals(
                mActivity
                        .getResources()
                        .getQuantityString(R.plurals.remove_tabs_from_group_menu_item, 1),
                modelList.get(1).model.get(TITLE));
        assertEquals(
                R.id.remove_from_tab_group,
                modelList.get(1).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 3
        assertEquals(DIVIDER, modelList.get(2).type);
        assertEquals(
                "Expected divider to have have COLOR_ID unset when not in incognito mode",
                0,
                modelList.get(2).model.get(COLOR_ID));

        // List item 4
        assertEquals(R.string.share, modelList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.share_tab, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 5
        assertEquals(R.string.close, modelList.get(4).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(4).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testListMenuItems_submenuCreateNewTabGroup() {
        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        // Add to group submenu
        ListItem addToGroupItem = modelList.get(0);
        List<ListItem> subMenu = addToGroupItem.model.get(SUBMENU_ITEMS);
        assertNotNull("Submenu should be present", subMenu);
        addToGroupItem.model.get(CLICK_LISTENER).onClick(mView);

        // Add to new group
        ListItem addToNewGroupItem = modelList.get(1);
        assertEquals(
                "Expected 2nd submenu item to be New Group",
                R.string.create_new_group_row_title,
                addToNewGroupItem.model.get(TITLE_ID));
        addToNewGroupItem.model.get(CLICK_LISTENER).onClick(mView);

        // Verify
        verify(mTabGroupCreationCallback).onTabGroupCreated(any());
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures({
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP,
        ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING
    })
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_tabInGroup_multipleTabs() {
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, List.of(TAB_ID, NON_URL_TAB_ID)));

        assertEquals("Number of items in the list menu is incorrect", 4, modelList.size());

        // List item 1
        var addToGroupItem = modelList.get(0);
        var subMenu = addToGroupItem.model.get(SUBMENU_ITEMS);
        assertNotNull("Submenu should be present", subMenu);
        assertEquals(
                "Submenu should have 1 item, but was " + getDebugString(subMenu),
                1,
                subMenu.size());

        addToGroupItem.model.get(CLICK_LISTENER).onClick(mView);
        assertEquals(
                "Expected 2 items to be displayed, but was " + getDebugString(modelList),
                2,
                modelList.size());
        ListItem headerItem = modelList.get(0);
        assertEquals(
                "Expected 1st submenu item to be a back header", SUBMENU_HEADER, headerItem.type);
        assertEquals(
                "Expected submenu back header to have the same text as submenu parent item",
                mActivity.getResources().getQuantityString(R.plurals.add_tab_to_group_menu_item, 2),
                headerItem.model.get(TITLE));
        assertEquals(
                "Expected 2nd submenu item to have MENU_ITEM type",
                MENU_ITEM,
                modelList.get(1).type);
        assertEquals(
                "Expected 2nd submenu item to be New Group",
                R.string.create_new_group_row_title,
                modelList.get(1).model.get(TITLE_ID));
        headerItem.model.get(CLICK_LISTENER).onClick(mView);
        assertEquals("Expected to navigate back to parent menu", 4, modelList.size());

        // List item 2
        assertEquals(
                mActivity
                        .getResources()
                        .getQuantityString(R.plurals.remove_tabs_from_group_menu_item, 2),
                modelList.get(1).model.get(TITLE));
        assertEquals(
                R.id.remove_from_tab_group,
                modelList.get(1).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 3
        assertEquals(DIVIDER, modelList.get(2).type);
        assertEquals(
                "Expected divider to have have COLOR_ID unset when not in incognito mode",
                0,
                modelList.get(2).model.get(COLOR_ID));

        // List item 4
        assertEquals(R.string.close, modelList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_tabOutsideOfGroup() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE, 1);

        // List item 2
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList, 1, R.plurals.move_tab_to_another_window, List.of(), mActivity);

        // List item 3
        assertEquals(DIVIDER, modelList.get(2).type);

        // List item 4
        assertEquals(R.string.share, modelList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.share_tab, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 5
        assertEquals(R.string.close, modelList.get(4).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(4).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures({
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP,
        ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING
    })
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_tabOutsideOfGroup_multipleTabs() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        List.of(TAB_OUTSIDE_OF_GROUP_ID, TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 4, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE, 2);

        // List item 2
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList, 1, R.plurals.move_tabs_to_another_window, List.of(), mActivity);

        // List item 3
        assertEquals(DIVIDER, modelList.get(2).type);

        // List item 4
        assertEquals(R.string.close, modelList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    @SuppressWarnings("DirectInvocationOnMock")
    public void testAddToGroupSubmenu_fallbackTabGroupName() {
        when(mTabGroupModelFilter.getTabGroupTitle(TAB_GROUP_ID)).thenReturn("");
        MultiWindowUtils.setInstanceCountForTesting(1);
        mSavedTabGroup.title = "";
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, "1 tab", 1);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    @SuppressWarnings("DirectInvocationOnMock")
    public void testAddToGroupSubmenu_fallbackTabGroupName_incognito() {
        setupWithIncognito(true);
        initializeCoordinator();
        when(mTabGroupModelFilter.getTabGroupTitle(TAB_GROUP_ID)).thenReturn("");
        MultiWindowUtils.setInstanceCountForTesting(1);
        mSavedTabGroup.title = "";
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, "1 tab", 1);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_tabOutsideOfGroup_multipleWindows() {
        MultiWindowUtils.setInstanceCountForTesting(3);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2));

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE, 1);

        // List item 2
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                1,
                R.plurals.move_tab_to_another_window,
                Collections.singletonList(WINDOW_TITLE_2),
                mActivity);

        // List item 3
        assertEquals(DIVIDER, modelList.get(2).type);

        // List item 4
        assertEquals(R.string.share, modelList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.share_tab, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 5
        assertEquals(R.string.close, modelList.get(4).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(4).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures({
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP,
        ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING
    })
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_tabOutsideOfGroup_multipleWindows_multipleTabs() {
        MultiWindowUtils.setInstanceCountForTesting(3);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2));

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        List.of(TAB_OUTSIDE_OF_GROUP_ID, TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 4, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE, 2);

        // List item 2
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                1,
                R.plurals.move_tabs_to_another_window,
                Collections.singletonList(WINDOW_TITLE_2),
                mActivity);

        // List item 3
        assertEquals(DIVIDER, modelList.get(2).type);

        // List item 4
        assertEquals(R.string.close, modelList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_belowApi31() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 4, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE, 1);

        // List item 2
        assertEquals(DIVIDER, modelList.get(1).type);

        // List item 3
        assertEquals(R.string.share, modelList.get(2).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.share_tab, modelList.get(2).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 4
        assertEquals(R.string.close, modelList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures({
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP,
        ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING
    })
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_belowApi31_multipleTabs() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        List.of(TAB_OUTSIDE_OF_GROUP_ID, TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 3, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE, 2);

        // List item 2
        assertEquals(DIVIDER, modelList.get(1).type);

        // List item 3
        assertEquals(R.string.close, modelList.get(2).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(2).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_nonShareableUrl() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(NON_URL_TAB_ID, Collections.singletonList(NON_URL_TAB_ID)));

        assertEquals("Number of items in the list menu is incorrect", 4, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE, 1);

        // List item 2
        verifyAddToWindowSubmenu(modelList, List.of());

        // List item 3
        assertEquals(DIVIDER, modelList.get(2).type);

        // List item 4
        assertEquals(R.string.close, modelList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_incognito() {
        setupWithIncognito(/* incognito= */ true);
        initializeCoordinator();
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE, 1);

        // List item 2
        assertEquals(DIVIDER, modelList.get(2).type);
        assertEquals(
                "Expected divider to have COLOR_ID set to R.color.divider_line_bg_color_light in"
                        + " incognito mode",
                R.color.divider_line_bg_color_light,
                modelList.get(2).model.get(COLOR_ID));

        // List item 3
        assertEquals(R.string.share, modelList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.share_tab, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                "Expected text appearance ID to be set to"
                    + " R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light in"
                    + " incognito",
                R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light,
                modelList.get(3).model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));

        // List item 4
        assertEquals(R.string.close, modelList.get(4).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(4).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                "Expected text appearance ID to be set to"
                    + " R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light in"
                    + " incognito",
                R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light,
                modelList.get(4).model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP,
        ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING
    })
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_incognito_multipleTabs() {
        setupWithIncognito(/* incognito= */ true);
        initializeCoordinator();
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        List.of(TAB_OUTSIDE_OF_GROUP_ID, TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 4, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE, 2);

        // List item 2
        assertEquals(DIVIDER, modelList.get(2).type);
        assertEquals(
                "Expected divider to have COLOR_ID set to R.color.divider_line_bg_color_light in"
                        + " incognito mode",
                R.color.divider_line_bg_color_light,
                modelList.get(2).model.get(COLOR_ID));

        // List item 3
        assertEquals(R.string.close, modelList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                "Expected text appearance ID to be set to"
                    + " R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light in"
                    + " incognito",
                R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light,
                modelList.get(3).model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures({
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP,
        ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP
    })
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_tabOutsideOfGroup_pinnedTabs_showPinTabOption() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 6, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE, 1);

        // List item 2
        verifyAddToWindowSubmenu(modelList, List.of());

        // List item 3
        assertEquals(DIVIDER, modelList.get(2).type);

        // List item 4
        assertEquals(R.string.share, modelList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.share_tab, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 5
        assertEquals(
                mActivity.getResources().getQuantityString(R.plurals.pin_tabs_menu_item, 1),
                modelList.get(4).model.get(TITLE));
        assertEquals(
                R.id.pin_tab_menu_id,
                modelList.get(4).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 6
        assertEquals(R.string.close, modelList.get(5).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(5).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures({
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP,
        ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP,
        ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING
    })
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_tabOutsideOfGroup_pinnedTabs_showPinTabOption_multipleTabs() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        List.of(TAB_OUTSIDE_OF_GROUP_ID, TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE, 2);

        // List item 2
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList, 1, R.plurals.move_tabs_to_another_window, List.of(), mActivity);

        // List item 3
        assertEquals(DIVIDER, modelList.get(2).type);

        // List item 4
        assertEquals(
                mActivity.getResources().getQuantityString(R.plurals.pin_tabs_menu_item, 2),
                modelList.get(3).model.get(TITLE));
        assertEquals(
                R.id.pin_tab_menu_id,
                modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 5
        assertEquals(R.string.close, modelList.get(4).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(4).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures({
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP,
        ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP
    })
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_tabOutsideOfGroup_pinnedTabs_showUnpinTabOption() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        var modelList = new ModelList();

        // Pin tab to show unpin option.
        when(mTabOutsideOfGroup.getIsPinned()).thenReturn(true);
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        Collections.singletonList(TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 6, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE, 1);

        // List item 2
        verifyAddToWindowSubmenu(modelList, List.of());

        // List item 3
        assertEquals(DIVIDER, modelList.get(2).type);

        // List item 4
        assertEquals(R.string.share, modelList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.share_tab, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 5
        assertEquals(
                mActivity.getResources().getQuantityString(R.plurals.unpin_tabs_menu_item, 1),
                modelList.get(4).model.get(TITLE));
        assertEquals(
                R.id.unpin_tab_menu_id,
                modelList.get(4).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 6
        assertEquals(R.string.close, modelList.get(5).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(5).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures({
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP,
        ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP,
        ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING
    })
    @SuppressWarnings("DirectInvocationOnMock")
    public void testListMenuItems_tabOutsideOfGroup_pinnedTabs_showUnpinTabOption_multipleTabs() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        var modelList = new ModelList();

        // Pin tab to show unpin option.
        when(mTabOutsideOfGroup.getIsPinned()).thenReturn(true);
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList,
                new AnchorInfo(
                        TAB_OUTSIDE_OF_GROUP_ID,
                        List.of(TAB_OUTSIDE_OF_GROUP_ID, TAB_OUTSIDE_OF_GROUP_ID)));

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE, 2);

        // List item 2
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList, 1, R.plurals.move_tabs_to_another_window, List.of(), mActivity);

        // List item 3
        assertEquals(DIVIDER, modelList.get(2).type);

        // List item 4
        assertEquals(
                mActivity.getResources().getQuantityString(R.plurals.unpin_tabs_menu_item, 2),
                modelList.get(3).model.get(TITLE));
        assertEquals(
                R.id.unpin_tab_menu_id,
                modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 5
        assertEquals(R.string.close, modelList.get(4).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(4).model.get(ListMenuItemProperties.MENU_ITEM_ID));
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

    @Test
    @Feature("Tab Strip Context Menu")
    @DisableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testAddToTabGroup_newTabGroup() {
        mOnItemClickedCallback.onClick(
                R.id.add_to_tab_group,
                new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)),
                COLLABORATION_ID,
                /* listViewTouchTracker= */ null);
        verify(mBottomSheetCoordinator, times(1)).showBottomSheet(List.of(mTab1));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
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
        StripLayoutContextMenuCoordinatorTestUtils.clickMoveToNewWindow(modelList, 1, mView);
        verify(mMultiInstanceManager, times(1))
                .moveTabsToNewWindow(
                        Collections.singletonList(mTabOutsideOfGroup), NewWindowAppSource.MENU);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
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

        StripLayoutContextMenuCoordinatorTestUtils.clickMoveToWindowRow(
                modelList, 1, WINDOW_TITLE_2, mView);

        verify(mMultiInstanceManager, times(1))
                .moveTabsToWindow(
                        INSTANCE_INFO_2,
                        Collections.singletonList(mTabOutsideOfGroup),
                        TabList.INVALID_TAB_INDEX,
                        NewWindowAppSource.MENU);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    @SuppressWarnings("DirectInvocationOnMock")
    public void testAnchorWidth() {
        StripLayoutContextMenuCoordinatorTestUtils.testAnchorWidth(
                mWeakReferenceActivity, mTabContextMenuCoordinator::getMenuWidth);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testAnchor_offset() {
        StripLayoutContextMenuCoordinatorTestUtils.testAnchor_offset(
                (rectProvider) ->
                        mTabContextMenuCoordinator.showMenu(
                                rectProvider,
                                new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID))),
                mTabContextMenuCoordinator::destroyMenuForTesting);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testAnchor_offset_incognito() {
        setupWithIncognito(/* incognito= */ true);
        StripLayoutContextMenuCoordinatorTestUtils.testAnchor_offset_incognito(
                (rectProvider) ->
                        mTabContextMenuCoordinator.showMenu(
                                rectProvider,
                                new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID))),
                mTabContextMenuCoordinator::destroyMenuForTesting);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.MEDIA_INDICATORS_ANDROID,
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP
    })
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
    @EnableFeatures({
        ChromeFeatureList.MEDIA_INDICATORS_ANDROID,
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP
    })
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
    @EnableFeatures({
        ChromeFeatureList.MEDIA_INDICATORS_ANDROID,
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP
    })
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
    @EnableFeatures({
        ChromeFeatureList.MEDIA_INDICATORS_ANDROID,
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP
    })
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
    @EnableFeatures({
        ChromeFeatureList.MEDIA_INDICATORS_ANDROID,
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP
    })
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

    private void verifyAddToGroupSubmenuForTabOutsideOfGroup(
            ModelList modelList, String expectedTabGroupName, int expectedTabCount) {
        int modelListSizeBeforeNav = modelList.size();
        var addToGroupItem = modelList.get(0);
        assertTrue("Expected 'Add to group' item to be enabled", addToGroupItem.model.get(ENABLED));
        var subMenu = addToGroupItem.model.get(SUBMENU_ITEMS);
        assertNotNull("Submenu should be present", subMenu);
        assertEquals(
                "Submenu should have 3 items, but was " + getDebugString(subMenu),
                3,
                subMenu.size());
        addToGroupItem.model.get(CLICK_LISTENER).onClick(mView);
        assertEquals(
                "Expected 4 items to be displayed, but was " + getDebugString(modelList),
                4,
                modelList.size());
        ListItem headerItem = modelList.get(0);
        assertEquals(
                "Expected 1st submenu item to be a back header", SUBMENU_HEADER, headerItem.type);
        assertEquals(
                "Expected submenu back header to have the same text as submenu parent item",
                mActivity
                        .getResources()
                        .getQuantityString(R.plurals.add_tab_to_group_menu_item, expectedTabCount),
                headerItem.model.get(TITLE));
        assertTrue("Expected back header to be enabled", headerItem.model.get(ENABLED));
        assertEquals(
                "Expected 2nd submenu item to have MENU_ITEM type",
                MENU_ITEM,
                modelList.get(1).type);
        assertEquals(
                "Expected 2nd submenu item to be New Group",
                R.string.create_new_group_row_title,
                modelList.get(1).model.get(TITLE_ID));
        assertTrue("Expected New Group item to be enabled", modelList.get(1).model.get(ENABLED));
        assertEquals(
                "Expected 3rd submenu item to have DIVIDER type", DIVIDER, modelList.get(2).type);
        assertEquals(
                "Expected 4th submenu child to have MENU_ITEM type",
                MENU_ITEM,
                modelList.get(3).type);
        PropertyModel tabGroupRowModel = modelList.get(3).model;
        assertEquals(
                "Expected 4th submenu child to contain the tab group identifier",
                expectedTabGroupName,
                tabGroupRowModel.get(TITLE));
        GradientDrawable drawable = (GradientDrawable) tabGroupRowModel.get(START_ICON_DRAWABLE);
        assertEquals(
                "Expected circle to have correct color",
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                        mActivity, TAB_GROUP_INDICATOR_COLOR_ID, /* isIncognito= */ false),
                drawable.getColor().getDefaultColor());
        assertTrue("Expected tab group row to be enabled", tabGroupRowModel.get(ENABLED));
        headerItem.model.get(CLICK_LISTENER).onClick(mView);
        assertEquals(
                "Expected to navigate back to parent menu",
                modelListSizeBeforeNav,
                modelList.size());
    }

    private void verifyAddToWindowSubmenu(ModelList modelList, List<String> otherWindowTitles) {
        int modelListSizeBeforeNav = modelList.size();
        var moveToOtherWindowItem = modelList.get(1);
        var subMenu = moveToOtherWindowItem.model.get(SUBMENU_ITEMS);
        int expectedNumberOfItems =
                1 + (otherWindowTitles.isEmpty() ? 0 : 1 + otherWindowTitles.size());
        assertEquals(
                "Submenu should have "
                        + expectedNumberOfItems
                        + " item(s), but was "
                        + getDebugString(subMenu),
                expectedNumberOfItems,
                subMenu.size());
        moveToOtherWindowItem.model.get(CLICK_LISTENER).onClick(mView);
        assertNotNull("Submenu should be present", subMenu);
        assertEquals(
                "Expected to display "
                        + expectedNumberOfItems
                        + 1 // Back header added
                        + " item(s) after entering submenu, but was "
                        + getDebugString(modelList),
                expectedNumberOfItems + 1,
                modelList.size());
        ListItem headerItem = modelList.get(0);
        assertEquals(
                "Expected first item to have SUBMENU_HEADER type", SUBMENU_HEADER, headerItem.type);
        assertEquals(
                "Expected submenu back header to have the same text as submenu parent item",
                mActivity.getResources().getQuantityString(R.plurals.move_tab_to_another_window, 2),
                headerItem.model.get(TITLE));
        assertTrue("Expected submenu header to be enabled", headerItem.model.get(ENABLED));
        assertEquals("Expected 2nd item to have MENU_ITEM type", MENU_ITEM, modelList.get(1).type);
        assertEquals(
                "Expected 2nd item to be 'New window' row",
                R.string.menu_new_window,
                modelList.get(1).model.get(TITLE_ID));
        if (!otherWindowTitles.isEmpty()) {
            assertEquals(
                    "Expected 3rd item to be divider, but was " + getDebugString(modelList),
                    DIVIDER,
                    modelList.get(2).type);
            for (int i = 0; i < otherWindowTitles.size(); i++) {
                assertEquals(
                        "Expected window row at position " + (i + 3) + " to have MENU_ITEM type",
                        MENU_ITEM,
                        modelList.get(1).type);
                assertEquals(
                        "Expected window row at position "
                                + (i + 2)
                                + " to have text "
                                + otherWindowTitles.get(i),
                        otherWindowTitles.get(i),
                        modelList.get(i + 3).model.get(TITLE));
                assertTrue(
                        "Expected window row at position " + (i + 3) + " to be enabled",
                        modelList.get(i + 3).model.get(ENABLED));
            }
        }
        headerItem.model.get(CLICK_LISTENER).onClick(mView);
        assertEquals(
                "Expected to navigate back to parent menu",
                modelListSizeBeforeNav,
                modelList.size());
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

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
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
        var addToGroupItem = modelList.get(0);
        addToGroupItem.model.get(CLICK_LISTENER).onClick(mView);

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
        var headerItem = modelList.get(0);
        headerItem.model.get(CLICK_LISTENER).onClick(mView);

        // Verify that the top item of the parent menu is selected.
        assertEquals(
                "Expected 1st item to be selected after navigating out of submenu",
                0,
                listView.getSelectedItemPosition());

        mTabContextMenuCoordinator.destroyMenuForTesting();
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures({
        ChromeFeatureList.ANDROID_CONTEXT_MENU_DUPLICATE_TABS,
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP
    })
    public void testListMenuItems_singleTab_duplicateTab_featureEnabled() {
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        // Items are: add to group, remove from group, divider, share, duplicate, close.
        assertEquals("Number of items in the list menu is incorrect", 6, modelList.size());
        ListItem duplicateItem = findItemByMenuId(modelList, R.id.duplicate_tab_menu_id);
        assertNotNull(duplicateItem);
        assertEquals(
                mActivity.getResources().getString(R.string.duplicate_tab_menu_item),
                duplicateItem.model.get(ListMenuItemProperties.TITLE));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @DisableFeatures({ChromeFeatureList.ANDROID_CONTEXT_MENU_DUPLICATE_TABS})
    @EnableFeatures({ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP})
    public void testListMenuItems_singleTab_duplicateTab_featureDisabled() {
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        // Items are: add to group, remove from group, divider, share, close.
        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());
        assertNull(findItemByMenuId(modelList, R.id.duplicate_tab_menu_id));
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
    @EnableFeatures({
        ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING,
        ChromeFeatureList.ANDROID_CONTEXT_MENU_DUPLICATE_TABS,
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP
    })
    public void testListMenuItems_multipleTabs_duplicateTabs_featureEnabled() {
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)));

        // Items are: add to group, remove from group, divider, duplicate, close.
        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());
        ListItem duplicateItem = findItemByMenuId(modelList, R.id.duplicate_tab_menu_id);
        assertNotNull(duplicateItem);
        assertEquals(
                mActivity.getResources().getString(R.string.duplicate_tab_menu_item),
                duplicateItem.model.get(ListMenuItemProperties.TITLE));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures({
        ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING,
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP
    })
    @DisableFeatures({ChromeFeatureList.ANDROID_CONTEXT_MENU_DUPLICATE_TABS})
    public void testListMenuItems_multipleTabs_duplicateTabs_featureDisabled() {
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)));

        // Items are: add to group, remove from group, divider, duplicate, close.
        assertEquals("Number of items in the list menu is incorrect", 4, modelList.size());
        assertNull(findItemByMenuId(modelList, R.id.duplicate_tab_menu_id));
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
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testListMenuItems_moveTabItems_accessibilityOn() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);

        var modelList = new ModelList();
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        assertEquals("Number of items in the list menu is incorrect", 7, modelList.size());

        // Items are: add to group, move to window, move left, move right, divider, share, close.
        ListItem moveStartItem = modelList.get(2);
        String moveStartTitle =
                String.valueOf(moveStartItem.model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                "Move toward start item has wrong title",
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 1),
                moveStartTitle);

        ListItem moveEndItem = modelList.get(3);
        String moveEndTitle = String.valueOf(moveEndItem.model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                "Move toward end item has wrong title",
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_right, 1),
                moveEndTitle);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testListMenuItems_moveTabItems_accessibilityOn_RTL() {
        LocalizationUtils.setRtlForTesting(true);
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);

        var modelList = new ModelList();
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        assertEquals("Number of items in the list menu is incorrect", 7, modelList.size());

        // Items are: add to group, move to window, move start, move end, divider, share, close.
        ListItem moveStartItem = modelList.get(2);
        String moveStartTitle =
                String.valueOf(moveStartItem.model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                "Move toward start item has wrong title",
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_right, 1),
                moveStartTitle);

        ListItem moveEndItem = modelList.get(3);
        String moveEndTitle = String.valueOf(moveEndItem.model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                "Move toward end item has wrong title",
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 1),
                moveEndTitle);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveTabLeft() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        modelList.get(2).model.get(CLICK_LISTENER).onClick(mView);

        verify(mReorderFunction, times(1)).accept(new AnchorInfo(TAB_ID, List.of(TAB_ID)), true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveTabLeft_firstTab() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        for (ListItem listItem : modelList) {
            if (!listItem.model.containsKey(TITLE)) continue;
            assertNotEquals(
                    "Did not expect any item to have 'Move left' title",
                    mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 1),
                    listItem.model.get(TITLE));
        }
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveTabRight() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        modelList.get(3).model.get(CLICK_LISTENER).onClick(mView);

        verify(mReorderFunction, times(1)).accept(new AnchorInfo(TAB_ID, List.of(TAB_ID)), false);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveTabRight_lastTab() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(2);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        for (ListItem listItem : modelList) {
            if (!listItem.model.containsKey(TITLE)) continue;
            assertNotEquals(
                    "Did not expect any item to have 'Move right' title",
                    mActivity.getResources().getQuantityString(R.plurals.move_tabs_right, 1),
                    listItem.model.get(TITLE));
        }
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveTabStart_RTL() {
        LocalizationUtils.setRtlForTesting(true);
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        // In RTL, the item to move toward the start is visually "Move right". It's at the same
        // position as "Move left" in LTR.
        modelList.get(2).model.get(CLICK_LISTENER).onClick(mView);
        verify(mReorderFunction, times(1)).accept(new AnchorInfo(TAB_ID, List.of(TAB_ID)), false);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
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
        for (ListItem listItem : modelList) {
            if (!listItem.model.containsKey(TITLE)) continue;
            assertNotEquals(
                    "Did not expect any item to have 'Move right' title",
                    mActivity.getResources().getQuantityString(R.plurals.move_tabs_right, 1),
                    listItem.model.get(TITLE));
        }
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveTabEnd_RTL() {
        LocalizationUtils.setRtlForTesting(true);
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        // In RTL, the item to move toward the end is visually "Move left". It's at the same
        // position as "Move right" in LTR.
        modelList.get(3).model.get(CLICK_LISTENER).onClick(mView);

        verify(mReorderFunction, times(1)).accept(new AnchorInfo(TAB_ID, List.of(TAB_ID)), true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
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
        for (ListItem listItem : modelList) {
            if (!listItem.model.containsKey(TITLE)) continue;
            assertNotEquals(
                    "Did not expect any item to have 'Move left' title",
                    mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 1),
                    listItem.model.get(TITLE));
        }
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveTabLeft_firstUnpinnedTab() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        for (ListItem listItem : modelList) {
            if (!listItem.model.containsKey(TITLE)) continue;
            assertNotEquals(
                    "Expected no 'Move left' title if tab to the left is pinned",
                    mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 1),
                    listItem.model.get(TITLE));
        }
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveTabRight_pinnedTab() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTab1.getIsPinned()).thenReturn(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        for (ListItem listItem : modelList) {
            if (!listItem.model.containsKey(TITLE)) continue;
            assertNotEquals(
                    "Did not expect pinned tab menu to have 'Move left' title",
                    mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 1),
                    listItem.model.get(TITLE));
            assertNotEquals(
                    "Did not expect pinned tab menu to have 'Move right' title",
                    mActivity.getResources().getQuantityString(R.plurals.move_tabs_right, 1),
                    listItem.model.get(TITLE));
        }
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveTabLeft_unpinnedTab() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(2);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(1);
        when(mTabModel.getCount()).thenReturn(3);

        ModelList modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, Collections.singletonList(TAB_ID)));

        modelList.get(2).model.get(CLICK_LISTENER).onClick(mView);

        verify(mReorderFunction, times(1)).accept(new AnchorInfo(TAB_ID, List.of(TAB_ID)), true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures({
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP,
        ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING
    })
    public void testListMenuItems_moveTabsItems_accessibilityOn() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);

        var modelList = new ModelList();
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.indexOf(mTab2)).thenReturn(2);
        when(mTabModel.getCount()).thenReturn(4);
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)));
        assertEquals("Number of items in the list menu is incorrect", 6, modelList.size());

        // Items are: add to group, move to window, move left, move right, divider, pin, close.
        ListItem moveStartItem = modelList.get(2);
        String moveStartTitle =
                String.valueOf(moveStartItem.model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                "Move toward start item has wrong title",
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_left, 2),
                moveStartTitle);

        ListItem moveEndItem = modelList.get(3);
        String moveEndTitle = String.valueOf(moveEndItem.model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                "Move toward end item has wrong title",
                mActivity.getResources().getQuantityString(R.plurals.move_tabs_right, 2),
                moveEndTitle);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures({
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP,
        ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING
    })
    public void testMoveTabsLeft() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.indexOf(mTab2)).thenReturn(2);
        when(mTabModel.getCount()).thenReturn(4);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)));

        modelList.get(2).model.get(CLICK_LISTENER).onClick(mView);

        verify(mReorderFunction, times(1))
                .accept(new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)), true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures({
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP,
        ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING
    })
    public void testMoveTabsRight() {
        mTabContextMenuCoordinator.setIsGesturesEnabledForTesting(true);
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.indexOf(mTab2)).thenReturn(2);
        when(mTabModel.getCount()).thenReturn(4);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(
                modelList, new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)));

        modelList.get(3).model.get(CLICK_LISTENER).onClick(mView);

        verify(mReorderFunction, times(1))
                .accept(new AnchorInfo(TAB_ID, List.of(TAB_ID, TAB_ID_2)), false);
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
}

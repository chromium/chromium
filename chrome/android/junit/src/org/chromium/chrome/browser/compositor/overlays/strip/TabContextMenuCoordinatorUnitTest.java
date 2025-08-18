// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
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
import android.graphics.drawable.GradientDrawable;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

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
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabList;
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
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link TabContextMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.DATA_SHARING})
public class TabContextMenuCoordinatorUnitTest {
    private static final int TAB_ID = 1;
    private static final int TAB_OUTSIDE_OF_GROUP_ID = 2;
    private static final int NON_URL_TAB_ID = 3;
    private static final Token TAB_GROUP_ID = Token.createRandom();
    private static final String TAB_GROUP_ID_STRING = TAB_GROUP_ID.toString();
    private static final String TAB_GROUP_TITLE = "Tab Group Title";
    private static final int TAB_GROUP_INDICATOR_COLOR_ID = 8;
    private static final @ColorInt int TAB_GROUP_INDICATOR_COLOR = -2916608;
    private static final String COLLABORATION_ID = "CollaborationId";
    private static final GURL EXAMPLE_URL = new GURL("https://example.com");
    private static final GURL CHROME_SCHEME_URL = new GURL("chrome://history");
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
                    NUM_TABS,
                    NUM_INCOGNITO_TABS,
                    /* isIncognitoSelected= */ false,
                    LAST_ACCESSED_TIME);

    private static final InstanceInfo INSTANCE_INFO_2 =
            new InstanceInfo(
                    INSTANCE_ID_2,
                    TASK_ID,
                    CURRENT,
                    EXAMPLE_URL.toString(),
                    WINDOW_TITLE_2,
                    NUM_TABS,
                    NUM_INCOGNITO_TABS,
                    /* isIncognitoSelected= */ false,
                    LAST_ACCESSED_TIME);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private TabContextMenuCoordinator mTabContextMenuCoordinator;
    private OnItemClickedCallback<Integer> mOnItemClickedCallback;
    private MockTabModel mTabModel;
    private final LocalTabGroupId mLocalId = new LocalTabGroupId(TAB_GROUP_ID);
    private final SavedTabGroup mSavedTabGroup = new SavedTabGroup();
    private final SavedTabGroupTab mSavedTabGroupTab = new SavedTabGroupTab();
    @Mock private TabList mTabList;
    @Mock private Tab mTab1;
    @Mock private Tab mTabOutsideOfGroup;
    @Mock private Tab mNonUrlTab;
    @Mock private TabRemover mTabRemover;
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private Profile mProfile;
    @Mock private TabGroupListBottomSheetCoordinator mBottomSheetCoordinator;
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

    @Before
    public void setUp() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        when(mServiceStatus.isAllowedToCreate()).thenReturn(true);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);

        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mWindowAndroid.getKeyboardDelegate()).thenReturn(mKeyboardVisibilityDelegate);
        when(mWindowAndroid.getActivity()).thenReturn(mWeakReferenceActivity);
        when(mWeakReferenceActivity.get()).thenReturn(activity);
        when(mTabList.getCount()).thenReturn(2);
        when(mTabList.getTabAtChecked(0)).thenReturn(mTab1);
        when(mTabList.getTabAtChecked(1)).thenReturn(mTabOutsideOfGroup);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab1);
        when(mTabModel.getTabById(TAB_OUTSIDE_OF_GROUP_ID)).thenReturn(mTabOutsideOfGroup);
        when(mTabModel.getTabById(NON_URL_TAB_ID)).thenReturn(mNonUrlTab);
        when(mTabModel.getComprehensiveModel()).thenReturn(mTabList);
        mTabModel.setTabRemoverForTesting(mTabRemover);
        mTabModel.setTabCreatorForTesting(mTabCreator);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTab1.getUrl()).thenReturn(EXAMPLE_URL);
        when(mTabOutsideOfGroup.getTabGroupId()).thenReturn(null);
        when(mTabOutsideOfGroup.getUrl()).thenReturn(EXAMPLE_URL);
        when(mNonUrlTab.getTabGroupId()).thenReturn(null);
        when(mNonUrlTab.getUrl()).thenReturn(CHROME_SCHEME_URL);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.getTabUngrouper()).thenReturn(mTabUngrouper);
        when(mTabGroupModelFilter.getAllTabGroupIds()).thenReturn(Set.of(TAB_GROUP_ID));
        when(mTabGroupModelFilter.getTabCountForGroup(TAB_GROUP_ID)).thenReturn(1);
        when(mTabGroupModelFilter.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1));
        when(mTabGroupModelFilter.getTabGroupColor(TAB_GROUP_ID))
                .thenReturn(TAB_GROUP_INDICATOR_COLOR_ID);
        when(mTabGroupModelFilter.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(TAB_GROUP_TITLE);
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(INSTANCE_ID_1);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE)).thenReturn(List.of(INSTANCE_INFO_1));
        mSavedTabGroupTab.localId = TAB_ID;
        mSavedTabGroupTab.url = EXAMPLE_URL;
        mSavedTabGroup.savedTabs = Arrays.asList(mSavedTabGroupTab);
        mSavedTabGroup.collaborationId = COLLABORATION_ID;
        mSavedTabGroup.localId = mLocalId;
        mSavedTabGroup.title = TAB_GROUP_TITLE;
        mSavedTabGroup.color = TAB_GROUP_INDICATOR_COLOR_ID;
        when(mTabWindowManager.findWindowIdForTabGroup(TAB_GROUP_ID)).thenReturn(INSTANCE_ID_1);
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
                        mMultiInstanceManager,
                        () -> mShareDelegate,
                        mWindowAndroid,
                        mWeakReferenceActivity.get());
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testListMenuItems_tabInGroup() {
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(modelList, TAB_ID);

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
                mWeakReferenceActivity.get().getString(R.string.menu_add_tab_to_group),
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
                R.string.remove_tab_from_group,
                modelList.get(1).model.get(ListMenuItemProperties.TITLE_ID));
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
    public void testListMenuItems_tabOutsideOfGroup() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(modelList, TAB_OUTSIDE_OF_GROUP_ID);

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE);

        // List item 2
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                1,
                R.plurals.move_tab_to_another_window,
                List.of(),
                mWeakReferenceActivity.get());

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
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testAddToGroupSubmenu_fallbackTabGroupName() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        mSavedTabGroup.title = "";
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(modelList, TAB_OUTSIDE_OF_GROUP_ID);

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, "1 tab");
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testListMenuItems_tabOutsideOfGroup_multipleWindows() {
        MultiWindowUtils.setInstanceCountForTesting(3);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2));

        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(modelList, TAB_OUTSIDE_OF_GROUP_ID);

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE);

        // List item 2
        StripLayoutContextMenuCoordinatorTestUtils.verifyAddToWindowSubmenu(
                modelList,
                1,
                R.plurals.move_tab_to_another_window,
                List.of(WINDOW_TITLE_2),
                mWeakReferenceActivity.get());

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
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testListMenuItems_belowApi31() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(modelList, TAB_OUTSIDE_OF_GROUP_ID);

        assertEquals("Number of items in the list menu is incorrect", 4, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE);

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
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testListMenuItems_nonShareableUrl() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(modelList, NON_URL_TAB_ID);

        assertEquals("Number of items in the list menu is incorrect", 4, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE);

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
    public void testListMenuItems_incognito() {
        setupWithIncognito(/* incognito= */ true);
        initializeCoordinator();
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(modelList, TAB_OUTSIDE_OF_GROUP_ID);

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE);

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
                        + " R.style.TextAppearance_TextLarge_Primary_Baseline_Light in incognito",
                R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                modelList.get(3).model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));

        // List item 4
        assertEquals(R.string.close, modelList.get(4).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(4).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                "Expected text appearance ID to be set to"
                        + " R.style.TextAppearance_TextLarge_Primary_Baseline_Light in incognito",
                R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                modelList.get(4).model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures({
        ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP,
        ChromeFeatureList.ANDROID_PINNED_TABS
    })
    public void testListMenuItems_tabOutsideOfGroup_pinnedTabs_showPinTabOption() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(modelList, TAB_OUTSIDE_OF_GROUP_ID);

        assertEquals("Number of items in the list menu is incorrect", 6, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE);

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
                R.string.menu_pin_tab, modelList.get(4).model.get(ListMenuItemProperties.TITLE_ID));
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
        ChromeFeatureList.ANDROID_PINNED_TABS
    })
    public void testListMenuItems_tabOutsideOfGroup_pinnedTabs_showUnpinTabOption() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        var modelList = new ModelList();

        // Pin tab to show unpin option.
        when(mTabOutsideOfGroup.getIsPinned()).thenReturn(true);
        mTabContextMenuCoordinator.configureMenuItemsForTesting(modelList, TAB_OUTSIDE_OF_GROUP_ID);

        assertEquals("Number of items in the list menu is incorrect", 6, modelList.size());

        // List item 1
        verifyAddToGroupSubmenuForTabOutsideOfGroup(modelList, TAB_GROUP_TITLE);

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
                R.string.menu_unpin_tab,
                modelList.get(4).model.get(ListMenuItemProperties.TITLE_ID));
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
    public void testRemoveFromGroup() {
        mOnItemClickedCallback.onClick(
                R.id.remove_from_tab_group,
                TAB_ID,
                COLLABORATION_ID,
                /* listViewTouchTracker= */ null);
        verify(mTabUngrouper, times(1)).ungroupTabs(List.of(mTab1), true, true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testShareUrl() {
        mOnItemClickedCallback.onClick(
                R.id.share_tab, TAB_ID, COLLABORATION_ID, /* listViewTouchTracker= */ null);
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
                R.id.close_tab, TAB_ID, COLLABORATION_ID, listViewTouchTracker);
        verify(mTabRemover, times(1))
                .closeTabs(
                        TabClosureParams.closeTab(mTab1)
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
                R.id.add_to_tab_group, TAB_ID, COLLABORATION_ID, /* listViewTouchTracker= */ null);
        verify(mBottomSheetCoordinator, times(1)).showBottomSheet(List.of(mTab1));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveToNewWindow() {
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(modelList, TAB_OUTSIDE_OF_GROUP_ID);
        StripLayoutContextMenuCoordinatorTestUtils.clickMoveToNewWindow(modelList, 1, mView);
        verify(mMultiInstanceManager, times(1)).moveTabsToNewWindow(List.of(mTabOutsideOfGroup));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testMoveToWindow() {
        MultiWindowUtils.setInstanceCountForTesting(2);
        when(mMultiInstanceManager.getInstanceInfo(ACTIVE))
                .thenReturn(List.of(INSTANCE_INFO_1, INSTANCE_INFO_2));
        var modelList = new ModelList();
        mTabContextMenuCoordinator.configureMenuItemsForTesting(modelList, TAB_OUTSIDE_OF_GROUP_ID);

        StripLayoutContextMenuCoordinatorTestUtils.clickMoveToWindowRow(
                modelList, 1, WINDOW_TITLE_2, mView);

        verify(mMultiInstanceManager, times(1))
                .moveTabsToWindow(
                        INSTANCE_INFO_2, List.of(mTabOutsideOfGroup), TabList.INVALID_TAB_INDEX);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testAnchorWidth() {
        StripLayoutContextMenuCoordinatorTestUtils.testAnchorWidth(
                mWeakReferenceActivity, mTabContextMenuCoordinator::getMenuWidth);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testAnchor_offset() {
        StripLayoutContextMenuCoordinatorTestUtils.testAnchor_offset(
                (rectProvider) -> mTabContextMenuCoordinator.showMenu(rectProvider, TAB_ID),
                mTabContextMenuCoordinator::destroyMenuForTesting);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUBMENUS_TAB_CONTEXT_MENU_LFF_TAB_STRIP)
    public void testAnchor_offset_incognito() {
        setupWithIncognito(/* incognito= */ true);
        StripLayoutContextMenuCoordinatorTestUtils.testAnchor_offset_incognito(
                (rectProvider) -> mTabContextMenuCoordinator.showMenu(rectProvider, TAB_ID),
                mTabContextMenuCoordinator::destroyMenuForTesting);
    }

    private void verifyAddToGroupSubmenuForTabOutsideOfGroup(
            ModelList modelList, String expectedTabGroupName) {
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
                mWeakReferenceActivity.get().getString(R.string.menu_add_tab_to_group),
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
                TAB_GROUP_INDICATOR_COLOR,
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
                mWeakReferenceActivity
                        .get()
                        .getResources()
                        .getQuantityString(R.plurals.move_tab_to_another_window, 2),
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
}

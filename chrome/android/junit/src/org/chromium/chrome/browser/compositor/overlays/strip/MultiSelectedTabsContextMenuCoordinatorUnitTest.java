// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.listmenu.ListItemType.DIVIDER;
import static org.chromium.ui.listmenu.ListSectionDividerProperties.COLOR_ID;

import android.app.Activity;

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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator.OnItemClickedCallback;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link MultiSelectedTabsContextMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ANDROID_TAB_HIGHLIGHTING)
public class MultiSelectedTabsContextMenuCoordinatorUnitTest {
    private static final int TAB_1_ID = 1;
    private static final int TAB_2_ID = 2;
    private static final int TAB_OUTSIDE_OF_GROUP_ID_1 = 3;
    private static final int TAB_OUTSIDE_OF_GROUP_ID_2 = 4;
    private static final int NON_URL_TAB_ID = 5;
    private static final Token TAB_GROUP_ID = Token.createRandom();
    private static final String COLLABORATION_ID = "CollaborationId";
    private static final GURL EXAMPLE_URL = new GURL("https://example.com");
    private static final GURL CHROME_SCHEME_URL = new GURL("chrome://history");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private MultiSelectedTabsContextMenuCoordinator mMultiSelectedTabsContextMenuCoordinator;
    private OnItemClickedCallback<List<Integer>> mOnItemClickedCallback;
    private MockTabModel mTabModel;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTabOutsideOfGroup1;
    @Mock private Tab mTabOutsideOfGroup2;
    @Mock private Tab mNonUrlTab;
    @Mock private TabRemover mTabRemover;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private Profile mProfile;
    @Mock private TabGroupListBottomSheetCoordinator mBottomSheetCoordinator;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private TabCreator mTabCreator;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ServiceStatus mServiceStatus;
    @Mock private WeakReference<Activity> mWeakReferenceActivity;

    @Before
    public void setUp() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        when(mServiceStatus.isAllowedToCreate()).thenReturn(true);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        MultiWindowUtils.setInstanceCountForTesting(1);

        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mWindowAndroid.getKeyboardDelegate()).thenReturn(mKeyboardVisibilityDelegate);
        when(mWindowAndroid.getActivity()).thenReturn(mWeakReferenceActivity);
        when(mWeakReferenceActivity.get()).thenReturn(activity);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabModel.getTabById(TAB_1_ID)).thenReturn(mTab1);
        when(mTabModel.getTabById(TAB_2_ID)).thenReturn(mTab2);
        when(mTabModel.getTabById(TAB_OUTSIDE_OF_GROUP_ID_1)).thenReturn(mTabOutsideOfGroup1);
        when(mTabModel.getTabById(TAB_OUTSIDE_OF_GROUP_ID_2)).thenReturn(mTabOutsideOfGroup2);
        when(mTabModel.getTabById(NON_URL_TAB_ID)).thenReturn(mNonUrlTab);
        mTabModel.setTabRemoverForTesting(mTabRemover);
        mTabModel.setTabCreatorForTesting(mTabCreator);

        when(mTab1.getId()).thenReturn(TAB_1_ID);
        when(mTab2.getId()).thenReturn(TAB_2_ID);

        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTab1.getUrl()).thenReturn(EXAMPLE_URL);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab2)).thenReturn(true);
        when(mTab2.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTab2.getUrl()).thenReturn(EXAMPLE_URL);
        when(mTabOutsideOfGroup1.getTabGroupId()).thenReturn(null);
        when(mTabOutsideOfGroup1.getUrl()).thenReturn(EXAMPLE_URL);
        when(mTabOutsideOfGroup2.getTabGroupId()).thenReturn(null);
        when(mTabOutsideOfGroup2.getUrl()).thenReturn(EXAMPLE_URL);
        when(mNonUrlTab.getTabGroupId()).thenReturn(null);
        when(mNonUrlTab.getUrl()).thenReturn(CHROME_SCHEME_URL);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.getTabUngrouper()).thenReturn(mTabUngrouper);
        setupWithIncognito(/* incognito= */ false); // Most tests will run not in incognito mode
        initializeCoordinator();
    }

    private void setupWithIncognito(boolean incognito) {
        when(mTabModel.isIncognito()).thenReturn(incognito);
        when(mTabModel.isIncognitoBranded()).thenReturn(incognito);
        when(mProfile.isOffTheRecord()).thenReturn(incognito);
    }

    private void initializeCoordinator() {
        mOnItemClickedCallback =
                MultiSelectedTabsContextMenuCoordinator.getMenuItemClickedCallback(
                        () -> mTabModel,
                        mTabGroupModelFilter,
                        mBottomSheetCoordinator,
                        mMultiInstanceManager);
        mMultiSelectedTabsContextMenuCoordinator =
                MultiSelectedTabsContextMenuCoordinator.createContextMenuCoordinator(
                        mTabModel,
                        mTabGroupModelFilter,
                        mBottomSheetCoordinator,
                        mMultiInstanceManager,
                        mWindowAndroid);
    }

    /**
     * Helper to retrieve a quantity string.
     *
     * @param resId The resource ID of the string.
     * @param quantity The quantity.
     * @return The formatted string.
     */
    private String getQuantityString(int resId, int quantity) {
        return mWeakReferenceActivity.get().getResources().getQuantityString(resId, quantity);
    }

    /**
     * Helper to assert menu item properties when the title is a string.
     *
     * @param modelList The model list for the menu.
     * @param index The index of the item to check.
     * @param expectedTitle The expected title.
     * @param expectedMenuItemId The expected menu item ID.
     */
    private void assertMenuItemTitle(
            ModelList modelList, int index, String expectedTitle, int expectedMenuItemId) {
        assertEquals(
                "Menu item title is incorrect.",
                expectedTitle,
                modelList.get(index).model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                "Menu item ID is incorrect.",
                expectedMenuItemId,
                modelList.get(index).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    /**
     * Helper to assert menu item properties when the title is a string resource.
     *
     * @param modelList The model list for the menu.
     * @param index The index of the item to check.
     * @param expectedTitleId The expected title resource ID.
     * @param expectedMenuItemId The expected menu item ID.
     */
    private void assertMenuItemTitleId(
            ModelList modelList, int index, int expectedTitleId, int expectedMenuItemId) {
        assertEquals(
                "Menu item title ID is incorrect.",
                expectedTitleId,
                modelList.get(index).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                "Menu item ID is incorrect.",
                expectedMenuItemId,
                modelList.get(index).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    /**
     * Helper to assert menu item string style.
     *
     * @param modelList The model list for the menu.
     * @param index The index of the item to check.
     */
    private void assertStringStyleForIncognito(ModelList modelList, int index) {
        assertEquals(
                "Expected text appearance ID to be set to"
                        + " R.style.TextAppearance_TextLarge_Primary_Baseline_Light in incognito",
                R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                modelList.get(index).model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));
    }

    @Test
    public void testAnchorWidth() {
        StripLayoutContextMenuCoordinatorTestUtils.testAnchorWidth(
                mWeakReferenceActivity, mMultiSelectedTabsContextMenuCoordinator::getMenuWidth);
    }

    @Test
    public void testAnchor_offset() {
        StripLayoutContextMenuCoordinatorTestUtils.testAnchor_offset(
                (rectProvider) ->
                        mMultiSelectedTabsContextMenuCoordinator.showMenu(
                                rectProvider, List.of(TAB_1_ID, TAB_OUTSIDE_OF_GROUP_ID_1)),
                mMultiSelectedTabsContextMenuCoordinator::destroyMenuForTesting);
    }

    @Test
    public void testAnchor_offset_incognito() {
        setupWithIncognito(/* incognito= */ true);
        StripLayoutContextMenuCoordinatorTestUtils.testAnchor_offset_incognito(
                (rectProvider) ->
                        mMultiSelectedTabsContextMenuCoordinator.showMenu(
                                rectProvider, List.of(TAB_1_ID, TAB_OUTSIDE_OF_GROUP_ID_1)),
                mMultiSelectedTabsContextMenuCoordinator::destroyMenuForTesting);
    }

    @Test
    public void testListMenuItems_tabsInGroup() {
        var modelList = new ModelList();
        mMultiSelectedTabsContextMenuCoordinator.buildMenuActionItems(
                modelList, List.of(TAB_1_ID, TAB_2_ID));
        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        assertMenuItemTitle(
                modelList,
                0,
                getQuantityString(R.plurals.add_tab_to_group_menu_item, 2),
                R.id.add_to_tab_group);

        // List item 2
        assertMenuItemTitleId(
                modelList, 1, R.string.remove_tabs_from_group, R.id.remove_from_tab_group);

        // List item 3
        assertMenuItemTitle(
                modelList,
                2,
                getQuantityString(R.plurals.move_tabs_to_another_window, 1),
                R.id.move_to_other_window_menu_id);

        // List item 4
        assertEquals(DIVIDER, modelList.get(3).type);
        assertEquals(
                "Expected divider to have have COLOR_ID unset when not in incognito mode",
                0,
                modelList.get(3).model.get(COLOR_ID));

        // List item 5
        assertMenuItemTitleId(modelList, 4, R.string.close, R.id.close_tab);
    }

    @Test
    public void testListMenuItems_tabsOutsideOfGroup() {
        var modelList = new ModelList();
        mMultiSelectedTabsContextMenuCoordinator.buildMenuActionItems(
                modelList, List.of(TAB_OUTSIDE_OF_GROUP_ID_1, TAB_OUTSIDE_OF_GROUP_ID_2));
        assertEquals("Number of items in the list menu is incorrect", 4, modelList.size());

        // List item 1
        assertMenuItemTitle(
                modelList,
                0,
                getQuantityString(R.plurals.add_tab_to_group_menu_item, 2),
                R.id.add_to_tab_group);

        // List item 2
        assertMenuItemTitle(
                modelList,
                1,
                getQuantityString(R.plurals.move_tabs_to_another_window, 1),
                R.id.move_to_other_window_menu_id);

        // List item 3
        assertEquals(DIVIDER, modelList.get(2).type);
        assertEquals(
                "Expected divider to have have COLOR_ID unset when not in incognito mode",
                0,
                modelList.get(2).model.get(COLOR_ID));

        // List item 4
        assertMenuItemTitleId(modelList, 3, R.string.close, R.id.close_tab);
    }

    @Test
    public void testListMenuItems_someTabsInGroup() {
        var modelList = new ModelList();
        mMultiSelectedTabsContextMenuCoordinator.buildMenuActionItems(
                modelList, List.of(TAB_1_ID, TAB_OUTSIDE_OF_GROUP_ID_1));
        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        assertMenuItemTitle(
                modelList,
                0,
                getQuantityString(R.plurals.add_tab_to_group_menu_item, 2),
                R.id.add_to_tab_group);

        // List item 2
        assertMenuItemTitleId(
                modelList, 1, R.string.remove_tabs_from_group, R.id.remove_from_tab_group);

        // List item 3
        assertMenuItemTitle(
                modelList,
                2,
                getQuantityString(R.plurals.move_tabs_to_another_window, 1),
                R.id.move_to_other_window_menu_id);

        // List item 4
        assertEquals(DIVIDER, modelList.get(3).type);
        assertEquals(
                "Expected divider to have have COLOR_ID unset when not in incognito mode",
                0,
                modelList.get(3).model.get(COLOR_ID));

        // List item 5
        assertMenuItemTitleId(modelList, 4, R.string.close, R.id.close_tab);
    }

    @Test
    public void testListMenuItems_incognito() {
        setupWithIncognito(/* incognito= */ true);
        initializeCoordinator();
        var modelList = new ModelList();
        mMultiSelectedTabsContextMenuCoordinator.buildMenuActionItems(
                modelList, List.of(TAB_1_ID, TAB_OUTSIDE_OF_GROUP_ID_1));

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        assertMenuItemTitle(
                modelList,
                0,
                getQuantityString(R.plurals.add_tab_to_group_menu_item, 2),
                R.id.add_to_tab_group);
        assertStringStyleForIncognito(modelList, 0);

        // List item 2
        assertMenuItemTitleId(
                modelList, 1, R.string.remove_tabs_from_group, R.id.remove_from_tab_group);
        assertStringStyleForIncognito(modelList, 1);

        // List item 3
        assertMenuItemTitle(
                modelList,
                2,
                getQuantityString(R.plurals.move_tabs_to_another_window, 1),
                R.id.move_to_other_window_menu_id);
        assertStringStyleForIncognito(modelList, 2);

        // List item 4
        assertEquals(DIVIDER, modelList.get(3).type);
        assertEquals(
                "Expected divider to have COLOR_ID set to R.color.divider_line_bg_color_light in"
                        + " incognito mode",
                R.color.divider_line_bg_color_light,
                modelList.get(3).model.get(COLOR_ID));

        // List item 5
        assertMenuItemTitleId(modelList, 4, R.string.close, R.id.close_tab);
        assertStringStyleForIncognito(modelList, 4);
    }

    @Test
    public void testListMenuItems_belowApi31() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        var modelList = new ModelList();
        mMultiSelectedTabsContextMenuCoordinator.buildMenuActionItems(
                modelList, List.of(TAB_1_ID, TAB_OUTSIDE_OF_GROUP_ID_1));

        assertEquals("Number of items in the list menu is incorrect", 4, modelList.size());

        // List item 1
        assertMenuItemTitle(
                modelList,
                0,
                getQuantityString(R.plurals.add_tab_to_group_menu_item, 2),
                R.id.add_to_tab_group);

        // List item 2
        assertMenuItemTitleId(
                modelList, 1, R.string.remove_tabs_from_group, R.id.remove_from_tab_group);

        // List item 3
        assertEquals(DIVIDER, modelList.get(2).type);
        assertEquals(
                "Expected divider to have have COLOR_ID unset when not in incognito mode",
                0,
                modelList.get(2).model.get(COLOR_ID));

        // List item 4
        assertMenuItemTitleId(modelList, 3, R.string.close, R.id.close_tab);
    }

    @Test
    public void testListMenuItems_tabOutsideOfGroup_multipleWindows() {
        MultiWindowUtils.setInstanceCountForTesting(2);

        var modelList = new ModelList();
        mMultiSelectedTabsContextMenuCoordinator.buildMenuActionItems(
                modelList, List.of(TAB_1_ID, TAB_OUTSIDE_OF_GROUP_ID_1));

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        assertMenuItemTitle(
                modelList,
                0,
                getQuantityString(R.plurals.add_tab_to_group_menu_item, 2),
                R.id.add_to_tab_group);

        // List item 2
        assertMenuItemTitleId(
                modelList, 1, R.string.remove_tabs_from_group, R.id.remove_from_tab_group);

        // List item 3
        assertMenuItemTitle(
                modelList,
                2,
                getQuantityString(R.plurals.move_tabs_to_another_window, 2),
                R.id.move_to_other_window_menu_id);

        // List item 4
        assertEquals(DIVIDER, modelList.get(3).type);
        assertEquals(
                "Expected divider to have have COLOR_ID unset when not in incognito mode",
                0,
                modelList.get(3).model.get(COLOR_ID));

        // List item 5
        assertMenuItemTitleId(modelList, 4, R.string.close, R.id.close_tab);
    }

    @Test
    public void testAddToTabsGroup_newTabGroup() {
        List<Integer> tabIds = List.of(TAB_1_ID, TAB_OUTSIDE_OF_GROUP_ID_1);
        List<Tab> tabs = List.of(mTab1, mTabOutsideOfGroup1);
        mOnItemClickedCallback.onClick(
                R.id.add_to_tab_group, tabIds, COLLABORATION_ID, /* listViewTouchTracker= */ null);
        verify(mBottomSheetCoordinator, times(1)).showBottomSheet(tabs);
    }

    @Test
    public void testRemoveTabsFromGroup() {
        List<Integer> tabIds = List.of(TAB_1_ID, TAB_OUTSIDE_OF_GROUP_ID_1);
        // Internally, tabs not part of a group are filtered out.
        List<Tab> groupedTabs = Collections.singletonList(mTab1);
        mOnItemClickedCallback.onClick(
                R.id.remove_from_tab_group,
                tabIds,
                COLLABORATION_ID,
                /* listViewTouchTracker= */ null);
        verify(mTabUngrouper, times(1)).ungroupTabs(groupedTabs, true, true);
    }

    @Test
    public void testMoveMultipleTabsToAnotherWindow() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        List<Tab> tabsToMove = List.of(mTabOutsideOfGroup1, mTabOutsideOfGroup2);

        mOnItemClickedCallback.onClick(
                R.id.move_to_other_window_menu_id,
                List.of(TAB_OUTSIDE_OF_GROUP_ID_1, TAB_OUTSIDE_OF_GROUP_ID_2),
                COLLABORATION_ID,
                /* listViewTouchTracker= */ null);

        verify(mTabUngrouper, times(0))
                .ungroupTabs(
                        (List<Tab>) org.mockito.ArgumentMatchers.any(),
                        org.mockito.ArgumentMatchers.anyBoolean(),
                        org.mockito.ArgumentMatchers.anyBoolean());
        verify(mMultiInstanceManager, times(1)).moveTabsToOtherWindow(tabsToMove);
    }

    @Test
    public void testMoveMultipleTabsToAnotherWindow_oneTabInGroup() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        List<Tab> tabsToMove = List.of(mTab1, mTabOutsideOfGroup1);

        mOnItemClickedCallback.onClick(
                R.id.move_to_other_window_menu_id,
                List.of(TAB_1_ID, TAB_OUTSIDE_OF_GROUP_ID_1),
                COLLABORATION_ID,
                /* listViewTouchTracker= */ null);

        verify(mTabUngrouper, times(1)).ungroupTabs(Collections.singletonList(mTab1), true, false);
        verify(mMultiInstanceManager, times(1)).moveTabsToOtherWindow(tabsToMove);
    }

    @Test
    public void testMoveMultipleTabsToAnotherWindow_multipleWindows() {
        MultiWindowUtils.setInstanceCountForTesting(2);
        List<Tab> tabsToMove = List.of(mTabOutsideOfGroup1, mTabOutsideOfGroup2);

        mOnItemClickedCallback.onClick(
                R.id.move_to_other_window_menu_id,
                List.of(TAB_OUTSIDE_OF_GROUP_ID_1, TAB_OUTSIDE_OF_GROUP_ID_2),
                COLLABORATION_ID,
                /* listViewTouchTracker= */ null);

        verify(mTabUngrouper, times(0))
                .ungroupTabs(
                        (List<Tab>) org.mockito.ArgumentMatchers.any(),
                        org.mockito.ArgumentMatchers.anyBoolean(),
                        org.mockito.ArgumentMatchers.anyBoolean());
        verify(mMultiInstanceManager, times(1)).moveTabsToOtherWindow(tabsToMove);
    }

    @Test
    public void testMoveMultipleTabsToAnotherWindow_multipleWindows_oneTabInGroup() {
        MultiWindowUtils.setInstanceCountForTesting(2);
        List<Tab> tabsToMove = List.of(mTab1, mTabOutsideOfGroup1);

        mOnItemClickedCallback.onClick(
                R.id.move_to_other_window_menu_id,
                List.of(TAB_1_ID, TAB_OUTSIDE_OF_GROUP_ID_1),
                COLLABORATION_ID,
                /* listViewTouchTracker= */ null);

        verify(mTabUngrouper, times(1)).ungroupTabs(Collections.singletonList(mTab1), true, false);
        verify(mMultiInstanceManager, times(1)).moveTabsToOtherWindow(tabsToMove);
    }

    @Test
    public void testCloseMultipleTabs() {
        mOnItemClickedCallback.onClick(
                R.id.close_tab,
                List.of(TAB_1_ID, TAB_2_ID),
                COLLABORATION_ID,
                /* listViewTouchTracker= */ null);
        verify(mTabRemover, times(1))
                .closeTabs(
                        TabClosureParams.closeTabs(List.of(mTab1, mTab2))
                                .allowUndo(true)
                                .tabClosingSource(TabClosingSource.TABLET_TAB_STRIP)
                                .build(),
                        /* allowDialog= */ true);
    }
}

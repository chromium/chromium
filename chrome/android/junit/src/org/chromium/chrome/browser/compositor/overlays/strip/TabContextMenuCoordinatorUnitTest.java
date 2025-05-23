// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator.ListItemType.DIVIDER;
import static org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin.TAB_STRIP_CONTEXT_MENU;
import static org.chromium.ui.listmenu.ListSectionDividerProperties.COLOR_ID;

import android.app.Activity;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.MotionEvent;

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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator.OnItemClickedCallback;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.util.motion.MotionEventTestUtils;
import org.chromium.components.browser_ui.widget.list_view.FakeListViewTouchTracker;
import org.chromium.components.browser_ui.widget.list_view.ListViewTouchTracker;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.RectProvider;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.List;

/** Unit tests for {@link TabContextMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.DATA_SHARING})
public class TabContextMenuCoordinatorUnitTest {
    private static final int TAB_ID = 1;
    private static final int TAB_OUTSIDE_OF_GROUP_ID = 2;
    private static final int NON_URL_TAB_ID = 3;
    private static final Token TAB_GROUP_ID = Token.createRandom();
    private static final String COLLABORATION_ID = "CollaborationId";
    private static final GURL EXAMPLE_URL = new GURL("https://example.com");
    private static final GURL CHROME_SCHEME_URL = new GURL("chrome://history");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private TabContextMenuCoordinator mTabContextMenuCoordinator;
    private OnItemClickedCallback<Integer> mOnItemClickedCallback;
    private MockTabModel mTabModel;
    private final SavedTabGroup mSavedTabGroup = new SavedTabGroup();
    @Mock private Tab mTab1;
    @Mock private Tab mTabOutsideOfGroup;
    @Mock private Tab mNonUrlTab;
    @Mock private TabRemover mTabRemover;
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

    @Before
    public void setUp() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        when(mServiceStatus.isAllowedToCreate()).thenReturn(true);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);

        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mWindowAndroid.getKeyboardDelegate()).thenReturn(mKeyboardVisibilityDelegate);
        when(mWindowAndroid.getActivity()).thenReturn(mWeakReferenceActivity);
        when(mWeakReferenceActivity.get()).thenReturn(activity);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabModel.getTabById(TAB_ID)).thenReturn(mTab1);
        when(mTabModel.getTabById(TAB_OUTSIDE_OF_GROUP_ID)).thenReturn(mTabOutsideOfGroup);
        when(mTabModel.getTabById(NON_URL_TAB_ID)).thenReturn(mNonUrlTab);
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
        mSavedTabGroup.collaborationId = COLLABORATION_ID;
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
                        mWindowAndroid);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testListMenuItems_tabInGroup() {
        var modelList = new ModelList();
        mTabContextMenuCoordinator.buildMenuActionItems(modelList, TAB_ID);

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        assertEquals(
                R.string.menu_add_tab_to_group,
                modelList.get(0).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.add_to_tab_group,
                modelList.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));

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
    public void testListMenuItems_tabOutsideOfGroup() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.buildMenuActionItems(modelList, TAB_OUTSIDE_OF_GROUP_ID);

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        assertEquals(
                R.string.menu_add_tab_to_group,
                modelList.get(0).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.add_to_tab_group,
                modelList.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 2
        assertEquals(
                mWeakReferenceActivity
                        .get()
                        .getResources()
                        .getQuantityString(R.plurals.move_tab_to_another_window, 1),
                modelList.get(1).model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                R.id.move_to_other_window_menu_id,
                modelList.get(1).model.get(ListMenuItemProperties.MENU_ITEM_ID));

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
    public void testListMenuItems_tabOutsideOfGroup_multipleWindows() {
        MultiWindowUtils.setInstanceCountForTesting(2);

        var modelList = new ModelList();
        mTabContextMenuCoordinator.buildMenuActionItems(modelList, TAB_OUTSIDE_OF_GROUP_ID);

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        assertEquals(
                R.string.menu_add_tab_to_group,
                modelList.get(0).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.add_to_tab_group,
                modelList.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 2
        assertEquals(
                mWeakReferenceActivity
                        .get()
                        .getResources()
                        .getQuantityString(R.plurals.move_tab_to_another_window, 2),
                modelList.get(1).model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                R.id.move_to_other_window_menu_id,
                modelList.get(1).model.get(ListMenuItemProperties.MENU_ITEM_ID));

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
    public void testListMenuItems_belowApi31() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.buildMenuActionItems(modelList, TAB_OUTSIDE_OF_GROUP_ID);

        assertEquals("Number of items in the list menu is incorrect", 4, modelList.size());

        // List item 1
        assertEquals(
                R.string.menu_add_tab_to_group,
                modelList.get(0).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.add_to_tab_group,
                modelList.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));

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
    public void testListMenuItems_nonShareableUrl() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        var modelList = new ModelList();
        mTabContextMenuCoordinator.buildMenuActionItems(modelList, NON_URL_TAB_ID);

        assertEquals("Number of items in the list menu is incorrect", 4, modelList.size());

        // List item 1
        assertEquals(
                R.string.menu_add_tab_to_group,
                modelList.get(0).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.add_to_tab_group,
                modelList.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 2
        assertEquals(
                mWeakReferenceActivity
                        .get()
                        .getResources()
                        .getQuantityString(R.plurals.move_tab_to_another_window, 1),
                modelList.get(1).model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                R.id.move_to_other_window_menu_id,
                modelList.get(1).model.get(ListMenuItemProperties.MENU_ITEM_ID));

        // List item 3
        assertEquals(DIVIDER, modelList.get(2).type);

        // List item 4
        assertEquals(R.string.close, modelList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.close_tab, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    public void testListMenuItems_incognito() {
        setupWithIncognito(/* incognito= */ true);
        initializeCoordinator();
        var modelList = new ModelList();
        mTabContextMenuCoordinator.buildMenuActionItems(modelList, TAB_ID);

        assertEquals("Number of items in the list menu is incorrect", 5, modelList.size());

        // List item 1
        assertEquals(
                R.string.menu_add_tab_to_group,
                modelList.get(0).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.add_to_tab_group,
                modelList.get(0).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                "Expected text appearance ID to be set to"
                        + " R.style.TextAppearance_TextLarge_Primary_Baseline_Light in incognito",
                R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                modelList.get(0).model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));

        // List item 2
        assertEquals(
                R.string.remove_tab_from_group,
                modelList.get(1).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.remove_from_tab_group,
                modelList.get(1).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                "Expected text appearance ID to be set to"
                        + " R.style.TextAppearance_TextLarge_Primary_Baseline_Light in incognito",
                R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                modelList.get(1).model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));

        // List item 3
        assertEquals(DIVIDER, modelList.get(2).type);
        assertEquals(
                "Expected divider to have COLOR_ID set to R.color.divider_line_bg_color_light in"
                        + " incognito mode",
                R.color.divider_line_bg_color_light,
                modelList.get(2).model.get(COLOR_ID));

        // List item 4
        assertEquals(R.string.share, modelList.get(3).model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.id.share_tab, modelList.get(3).model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                "Expected text appearance ID to be set to"
                        + " R.style.TextAppearance_TextLarge_Primary_Baseline_Light in incognito",
                R.style.TextAppearance_TextLarge_Primary_Baseline_Light,
                modelList.get(3).model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));

        // List item 5
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
                        TabClosureParams.closeTab(mTab1).allowUndo(shouldAllowUndo).build(),
                        /* allowDialog= */ true);
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testAddToTabGroup_newTabGroup() {
        mOnItemClickedCallback.onClick(
                R.id.add_to_tab_group, TAB_ID, COLLABORATION_ID, /* listViewTouchTracker= */ null);
        verify(mBottomSheetCoordinator, times(1)).showBottomSheet(List.of(mTab1));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testAnchorWidth_smallAnchorWidth() {
        assertEquals(
                mWeakReferenceActivity
                        .get()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_strip_context_menu_min_width),
                mTabContextMenuCoordinator.getMenuWidth(1));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testAnchorWidth_largeAnchorWidth() {
        assertEquals(
                mWeakReferenceActivity
                        .get()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_strip_context_menu_max_width),
                mTabContextMenuCoordinator.getMenuWidth(10000));
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testAnchorWidth_moderateAnchorWidth() {
        int minWidth =
                mWeakReferenceActivity
                        .get()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_strip_context_menu_min_width);
        int expectedWidth = minWidth + 1;
        assertEquals(expectedWidth, mTabContextMenuCoordinator.getMenuWidth(expectedWidth));
    }

    @Test
    public void testAnchor_offset() {
        RectProvider rectProvider = new RectProvider();
        rectProvider.setRect(new Rect(0, 10, 50, 40));
        mTabContextMenuCoordinator.showMenu(rectProvider, 0);
        assertEquals(
                "Expected anchor rect to have a top offset of popup_menu_shadow_length, "
                        + "and a width which accounts for the popup_menu_shadow_length",
                new Rect(0, 4, 74, 34),
                rectProvider.getRect());
        // Clean up to avoid "object not destroyed after test".
        mTabContextMenuCoordinator.destroyMenuForTesting();
    }

    @Test
    public void testAnchor_offset_incognito() {
        setupWithIncognito(/* incognito= */ true);
        RectProvider rectProvider = new RectProvider();
        rectProvider.setRect(new Rect(0, 10, 50, 40));
        mTabContextMenuCoordinator.showMenu(rectProvider, 0);
        assertEquals(
                "Expected anchor rect to not have any offset in incognito",
                new Rect(0, 10, 50, 40),
                rectProvider.getRect());
        // Clean up to avoid "object not destroyed after test".
        mTabContextMenuCoordinator.destroyMenuForTesting();
    }

    @Test
    @Feature("Tab Strip Context Menu")
    public void testMoveToAnotherWindow() {
        mOnItemClickedCallback.onClick(
                R.id.move_to_other_window_menu_id,
                TAB_ID,
                COLLABORATION_ID,
                /* listViewTouchTracker= */ null);
        verify(mMultiInstanceManager, times(1)).moveTabToOtherWindow(mTab1);
    }
}

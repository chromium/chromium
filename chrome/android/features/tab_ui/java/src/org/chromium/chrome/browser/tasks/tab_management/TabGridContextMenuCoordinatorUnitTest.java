// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin.TAB_STRIP_CONTEXT_MENU;

import android.app.Activity;
import android.content.res.Resources;
import android.os.SystemClock;
import android.view.MotionEvent;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tasks.tab_management.TabGridContextMenuCoordinator.ShowTabListEditor;
import org.chromium.components.browser_ui.util.motion.MotionEventTestUtils;
import org.chromium.components.browser_ui.widget.list_view.FakeListViewTouchTracker;
import org.chromium.components.browser_ui.widget.list_view.ListViewTouchTracker;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;
import org.chromium.url.GURL;

import java.util.List;
import java.util.function.Supplier;

/** Unit tests for {@link TabGridContextMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID,
    ChromeFeatureList.ANDROID_PINNED_TABS
})
public class TabGridContextMenuCoordinatorUnitTest {
    private static @TabId final int TAB_ID = 1;
    private static final int MENU_WIDTH = 300;
    private static final String LOCALHOST_URL = "localhost://";
    private static final String CHROME_URL = "chrome://";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private TabBookmarker mTabBookmarker;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupListBottomSheetCoordinator mTabGroupListBottomSheetCoordinator;
    @Mock private TabGroupCreationDialogManager mTabGroupCreationDialogManager;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private TabModel mTabModel;
    @Mock private TabRemover mTabRemover;
    @Mock private Tab mTab;
    @Mock private ShareDelegate mShareDelegate;
    @Mock private Profile mProfile;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private ShowTabListEditor mShowTabListEditor;

    private TabGridContextMenuCoordinator mCoordinator;
    private ModelList mMenuItemList;
    private Activity mActivity;
    private GURL mUrl;
    private Token mTabGroupId;
    private ObservableSupplierImpl<TabBookmarker> mTabBookmarkerSupplier;

    @Before
    public void setUp() {
        mTabGroupId = Token.createRandom();
        mTabBookmarkerSupplier = new ObservableSupplierImpl<>(mTabBookmarker);

        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(1);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mShareDelegateSupplier.get()).thenReturn(mShareDelegate);
        when(mTab.getTabGroupId()).thenReturn(mTabGroupId);

        BookmarkModel.setInstanceForTesting(mBookmarkModel);

        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mCoordinator =
                new TabGridContextMenuCoordinator(
                        mActivity,
                        mTabBookmarkerSupplier,
                        mProfile,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mTabGroupSyncService,
                        mCollaborationService,
                        mShowTabListEditor);
        mMenuItemList = new ModelList();
        when(mTabModel.getTabById(anyInt())).thenReturn(mTab);
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mBookmarkModel.hasBookmarkIdForTab(any())).thenReturn(false);
    }

    @Test
    public void testShowMenu() {
        RectProvider rectProvider = new RectProvider();
        mCoordinator = spy(mCoordinator);
        doNothing()
                .when(mCoordinator)
                .createAndShowMenu(
                        any(),
                        any(),
                        anyBoolean(),
                        anyBoolean(),
                        anyInt(),
                        anyInt(),
                        any(),
                        anyBoolean());
        mCoordinator.showMenu(rectProvider, TAB_ID, /* focusable= */ true);
        verify(mCoordinator)
                .createAndShowMenu(
                        eq(rectProvider),
                        eq(TAB_ID),
                        eq(true),
                        eq(false),
                        eq(Resources.ID_NULL),
                        eq(HorizontalOrientation.LAYOUT_DIRECTION),
                        eq(mActivity),
                        eq(false));
    }

    @Test
    public void testGetMenuItemClickedCallback_shareTab() {
        TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabGridContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mShowTabListEditor);

        callback.onClick(
                R.id.share_tab,
                TAB_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mShareDelegate).share(mTab, false, TAB_STRIP_CONTEXT_MENU);
    }

    @Test
    public void testGetMenuItemClickedCallback_addToTabGroup() {
        TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabGridContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mShowTabListEditor);

        callback.onClick(
                R.id.add_to_tab_group,
                TAB_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabGroupListBottomSheetCoordinator).showBottomSheet(List.of(mTab));
    }

    @Test
    public void testGetMenuItemClickedCallback_addToNewTabGroup() {
        TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabGridContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mShowTabListEditor);

        callback.onClick(
                R.id.add_to_new_tab_group,
                TAB_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabGroupCreationDialogManager).showDialog(mTabGroupId, mTabGroupModelFilter);
    }

    @Test
    public void testGetMenuItemClickedCallback_addToBookmarks() {
        TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabGridContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mShowTabListEditor);

        callback.onClick(
                R.id.add_to_bookmarks,
                TAB_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabBookmarker).addOrEditBookmark(mTab);
    }

    @Test
    public void testGetMenuItemClickedCallback_editBookmark() {
        TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabGridContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mShowTabListEditor);

        callback.onClick(
                R.id.edit_bookmark,
                TAB_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabBookmarker).addOrEditBookmark(mTab);
    }

    @Test
    public void testGetMenuItemClickedCallback_selectTabs() {
        TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabGridContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mShowTabListEditor);

        callback.onClick(
                R.id.select_tabs,
                TAB_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mShowTabListEditor).show(TAB_ID);
    }

    @Test
    public void testGetMenuItemClickedCallback_pinTab() {
        TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabGridContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mShowTabListEditor);

        callback.onClick(
                R.id.pin_tab,
                TAB_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabModel).pinTab(TAB_ID, /* showUngroupDialog= */ true);
    }

    @Test
    public void testGetMenuItemClickedCallback_unpinTab() {
        TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabGridContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mShowTabListEditor);

        callback.onClick(
                R.id.unpin_tab,
                TAB_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabModel).unpinTab(TAB_ID);
    }

    @Test
    public void testGetMenuItemClickedCallback_closeTab_nullListViewTouchTracker() {
        testGetMenuItemClickedCallback_closeTab(
                /* listViewTouchTracker= */ null, /* shouldAllowUndo= */ true);
    }

    @Test
    public void testGetMenuItemClickedCallback_closeTab_withTouch() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker listViewTouchTracker = new FakeListViewTouchTracker();
        listViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createTouchMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        testGetMenuItemClickedCallback_closeTab(listViewTouchTracker, /* shouldAllowUndo= */ true);
    }

    @Test
    public void testGetMenuItemClickedCallback_closeTab_withMouse() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker listViewTouchTracker = new FakeListViewTouchTracker();
        listViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createMouseMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        testGetMenuItemClickedCallback_closeTab(listViewTouchTracker, /* shouldAllowUndo= */ false);
    }

    private void testGetMenuItemClickedCallback_closeTab(
            @Nullable ListViewTouchTracker listViewTouchTracker, boolean shouldAllowUndo) {
        // Setup
        TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabGridContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mShowTabListEditor);

        // Act
        callback.onClick(R.id.close_tab, TAB_ID, /* collaborationId= */ null, listViewTouchTracker);

        // Assert
        ArgumentCaptor<TabClosureParams> tabClosureParamsCaptor =
                ArgumentCaptor.forClass(TabClosureParams.class);
        verify(mTabRemover)
                .closeTabs(tabClosureParamsCaptor.capture(), /* allowDialog= */ eq(true));
        assertEquals(shouldAllowUndo, tabClosureParamsCaptor.getValue().allowUndo);
    }

    @Test
    public void testGetMenuItemClickedCallback_invalidTabId() {
        TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabGridContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mShowTabListEditor);

        callback.onClick(
                R.id.share_tab,
                Tab.INVALID_TAB_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mShareDelegate, never()).share(any(), anyBoolean(), anyInt());
    }

    @Test
    public void testGetMenuItemClickedCallback_tabNotFound() {
        when(mTabModel.getTabById(anyInt())).thenReturn(null);
        TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> callback =
                TabGridContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        mShowTabListEditor);

        callback.onClick(
                R.id.share_tab,
                TAB_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mShareDelegate, never()).share(any(), anyBoolean(), anyInt());
    }

    @Test
    public void testBuildMenuActionItems_withGroups() {
        mUrl = new GURL(LOCALHOST_URL);
        when(mTab.getUrl()).thenReturn(mUrl);
        mCoordinator.buildMenuActionItems(mMenuItemList, TAB_ID);

        assertEquals(6, mMenuItemList.size());
        assertEquals(R.string.share, getMenuItemTitleId(0));
        assertEquals(R.string.menu_move_tab_to_group, getMenuItemTitleId(1));
        assertEquals(R.string.add_to_bookmarks, getMenuItemTitleId(2));
        assertEquals(R.string.select_tab, getMenuItemTitleId(3));
        assertEquals(R.string.pin_tab, getMenuItemTitleId(4));
        assertEquals(R.string.close_tab, getMenuItemTitleId(5));
    }

    @Test
    public void testBuildMenuActionItems_withUnpinning() {
        mUrl = new GURL(LOCALHOST_URL);
        when(mTab.getUrl()).thenReturn(mUrl);
        when(mTab.getIsPinned()).thenReturn(true);
        mCoordinator.buildMenuActionItems(mMenuItemList, TAB_ID);

        assertEquals(6, mMenuItemList.size());
        assertEquals(R.string.share, getMenuItemTitleId(0));
        assertEquals(R.string.menu_move_tab_to_group, getMenuItemTitleId(1));
        assertEquals(R.string.add_to_bookmarks, getMenuItemTitleId(2));
        assertEquals(R.string.select_tab, getMenuItemTitleId(3));
        assertEquals(R.string.unpin_tab, getMenuItemTitleId(4));
        assertEquals(R.string.close_tab, getMenuItemTitleId(5));
    }

    @Test
    public void testBuildMenuActionItems_notInGroup() {
        mUrl = new GURL(LOCALHOST_URL);
        when(mTab.getUrl()).thenReturn(mUrl);
        when(mTab.getTabGroupId()).thenReturn(null);
        mCoordinator.buildMenuActionItems(mMenuItemList, TAB_ID);

        assertEquals(6, mMenuItemList.size());
        assertEquals(R.string.share, getMenuItemTitleId(0));
        assertEquals(R.string.menu_add_tab_to_group, getMenuItemTitleId(1));
        assertEquals(R.string.add_to_bookmarks, getMenuItemTitleId(2));
        assertEquals(R.string.select_tab, getMenuItemTitleId(3));
        assertEquals(R.string.pin_tab, getMenuItemTitleId(4));
        assertEquals(R.string.close_tab, getMenuItemTitleId(5));
    }

    @Test
    public void testBuildMenuActionItems_noGroups() {
        mUrl = new GURL(LOCALHOST_URL);
        when(mTab.getUrl()).thenReturn(mUrl);
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(0);
        mCoordinator.buildMenuActionItems(mMenuItemList, TAB_ID);

        assertEquals(6, mMenuItemList.size());
        assertEquals(R.string.share, getMenuItemTitleId(0));
        assertEquals(R.string.menu_add_tab_to_new_group, getMenuItemTitleId(1));
        assertEquals(R.string.add_to_bookmarks, getMenuItemTitleId(2));
        assertEquals(R.string.select_tab, getMenuItemTitleId(3));
        assertEquals(R.string.pin_tab, getMenuItemTitleId(4));
        assertEquals(R.string.close_tab, getMenuItemTitleId(5));
    }

    @Test
    public void testBuildMenuActionItems_alreadyBookmarked() {
        when(mBookmarkModel.hasBookmarkIdForTab(any())).thenReturn(true);

        mUrl = new GURL(LOCALHOST_URL);
        when(mTab.getUrl()).thenReturn(mUrl);
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(0);
        mCoordinator.buildMenuActionItems(mMenuItemList, TAB_ID);

        assertEquals(6, mMenuItemList.size());
        assertEquals(R.string.share, getMenuItemTitleId(0));
        assertEquals(R.string.menu_add_tab_to_new_group, getMenuItemTitleId(1));
        assertEquals(R.string.edit_bookmark, getMenuItemTitleId(2));
        assertEquals(R.string.select_tab, getMenuItemTitleId(3));
        assertEquals(R.string.pin_tab, getMenuItemTitleId(4));
        assertEquals(R.string.close_tab, getMenuItemTitleId(5));
    }

    @Test
    public void testBuildMenuActionItems_sharingDisabled() {
        mUrl = new GURL(CHROME_URL);
        when(mTab.getTabGroupId()).thenReturn(null);
        when(mTab.getUrl()).thenReturn(mUrl);
        mCoordinator.buildMenuActionItems(mMenuItemList, TAB_ID);

        assertEquals(5, mMenuItemList.size());
        assertEquals(R.string.menu_add_tab_to_group, getMenuItemTitleId(0));
        assertEquals(R.string.add_to_bookmarks, getMenuItemTitleId(1));
        assertEquals(R.string.select_tab, getMenuItemTitleId(2));
        assertEquals(R.string.pin_tab, getMenuItemTitleId(3));
        assertEquals(R.string.close_tab, getMenuItemTitleId(4));
    }

    @Test
    public void testBuildMenuActionItems_tabNotFound() {
        when(mTabModel.getTabById(anyInt())).thenReturn(null);
        mCoordinator.buildMenuActionItems(mMenuItemList, TAB_ID);
        assertEquals(0, mMenuItemList.size());
    }

    @Test
    public void testGetMenuWidth_withTabGroups() {
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(1);
        assertEquals(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_grid_context_menu_max_width),
                // Provide an arbitrary value for anchorViewWidthPx for the test.
                mCoordinator.getMenuWidth(/* anchorViewWidthPx= */ 0));
    }

    @Test
    public void testGetMenuWidth_noTabGroups() {
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(0);
        assertEquals(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_grid_context_menu_extended_width),
                // Provide an arbitrary value for anchorViewWidthPx for the test.
                mCoordinator.getMenuWidth(/* anchorViewWidthPx= */ 0));
    }

    @Test
    public void testGetCollaborationIdOrNull_tabNotFound() {
        when(mTabModel.getTabById(anyInt())).thenReturn(null);
        assertNull(mCoordinator.getCollaborationIdOrNull(TAB_ID));
    }

    private int getMenuItemTitleId(int mMenuItemListIndex) {
        return mMenuItemList.get(mMenuItemListIndex).model.get(ListMenuItemProperties.TITLE_ID);
    }
}

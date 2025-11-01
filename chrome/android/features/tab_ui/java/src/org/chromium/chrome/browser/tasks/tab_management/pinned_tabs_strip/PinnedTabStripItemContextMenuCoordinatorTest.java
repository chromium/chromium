// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupCreationDialogManager;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.url.GURL;

import java.util.List;
import java.util.function.Supplier;

/** Unit tests for {@link PinnedTabStripItemContextMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PinnedTabStripItemContextMenuCoordinatorTest {
    private static @TabId final int TAB_ID = 1;
    private static final String LOCALHOST_URL = "localhost://";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Supplier<TabBookmarker> mTabBookmarkerSupplier;
    @Mock private TabBookmarker mTabBookmarker;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupListBottomSheetCoordinator mTabGroupListBottomSheetCoordinator;
    @Mock private TabGroupCreationDialogManager mTabGroupCreationDialogManager;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private TabModel mTabModel;
    @Mock private TabRemover mTabRemover;
    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private BookmarkModel mBookmarkModel;

    private PinnedTabStripItemContextMenuCoordinator mCoordinator;
    private ModelList mMenuItemList;
    private Activity mActivity;
    private GURL mUrl;
    private Token mTabGroupId;

    @Before
    public void setUp() {
        mTabGroupId = Token.createRandom();
        when(mTabBookmarkerSupplier.get()).thenReturn(mTabBookmarker);

        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(1);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTab.getTabGroupId()).thenReturn(mTabGroupId);

        BookmarkModel.setInstanceForTesting(mBookmarkModel);

        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mCoordinator =
                new PinnedTabStripItemContextMenuCoordinator(
                        mActivity,
                        mProfile,
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager,
                        mTabGroupSyncService,
                        mCollaborationService);
        mMenuItemList = new ModelList();
        when(mTabModel.getTabById(anyInt())).thenReturn(mTab);
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mBookmarkModel.hasBookmarkIdForTab(any())).thenReturn(false);
    }

    @Test
    public void testGetMenuItemClickedCallback_addToTabGroup() {
        TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> callback =
                PinnedTabStripItemContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager);

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
                PinnedTabStripItemContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager);

        callback.onClick(
                R.id.add_to_new_tab_group,
                TAB_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabGroupModelFilter).createSingleTabGroup(mTab);
        verify(mTabGroupCreationDialogManager).showDialog(mTabGroupId, mTabGroupModelFilter);
    }

    @Test
    public void testGetMenuItemClickedCallback_addToBookmarks() {
        TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> callback =
                PinnedTabStripItemContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager);

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
                PinnedTabStripItemContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager);

        callback.onClick(
                R.id.edit_bookmark,
                TAB_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabBookmarker).addOrEditBookmark(mTab);
    }

    @Test
    public void testGetMenuItemClickedCallback_unpinTab() {
        TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> callback =
                PinnedTabStripItemContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager);

        callback.onClick(
                R.id.unpin_tab,
                TAB_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabModel).unpinTab(TAB_ID);
    }

    @Test
    public void testGetMenuItemClickedCallback_closeTab() {
        TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> callback =
                PinnedTabStripItemContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager);

        callback.onClick(
                R.id.close_tab,
                TAB_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);

        ArgumentCaptor<TabClosureParams> tabClosureParamsCaptor =
                ArgumentCaptor.forClass(TabClosureParams.class);
        verify(mTabRemover)
                .closeTabs(tabClosureParamsCaptor.capture(), /* allowDialog= */ eq(true));
        assertEquals(true, tabClosureParamsCaptor.getValue().allowUndo);
    }

    @Test
    public void testGetMenuItemClickedCallback_invalidTabId() {
        TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> callback =
                PinnedTabStripItemContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager);

        callback.onClick(
                R.id.unpin_tab,
                Tab.INVALID_TAB_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabModel, never()).unpinTab(anyInt());
    }

    @Test
    public void testGetMenuItemClickedCallback_tabNotFound() {
        when(mTabModel.getTabById(anyInt())).thenReturn(null);
        TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> callback =
                PinnedTabStripItemContextMenuCoordinator.getMenuItemClickedCallback(
                        mTabBookmarkerSupplier,
                        mTabGroupModelFilter,
                        mTabGroupListBottomSheetCoordinator,
                        mTabGroupCreationDialogManager);

        callback.onClick(
                R.id.unpin_tab,
                TAB_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabModel, never()).unpinTab(anyInt());
    }

    @Test
    public void testBuildMenuActionItems_withGroups() {
        mUrl = new GURL(LOCALHOST_URL);
        when(mTab.getUrl()).thenReturn(mUrl);
        mCoordinator.buildMenuActionItems(mMenuItemList, TAB_ID);

        assertEquals(4, mMenuItemList.size());
        assertEquals(R.string.menu_move_tab_to_group, getMenuItemTitleId(0));
        assertEquals(R.string.add_to_bookmarks, getMenuItemTitleId(1));
        assertEquals(R.string.unpin_tab, getMenuItemTitleId(2));
        assertEquals(R.string.close_tab, getMenuItemTitleId(3));
    }

    @Test
    public void testBuildMenuActionItems_notInGroup() {
        mUrl = new GURL(LOCALHOST_URL);
        when(mTab.getUrl()).thenReturn(mUrl);
        when(mTab.getTabGroupId()).thenReturn(null);
        mCoordinator.buildMenuActionItems(mMenuItemList, TAB_ID);

        assertEquals(4, mMenuItemList.size());
        assertEquals(R.string.menu_add_tab_to_group, getMenuItemTitleId(0));
        assertEquals(R.string.add_to_bookmarks, getMenuItemTitleId(1));
        assertEquals(R.string.unpin_tab, getMenuItemTitleId(2));
        assertEquals(R.string.close_tab, getMenuItemTitleId(3));
    }

    @Test
    public void testBuildMenuActionItems_noGroups() {
        mUrl = new GURL(LOCALHOST_URL);
        when(mTab.getUrl()).thenReturn(mUrl);
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(0);
        mCoordinator.buildMenuActionItems(mMenuItemList, TAB_ID);

        assertEquals(4, mMenuItemList.size());
        assertEquals(R.string.menu_add_tab_to_new_group, getMenuItemTitleId(0));
        assertEquals(R.string.add_to_bookmarks, getMenuItemTitleId(1));
        assertEquals(R.string.unpin_tab, getMenuItemTitleId(2));
        assertEquals(R.string.close_tab, getMenuItemTitleId(3));
    }

    @Test
    public void testBuildMenuActionItems_alreadyBookmarked() {
        when(mBookmarkModel.hasBookmarkIdForTab(any())).thenReturn(true);

        mUrl = new GURL(LOCALHOST_URL);
        when(mTab.getUrl()).thenReturn(mUrl);
        when(mTabGroupModelFilter.getTabGroupCount()).thenReturn(0);
        mCoordinator.buildMenuActionItems(mMenuItemList, TAB_ID);

        assertEquals(4, mMenuItemList.size());
        assertEquals(R.string.menu_add_tab_to_new_group, getMenuItemTitleId(0));
        assertEquals(R.string.edit_bookmark, getMenuItemTitleId(1));
        assertEquals(R.string.unpin_tab, getMenuItemTitleId(2));
        assertEquals(R.string.close_tab, getMenuItemTitleId(3));
    }

    @Test
    public void testBuildMenuActionItems_tabNotFound() {
        when(mTabModel.getTabById(anyInt())).thenReturn(null);
        mCoordinator.buildMenuActionItems(mMenuItemList, TAB_ID);
        assertEquals(0, mMenuItemList.size());
    }

    private int getMenuItemTitleId(int mMenuItemListIndex) {
        return mMenuItemList.get(mMenuItemListIndex).model.get(ListMenuItemProperties.TITLE_ID);
    }
}

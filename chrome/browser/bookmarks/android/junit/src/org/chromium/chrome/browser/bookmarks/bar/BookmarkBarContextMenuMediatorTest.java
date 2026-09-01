// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.FakeBookmarkModel;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarContextMenuMetrics.BookmarkBarContextMenuAction;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarContextMenuMetrics.BookmarkBarContextMenuEntrypoint;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuSubmenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;
import java.util.function.Supplier;

/** Unit tests for the {@link BookmarkBarContextMenuMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({ChromeFeatureList.BOOKMARKS_BAR_NTP, ChromeFeatureList.FLYOUT_IN_BOOKMARKS_BAR})
@EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_CONTEXT_MENU)
public class BookmarkBarContextMenuMediatorTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mCurrentTab;
    @Mock private Runnable mDismissRunnable;
    @Mock private BookmarkBarContextMenuDelegate mContextMenuDelegate;

    private Activity mActivity;
    private FakeBookmarkModel mBookmarkModel;
    private BookmarkBarContextMenuMediator mMediator;
    private Profile mProfile;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        mBookmarkModel = FakeBookmarkModel.createModel();
        BookmarkModel.setInstanceForTesting(mBookmarkModel);
        mProfile = mock(Profile.class);

        mMediator =
                new BookmarkBarContextMenuMediator(
                        mActivity,
                        ObservableSuppliers.createMonotonic(mProfile),
                        () -> mCurrentTab,
                        mContextMenuDelegate,
                        mDismissRunnable,
                        ObservableSuppliers.createNonNull(false));
    }

    // Tests for the layout of the context menu.

    @Test
    @SmallTest
    public void testBookmarkItem() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        bookmarkItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_ITEM);

        assertMenuStructure(
                list,
                enabled(R.string.contextmenu_open_in_new_tab),
                enabled(R.string.contextmenu_open_in_new_window),
                enabled(R.string.contextmenu_open_in_incognito_window),
                divider(),
                enabled(R.string.contextmenu_edit_bookmark_ellipsis),
                enabled(R.string.bookmark_item_move),
                divider(),
                enabled(R.string.bookmark_item_delete),
                divider(),
                enabled(R.string.contextmenu_add_page),
                enabled(R.string.contextmenu_add_folder),
                divider(),
                enabled(R.string.contextmenu_open_bookmarks_manager),
                enabled(R.string.contextmenu_show_bookmarks_bar));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testBookmarkItem_NtpFeatureEnabled() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        bookmarkItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_ITEM);

        assertMenuStructure(
                list,
                enabled(R.string.contextmenu_open_in_new_tab),
                enabled(R.string.contextmenu_open_in_new_window),
                enabled(R.string.contextmenu_open_in_incognito_window),
                divider(),
                enabled(R.string.contextmenu_edit_bookmark_ellipsis),
                enabled(R.string.bookmark_item_move),
                divider(),
                enabled(R.string.bookmark_item_delete),
                divider(),
                enabled(R.string.contextmenu_add_page),
                enabled(R.string.contextmenu_add_folder),
                divider(),
                enabled(R.string.contextmenu_open_bookmarks_manager),
                divider(),
                enabled(R.string.contextmenu_always_hide_bookmarks_bar),
                enabled(R.string.contextmenu_always_show_bookmarks_bar),
                enabled(R.string.contextmenu_only_show_bookmarks_bar_on_ntp));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.BOOKMARKS_BAR_NTP,
        ChromeFeatureList.FLYOUT_IN_BOOKMARKS_BAR
    })
    public void testBookmarkItem_NtpAndSubmenuFeatureEnabled() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        bookmarkItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_ITEM);

        assertMenuStructure(
                list,
                enabled(R.string.contextmenu_open_in_new_tab),
                enabled(R.string.contextmenu_open_in_new_window),
                enabled(R.string.contextmenu_open_in_incognito_window),
                divider(),
                enabled(R.string.contextmenu_edit_bookmark_ellipsis),
                enabled(R.string.bookmark_item_move),
                divider(),
                enabled(R.string.bookmark_item_delete),
                divider(),
                enabled(R.string.contextmenu_add_page),
                enabled(R.string.contextmenu_add_folder),
                divider(),
                enabled(R.string.contextmenu_open_bookmarks_manager),
                divider(),
                submenu(R.string.bookmark_bar_settings_title));

        Supplier<List<ListItem>> submenuProvider =
                list.get(list.size() - 1).model.get(ListMenuSubmenuItemProperties.SUBMENU_PROVIDER);
        assertNotNull(submenuProvider);
        List<ListItem> submenuItems = submenuProvider.get();
        assertEquals(3, submenuItems.size());
        assertEquals(
                mActivity.getString(R.string.contextmenu_always_hide_bookmarks_bar),
                submenuItems.get(0).model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                R.drawable.material_ic_check_24dp,
                submenuItems.get(0).model.get(ListMenuItemProperties.END_ICON_ID));
        assertTrue(submenuItems.get(0).model.get(ListMenuItemProperties.CHECKABLE));
        assertTrue(submenuItems.get(0).model.get(ListMenuItemProperties.CHECKED));
        assertEquals(0, submenuItems.get(0).model.get(ListMenuItemProperties.POSITION));
        assertEquals(
                mActivity.getString(R.string.contextmenu_always_show_bookmarks_bar),
                submenuItems.get(1).model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                android.R.color.transparent,
                submenuItems.get(1).model.get(ListMenuItemProperties.END_ICON_ID));
        assertTrue(submenuItems.get(1).model.get(ListMenuItemProperties.CHECKABLE));
        assertFalse(submenuItems.get(1).model.get(ListMenuItemProperties.CHECKED));
        assertEquals(1, submenuItems.get(1).model.get(ListMenuItemProperties.POSITION));
        assertEquals(
                mActivity.getString(R.string.contextmenu_only_show_bookmarks_bar_on_ntp),
                submenuItems.get(2).model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                android.R.color.transparent,
                submenuItems.get(2).model.get(ListMenuItemProperties.END_ICON_ID));
        assertTrue(submenuItems.get(2).model.get(ListMenuItemProperties.CHECKABLE));
        assertFalse(submenuItems.get(2).model.get(ListMenuItemProperties.CHECKED));
        assertEquals(2, submenuItems.get(2).model.get(ListMenuItemProperties.POSITION));
    }

    @Test
    @SmallTest
    public void testFolder_Empty() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        BookmarkId folderId =
                mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "Empty Folder");
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        folderItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER);

        assertMenuStructure(
                list,
                disabled(R.string.contextmenu_open_all),
                disabled(R.string.contextmenu_open_all_in_new_window),
                disabled(R.string.contextmenu_open_all_in_incognito_window),
                disabled(R.string.contextmenu_open_all_in_new_tab_group),
                divider(),
                enabled(R.string.contextmenu_edit_bookmark_ellipsis),
                enabled(R.string.bookmark_item_move),
                divider(),
                enabled(R.string.bookmark_item_delete),
                divider(),
                enabled(R.string.contextmenu_add_page),
                enabled(R.string.contextmenu_add_folder),
                divider(),
                enabled(R.string.contextmenu_open_bookmarks_manager),
                enabled(R.string.contextmenu_show_bookmarks_bar));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testFolder_Empty_NtpFeatureEnabled() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        BookmarkId folderId =
                mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "Empty Folder");
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        folderItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER);

        assertMenuStructure(
                list,
                disabled(R.string.contextmenu_open_all),
                disabled(R.string.contextmenu_open_all_in_new_window),
                disabled(R.string.contextmenu_open_all_in_incognito_window),
                disabled(R.string.contextmenu_open_all_in_new_tab_group),
                divider(),
                enabled(R.string.contextmenu_edit_bookmark_ellipsis),
                enabled(R.string.bookmark_item_move),
                divider(),
                enabled(R.string.bookmark_item_delete),
                divider(),
                enabled(R.string.contextmenu_add_page),
                enabled(R.string.contextmenu_add_folder),
                divider(),
                enabled(R.string.contextmenu_open_bookmarks_manager),
                divider(),
                enabled(R.string.contextmenu_always_hide_bookmarks_bar),
                enabled(R.string.contextmenu_always_show_bookmarks_bar),
                enabled(R.string.contextmenu_only_show_bookmarks_bar_on_ntp));
    }

    @Test
    @SmallTest
    public void testFolder_SingleBookmark() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        BookmarkId folderId =
                mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "Folder");
        mBookmarkModel.addBookmark(folderId, 0, "Child Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        folderItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER);

        assertMenuStructure(
                list,
                enabledPlural(R.plurals.contextmenu_open_all_plural, 1),
                enabledPlural(R.plurals.contextmenu_open_all_in_new_window_plural, 1),
                enabledPlural(R.plurals.contextmenu_open_all_in_incognito_window_plural, 1),
                enabledPlural(R.plurals.contextmenu_open_all_in_new_tab_group_plural, 1),
                divider(),
                enabled(R.string.contextmenu_edit_bookmark_ellipsis),
                enabled(R.string.bookmark_item_move),
                divider(),
                enabled(R.string.bookmark_item_delete),
                divider(),
                enabled(R.string.contextmenu_add_page),
                enabled(R.string.contextmenu_add_folder),
                divider(),
                enabled(R.string.contextmenu_open_bookmarks_manager),
                enabled(R.string.contextmenu_show_bookmarks_bar));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testFolder_SingleBookmark_NtpFeatureEnabled() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        BookmarkId folderId =
                mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "Folder");
        mBookmarkModel.addBookmark(folderId, 0, "Child Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        folderItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER);

        assertMenuStructure(
                list,
                enabledPlural(R.plurals.contextmenu_open_all_plural, 1),
                enabledPlural(R.plurals.contextmenu_open_all_in_new_window_plural, 1),
                enabledPlural(R.plurals.contextmenu_open_all_in_incognito_window_plural, 1),
                enabledPlural(R.plurals.contextmenu_open_all_in_new_tab_group_plural, 1),
                divider(),
                enabled(R.string.contextmenu_edit_bookmark_ellipsis),
                enabled(R.string.bookmark_item_move),
                divider(),
                enabled(R.string.bookmark_item_delete),
                divider(),
                enabled(R.string.contextmenu_add_page),
                enabled(R.string.contextmenu_add_folder),
                divider(),
                enabled(R.string.contextmenu_open_bookmarks_manager),
                divider(),
                enabled(R.string.contextmenu_always_hide_bookmarks_bar),
                enabled(R.string.contextmenu_always_show_bookmarks_bar),
                enabled(R.string.contextmenu_only_show_bookmarks_bar_on_ntp));
    }

    @Test
    @SmallTest
    public void testFolder_MultipleBookmarks() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        BookmarkId folderId =
                mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "Folder");
        mBookmarkModel.addBookmark(folderId, 0, "Child Bookmark 1", JUnitTestGURLs.URL_1);
        mBookmarkModel.addBookmark(folderId, 1, "Child Bookmark 2", JUnitTestGURLs.URL_2);
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        folderItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER);

        assertMenuStructure(
                list,
                enabledPlural(R.plurals.contextmenu_open_all_plural, 2),
                enabledPlural(R.plurals.contextmenu_open_all_in_new_window_plural, 2),
                enabledPlural(R.plurals.contextmenu_open_all_in_incognito_window_plural, 2),
                enabledPlural(R.plurals.contextmenu_open_all_in_new_tab_group_plural, 2),
                divider(),
                enabled(R.string.contextmenu_edit_bookmark_ellipsis),
                enabled(R.string.bookmark_item_move),
                divider(),
                enabled(R.string.bookmark_item_delete),
                divider(),
                enabled(R.string.contextmenu_add_page),
                enabled(R.string.contextmenu_add_folder),
                divider(),
                enabled(R.string.contextmenu_open_bookmarks_manager),
                enabled(R.string.contextmenu_show_bookmarks_bar));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testFolder_MultipleBookmarks_NtpFeatureEnabled() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        BookmarkId folderId =
                mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "Folder");
        mBookmarkModel.addBookmark(folderId, 0, "Child Bookmark 1", JUnitTestGURLs.URL_1);
        mBookmarkModel.addBookmark(folderId, 1, "Child Bookmark 2", JUnitTestGURLs.URL_2);
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        folderItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER);

        assertMenuStructure(
                list,
                enabledPlural(R.plurals.contextmenu_open_all_plural, 2),
                enabledPlural(R.plurals.contextmenu_open_all_in_new_window_plural, 2),
                enabledPlural(R.plurals.contextmenu_open_all_in_incognito_window_plural, 2),
                enabledPlural(R.plurals.contextmenu_open_all_in_new_tab_group_plural, 2),
                divider(),
                enabled(R.string.contextmenu_edit_bookmark_ellipsis),
                enabled(R.string.bookmark_item_move),
                divider(),
                enabled(R.string.bookmark_item_delete),
                divider(),
                enabled(R.string.contextmenu_add_page),
                enabled(R.string.contextmenu_add_folder),
                divider(),
                enabled(R.string.contextmenu_open_bookmarks_manager),
                divider(),
                enabled(R.string.contextmenu_always_hide_bookmarks_bar),
                enabled(R.string.contextmenu_always_show_bookmarks_bar),
                enabled(R.string.contextmenu_only_show_bookmarks_bar_on_ntp));
    }

    @Test
    @SmallTest
    public void testDesktopRootFolder_DisabledActions() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        BookmarkItem desktopItem =
                mBookmarkModel.getBookmarkById(mBookmarkModel.getDesktopFolderId());
        ModelList list =
                mMediator.buildContextMenuModelList(
                        desktopItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER);

        assertMenuStructure(
                list,
                disabled(R.string.contextmenu_open_all),
                disabled(R.string.contextmenu_open_all_in_new_window),
                disabled(R.string.contextmenu_open_all_in_incognito_window),
                disabled(R.string.contextmenu_open_all_in_new_tab_group),
                divider(),
                disabled(R.string.contextmenu_edit_bookmark_ellipsis),
                disabled(R.string.bookmark_item_move),
                divider(),
                disabled(R.string.bookmark_item_delete),
                divider(),
                enabled(R.string.contextmenu_add_page),
                enabled(R.string.contextmenu_add_folder),
                divider(),
                enabled(R.string.contextmenu_open_bookmarks_manager),
                enabled(R.string.contextmenu_show_bookmarks_bar));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testDesktopRootFolder_DisabledActions_NtpFeatureEnabled() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        BookmarkItem desktopItem =
                mBookmarkModel.getBookmarkById(mBookmarkModel.getDesktopFolderId());
        ModelList list =
                mMediator.buildContextMenuModelList(
                        desktopItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER);

        assertMenuStructure(
                list,
                disabled(R.string.contextmenu_open_all),
                disabled(R.string.contextmenu_open_all_in_new_window),
                disabled(R.string.contextmenu_open_all_in_incognito_window),
                disabled(R.string.contextmenu_open_all_in_new_tab_group),
                divider(),
                disabled(R.string.contextmenu_edit_bookmark_ellipsis),
                disabled(R.string.bookmark_item_move),
                divider(),
                disabled(R.string.bookmark_item_delete),
                divider(),
                enabled(R.string.contextmenu_add_page),
                enabled(R.string.contextmenu_add_folder),
                divider(),
                enabled(R.string.contextmenu_open_bookmarks_manager),
                divider(),
                enabled(R.string.contextmenu_always_hide_bookmarks_bar),
                enabled(R.string.contextmenu_always_show_bookmarks_bar),
                enabled(R.string.contextmenu_only_show_bookmarks_bar_on_ntp));
    }

    @Test
    @SmallTest
    public void testEmptySpaceContextMenu_NtpFeatureDisabled() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        ModelList list = mMediator.buildBookmarksBarEmptySpaceContextMenuModelList(mBookmarkModel);

        assertMenuStructure(
                list,
                disabled(R.string.contextmenu_open_all),
                disabled(R.string.contextmenu_open_all_in_new_window),
                disabled(R.string.contextmenu_open_all_in_incognito_window),
                disabled(R.string.contextmenu_open_all_in_new_tab_group),
                divider(),
                enabled(R.string.contextmenu_add_page),
                enabled(R.string.contextmenu_add_folder),
                divider(),
                enabled(R.string.contextmenu_open_bookmarks_manager),
                enabled(R.string.contextmenu_show_bookmarks_bar));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testEmptySpaceContextMenu_NtpFeatureEnabled() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        ModelList list = mMediator.buildBookmarksBarEmptySpaceContextMenuModelList(mBookmarkModel);

        assertMenuStructure(
                list,
                disabled(R.string.contextmenu_open_all),
                disabled(R.string.contextmenu_open_all_in_new_window),
                disabled(R.string.contextmenu_open_all_in_incognito_window),
                disabled(R.string.contextmenu_open_all_in_new_tab_group),
                divider(),
                enabled(R.string.contextmenu_add_page),
                enabled(R.string.contextmenu_add_folder),
                divider(),
                enabled(R.string.contextmenu_open_bookmarks_manager),
                divider(),
                enabled(R.string.contextmenu_always_hide_bookmarks_bar),
                enabled(R.string.contextmenu_always_show_bookmarks_bar),
                enabled(R.string.contextmenu_only_show_bookmarks_bar_on_ntp));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.BOOKMARKS_BAR_NTP,
        ChromeFeatureList.FLYOUT_IN_BOOKMARKS_BAR
    })
    public void testEmptySpaceContextMenu_NtpAndSubmenuFeatureEnabled() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        ModelList list = mMediator.buildBookmarksBarEmptySpaceContextMenuModelList(mBookmarkModel);

        assertMenuStructure(
                list,
                disabled(R.string.contextmenu_open_all),
                disabled(R.string.contextmenu_open_all_in_new_window),
                disabled(R.string.contextmenu_open_all_in_incognito_window),
                disabled(R.string.contextmenu_open_all_in_new_tab_group),
                divider(),
                enabled(R.string.contextmenu_add_page),
                enabled(R.string.contextmenu_add_folder),
                divider(),
                enabled(R.string.contextmenu_open_bookmarks_manager),
                divider(),
                submenu(R.string.bookmark_bar_settings_title));

        Supplier<List<ListItem>> submenuProvider =
                list.get(list.size() - 1).model.get(ListMenuSubmenuItemProperties.SUBMENU_PROVIDER);
        assertNotNull(submenuProvider);
        List<ListItem> submenuItems = submenuProvider.get();
        assertEquals(3, submenuItems.size());
        assertEquals(
                mActivity.getString(R.string.contextmenu_always_hide_bookmarks_bar),
                submenuItems.get(0).model.get(ListMenuItemProperties.TITLE));
        assertTrue(submenuItems.get(0).model.get(ListMenuItemProperties.CHECKABLE));
        assertTrue(submenuItems.get(0).model.get(ListMenuItemProperties.CHECKED));
        assertEquals(0, submenuItems.get(0).model.get(ListMenuItemProperties.POSITION));
        assertEquals(
                mActivity.getString(R.string.contextmenu_always_show_bookmarks_bar),
                submenuItems.get(1).model.get(ListMenuItemProperties.TITLE));
        assertTrue(submenuItems.get(1).model.get(ListMenuItemProperties.CHECKABLE));
        assertFalse(submenuItems.get(1).model.get(ListMenuItemProperties.CHECKED));
        assertEquals(1, submenuItems.get(1).model.get(ListMenuItemProperties.POSITION));
        assertEquals(
                mActivity.getString(R.string.contextmenu_only_show_bookmarks_bar_on_ntp),
                submenuItems.get(2).model.get(ListMenuItemProperties.TITLE));
        assertTrue(submenuItems.get(2).model.get(ListMenuItemProperties.CHECKABLE));
        assertFalse(submenuItems.get(2).model.get(ListMenuItemProperties.CHECKED));
        assertEquals(2, submenuItems.get(2).model.get(ListMenuItemProperties.POSITION));
    }

    // Tests for actions of the items in the context menu.

    @Test
    @SmallTest
    public void testClickOpenInNewTab() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        bookmarkItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_ITEM);

        click(list, R.string.contextmenu_open_in_new_tab);
        verify(mContextMenuDelegate).openInNewTab(eq(bookmarkId));
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testClickOpenInNewWindow() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        bookmarkItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_ITEM);

        click(list, R.string.contextmenu_open_in_new_window);
        verify(mContextMenuDelegate).openInNewWindow(eq(bookmarkId));
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testClickOpenInIncognitoWindow() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        bookmarkItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_ITEM);

        click(list, R.string.contextmenu_open_in_incognito_window);
        verify(mContextMenuDelegate).openInIncognitoWindow(eq(bookmarkId));
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testClickOpenAll() {
        BookmarkId folderId =
                mBookmarkModel.addFolder(
                        mBookmarkModel.getDesktopFolderId(), 0, "My Special Folder");
        mBookmarkModel.addBookmark(folderId, 0, "Child Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);

        ModelList list =
                mMediator.buildContextMenuModelList(
                        folderItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER);

        clickPlural(list, R.plurals.contextmenu_open_all_plural, 1);
        verify(mContextMenuDelegate).openFolderInNewTabs(eq(folderId));
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testClickOpenAllInNewWindow() {
        BookmarkId folderId =
                mBookmarkModel.addFolder(
                        mBookmarkModel.getDesktopFolderId(), 0, "My Special Folder");
        mBookmarkModel.addBookmark(folderId, 0, "Child Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);

        ModelList list =
                mMediator.buildContextMenuModelList(
                        folderItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER);

        clickPlural(list, R.plurals.contextmenu_open_all_in_new_window_plural, 1);
        verify(mContextMenuDelegate).openFolderInNewWindow(eq(folderId));
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testClickOpenAllInIncognitoWindow() {
        BookmarkId folderId =
                mBookmarkModel.addFolder(
                        mBookmarkModel.getDesktopFolderId(), 0, "My Special Folder");
        mBookmarkModel.addBookmark(folderId, 0, "Child Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);

        ModelList list =
                mMediator.buildContextMenuModelList(
                        folderItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER);

        clickPlural(list, R.plurals.contextmenu_open_all_in_incognito_window_plural, 1);
        verify(mContextMenuDelegate).openFolderInIncognitoWindow(eq(folderId));
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testClickOpenAllInNewTabGroup() {
        BookmarkId folderId =
                mBookmarkModel.addFolder(
                        mBookmarkModel.getDesktopFolderId(), 0, "My Special Folder");
        mBookmarkModel.addBookmark(folderId, 0, "Child Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);

        ModelList list =
                mMediator.buildContextMenuModelList(
                        folderItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER);

        clickPlural(list, R.plurals.contextmenu_open_all_in_new_tab_group_plural, 1);
        verify(mContextMenuDelegate).openFolderInNewTabGroup(eq(folderId), eq("My Special Folder"));
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testClickEdit() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        bookmarkItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_ITEM);

        click(list, R.string.contextmenu_edit_bookmark_ellipsis);
        verify(mContextMenuDelegate).editBookmark(eq(bookmarkId));
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testClickMove() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        bookmarkItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_ITEM);

        click(list, R.string.bookmark_item_move);
        verify(mContextMenuDelegate).moveBookmark(eq(bookmarkId));
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testClickDelete() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        bookmarkItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_ITEM);

        click(list, R.string.bookmark_item_delete);
        verify(mContextMenuDelegate).deleteBookmark(eq(bookmarkId));
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testClickAddPage() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();
        BookmarkId folderId =
                mBookmarkModel.addFolder(
                        mBookmarkModel.getDesktopFolderId(), 0, "My Special Folder");
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        folderItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER);

        click(list, R.string.contextmenu_add_page);
        verify(mContextMenuDelegate).addPage(eq(folderId));
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testClickAddFolder() {
        BookmarkId folderId =
                mBookmarkModel.addFolder(
                        mBookmarkModel.getDesktopFolderId(), 0, "My Special Folder");
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        folderItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER);

        click(list, R.string.contextmenu_add_folder);
        verify(mContextMenuDelegate).addFolder(eq(folderId));
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testClickOpenBookmarksManager() {
        BookmarkId folderId =
                mBookmarkModel.addFolder(
                        mBookmarkModel.getDesktopFolderId(), 0, "My Special Folder");
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);
        ModelList list =
                mMediator.buildContextMenuModelList(
                        folderItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_FOLDER);

        click(list, R.string.contextmenu_open_bookmarks_manager);
        verify(mContextMenuDelegate).openBookmarksManager(eq(folderId));
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testClickShowBookmarksBar() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();
        ModelList list = mMediator.buildBookmarksBarEmptySpaceContextMenuModelList(mBookmarkModel);

        click(list, R.string.contextmenu_show_bookmarks_bar);
        verify(mContextMenuDelegate).toggleBookmarksBar();
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testClickOnlyShowBookmarkBarOnNTP() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();
        ModelList list = mMediator.buildBookmarksBarEmptySpaceContextMenuModelList(mBookmarkModel);

        click(list, R.string.contextmenu_only_show_bookmarks_bar_on_ntp);
        verify(mContextMenuDelegate).setBookmarksBarVisibilityToOnlyShowOnNTP();
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testClickAlwaysHideBookmarksBar() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();
        ModelList list = mMediator.buildBookmarksBarEmptySpaceContextMenuModelList(mBookmarkModel);

        click(list, R.string.contextmenu_always_hide_bookmarks_bar);
        verify(mContextMenuDelegate).setBookmarksBarVisibilityToAlwaysHide();
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testClickAlwaysShowBookmarksBar() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();
        ModelList list = mMediator.buildBookmarksBarEmptySpaceContextMenuModelList(mBookmarkModel);

        click(list, R.string.contextmenu_always_show_bookmarks_bar);
        verify(mContextMenuDelegate).setBookmarksBarVisibilityToAlwaysShow();
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.BOOKMARKS_BAR_NTP,
        ChromeFeatureList.FLYOUT_IN_BOOKMARKS_BAR
    })
    public void testSubmenuClickAlwaysHideBookmarksBar() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();
        ModelList list = mMediator.buildBookmarksBarEmptySpaceContextMenuModelList(mBookmarkModel);

        Supplier<List<ListItem>> submenuProvider =
                list.get(list.size() - 1).model.get(ListMenuSubmenuItemProperties.SUBMENU_PROVIDER);
        assertNotNull(submenuProvider);
        List<ListItem> submenuItems = submenuProvider.get();

        click(submenuItems, R.string.contextmenu_always_hide_bookmarks_bar);
        verify(mContextMenuDelegate).setBookmarksBarVisibilityToAlwaysHide();
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.BOOKMARKS_BAR_NTP,
        ChromeFeatureList.FLYOUT_IN_BOOKMARKS_BAR
    })
    public void testSubmenuClickAlwaysShowBookmarksBar() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();
        ModelList list = mMediator.buildBookmarksBarEmptySpaceContextMenuModelList(mBookmarkModel);

        Supplier<List<ListItem>> submenuProvider =
                list.get(list.size() - 1).model.get(ListMenuSubmenuItemProperties.SUBMENU_PROVIDER);
        assertNotNull(submenuProvider);
        List<ListItem> submenuItems = submenuProvider.get();

        click(submenuItems, R.string.contextmenu_always_show_bookmarks_bar);
        verify(mContextMenuDelegate).setBookmarksBarVisibilityToAlwaysShow();
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.BOOKMARKS_BAR_NTP,
        ChromeFeatureList.FLYOUT_IN_BOOKMARKS_BAR
    })
    public void testSubmenuClickOnlyShowOnNTP() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();
        ModelList list = mMediator.buildBookmarksBarEmptySpaceContextMenuModelList(mBookmarkModel);

        Supplier<List<ListItem>> submenuProvider =
                list.get(list.size() - 1).model.get(ListMenuSubmenuItemProperties.SUBMENU_PROVIDER);
        assertNotNull(submenuProvider);
        List<ListItem> submenuItems = submenuProvider.get();

        click(submenuItems, R.string.contextmenu_only_show_bookmarks_bar_on_ntp);
        verify(mContextMenuDelegate).setBookmarksBarVisibilityToOnlyShowOnNTP();
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testEmptySpaceClickOpenAll() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        ModelList list = mMediator.buildBookmarksBarEmptySpaceContextMenuModelList(mBookmarkModel);

        clickPlural(list, R.plurals.contextmenu_open_all_plural, 1);
        verify(mContextMenuDelegate).openBookmarksInNewTabs(eq(List.of(bookmarkId)));
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testEmptySpaceClickOpenAllInNewWindow() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        ModelList list = mMediator.buildBookmarksBarEmptySpaceContextMenuModelList(mBookmarkModel);

        clickPlural(list, R.plurals.contextmenu_open_all_in_new_window_plural, 1);
        verify(mContextMenuDelegate).openBookmarksInNewWindow(eq(List.of(bookmarkId)));
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testEmptySpaceClickOpenAllInIncognitoWindow() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        ModelList list = mMediator.buildBookmarksBarEmptySpaceContextMenuModelList(mBookmarkModel);

        clickPlural(list, R.plurals.contextmenu_open_all_in_incognito_window_plural, 1);
        verify(mContextMenuDelegate).openBookmarksInIncognitoWindow(eq(List.of(bookmarkId)));
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testEmptySpaceClickOpenAllInNewTabGroup() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        ModelList list = mMediator.buildBookmarksBarEmptySpaceContextMenuModelList(mBookmarkModel);

        clickPlural(list, R.plurals.contextmenu_open_all_in_new_tab_group_plural, 1);
        verify(mContextMenuDelegate)
                .openBookmarksInNewTabGroup(eq(List.of(bookmarkId)), eq("Bookmarks bar"));
        verify(mDismissRunnable).run();
    }

    // Helper methods for performing actions on menu items.

    private void click(Iterable<ListItem> list, int titleResId) {
        PropertyModel item = getMenuItem(list, titleResId);
        String name = mActivity.getResources().getResourceEntryName(titleResId);
        assertNotNull("Cannot click '" + name + "' because the item is null", item);
        item.get(ListMenuItemProperties.CLICK_LISTENER).onClick(null);
    }

    private void clickPlural(Iterable<ListItem> list, int pluralResId, int quantity) {
        PropertyModel item = getMenuItem(list, pluralResId, quantity);
        String name = mActivity.getResources().getResourceEntryName(pluralResId);
        assertNotNull("Cannot click '" + name + "' because the item is null", item);
        item.get(ListMenuItemProperties.CLICK_LISTENER).onClick(null);
    }

    // Helper methods for fetching menu items.

    private PropertyModel getMenuItem(Iterable<ListItem> list, int titleResId) {
        return getMenuItemByTitle(list, mActivity.getString(titleResId));
    }

    private PropertyModel getMenuItem(Iterable<ListItem> list, int pluralResId, int quantity) {
        return getMenuItemByTitle(
                list, mActivity.getResources().getQuantityString(pluralResId, quantity, quantity));
    }

    private PropertyModel getMenuItemByTitle(Iterable<ListItem> list, String expectedTitle) {
        for (ListItem item : list) {
            if (item.model.containsKey(ListMenuItemProperties.TITLE)) {
                String title = item.model.get(ListMenuItemProperties.TITLE).toString();
                if (expectedTitle.equals(title)) {
                    return item.model;
                }
            }
        }
        return null;
    }

    // Helper class and methods to verify presence and ordering of menu items.

    private static class ExpectedItem {
        public final String title;
        public final boolean isDivider;
        public final boolean isSubmenu;
        public final boolean isEnabled;

        ExpectedItem(String title, boolean isDivider, boolean isSubmenu, boolean isEnabled) {
            this.title = title;
            this.isDivider = isDivider;
            this.isSubmenu = isSubmenu;
            this.isEnabled = isEnabled;
        }
    }

    private ExpectedItem divider() {
        return new ExpectedItem(
                /* title= */ null,
                /* isDivider= */ true,
                /* isSubmenu= */ false,
                /* isEnabled= */ false);
    }

    private ExpectedItem enabled(int titleResId) {
        return new ExpectedItem(
                mActivity.getString(titleResId),
                /* isDivider= */ false,
                /* isSubmenu= */ false,
                /* isEnabled= */ true);
    }

    private ExpectedItem disabled(int titleResId) {
        return new ExpectedItem(
                mActivity.getString(titleResId),
                /* isDivider= */ false,
                /* isSubmenu= */ false,
                /* isEnabled= */ false);
    }

    private ExpectedItem enabledPlural(int pluralResId, int quantity) {
        return new ExpectedItem(
                mActivity.getResources().getQuantityString(pluralResId, quantity, quantity),
                /* isDivider= */ false,
                /* isSubmenu= */ false,
                /* isEnabled= */ true);
    }

    private ExpectedItem submenu(int titleResId) {
        return new ExpectedItem(
                mActivity.getString(titleResId),
                /* isDivider= */ false,
                /* isSubmenu= */ true,
                /* isEnabled= */ true);
    }

    private void assertMenuStructure(ModelList list, ExpectedItem... expectedItems) {
        assertEquals("Menu item count mismatch!", expectedItems.length, list.size());

        for (int i = 0; i < expectedItems.length; i++) {
            ExpectedItem expected = expectedItems[i];
            ListItem actual = list.get(i);

            if (expected.isDivider) {
                assertEquals(
                        "Index " + i + " should be a DIVIDER", ListItemType.DIVIDER, actual.type);
            } else if (expected.isSubmenu) {
                assertEquals(
                        "Index " + i + " should be a MENU_ITEM_WITH_SUBMENU",
                        ListItemType.MENU_ITEM_WITH_SUBMENU,
                        actual.type);

                String actualTitle = actual.model.get(ListMenuItemProperties.TITLE).toString();
                assertEquals("Title mismatch at index " + i, expected.title, actualTitle);
                assertEquals(
                        "Enabled state mismatch for: " + expected.title,
                        expected.isEnabled,
                        actual.model.get(ListMenuItemProperties.ENABLED));
            } else {
                assertEquals(
                        "Index " + i + " should be a MENU_ITEM",
                        ListItemType.MENU_ITEM,
                        actual.type);

                String actualTitle = actual.model.get(ListMenuItemProperties.TITLE).toString();
                assertEquals("Title mismatch at index " + i, expected.title, actualTitle);
                assertEquals(
                        "Enabled state mismatch for: " + expected.title,
                        expected.isEnabled,
                        actual.model.get(ListMenuItemProperties.ENABLED));
            }
        }
    }

    @Test
    @SmallTest
    public void testRecordMetrics_EmptySpace() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.BookmarkBar.ContextMenu.EmptySpace.Action",
                                BookmarkBarContextMenuAction.ADD_PAGE)
                        .build();

        ModelList list = mMediator.buildBookmarksBarEmptySpaceContextMenuModelList(mBookmarkModel);
        click(list, R.string.contextmenu_add_page);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordActionMetric() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Bookmarks.BookmarkBar.ContextMenu.BookmarkBarItem.Action",
                                BookmarkBarContextMenuAction.OPEN_IN_NEW_TAB)
                        .build();

        ModelList list =
                mMediator.buildContextMenuModelList(
                        bookmarkItem,
                        mBookmarkModel,
                        BookmarkBarContextMenuEntrypoint.BOOKMARK_BAR_ITEM);
        click(list, R.string.contextmenu_open_in_new_tab);

        histogramWatcher.assertExpected();
    }
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.anyList;
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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.FakeBookmarkModel;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for the {@link BookmarkBarContextMenuMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
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
                        mDismissRunnable);
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
        ModelList list = mMediator.buildContextMenuModelList(bookmarkItem, mBookmarkModel);

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
        ModelList list = mMediator.buildContextMenuModelList(bookmarkItem, mBookmarkModel);

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
                enabled(R.string.contextmenu_always_show_bookmarks_bar));
    }

    @Test
    @SmallTest
    public void testFolder_Empty() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        BookmarkId folderId =
                mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "Empty Folder");
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);
        ModelList list = mMediator.buildContextMenuModelList(folderItem, mBookmarkModel);

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
        ModelList list = mMediator.buildContextMenuModelList(folderItem, mBookmarkModel);

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
                enabled(R.string.contextmenu_always_show_bookmarks_bar));
    }

    @Test
    @SmallTest
    public void testFolder_SingleBookmark() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        BookmarkId folderId =
                mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "Folder");
        mBookmarkModel.addBookmark(folderId, 0, "Child Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);
        ModelList list = mMediator.buildContextMenuModelList(folderItem, mBookmarkModel);

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
        ModelList list = mMediator.buildContextMenuModelList(folderItem, mBookmarkModel);

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
                enabled(R.string.contextmenu_always_show_bookmarks_bar));
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
        ModelList list = mMediator.buildContextMenuModelList(folderItem, mBookmarkModel);

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
        ModelList list = mMediator.buildContextMenuModelList(folderItem, mBookmarkModel);

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
                enabled(R.string.contextmenu_always_show_bookmarks_bar));
    }

    @Test
    @SmallTest
    public void testDesktopRootFolder_DisabledActions() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();

        BookmarkItem desktopItem =
                mBookmarkModel.getBookmarkById(mBookmarkModel.getDesktopFolderId());
        ModelList list = mMediator.buildContextMenuModelList(desktopItem, mBookmarkModel);

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
        ModelList list = mMediator.buildContextMenuModelList(desktopItem, mBookmarkModel);

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
                enabled(R.string.contextmenu_always_show_bookmarks_bar));
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
                enabled(R.string.contextmenu_always_show_bookmarks_bar));
    }

    // Tests for actions of the items in the context menu.

    @Test
    @SmallTest
    public void testClickOpenInNewTab() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
        ModelList list = mMediator.buildContextMenuModelList(bookmarkItem, mBookmarkModel);

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
        ModelList list = mMediator.buildContextMenuModelList(bookmarkItem, mBookmarkModel);

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
        ModelList list = mMediator.buildContextMenuModelList(bookmarkItem, mBookmarkModel);

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

        ModelList list = mMediator.buildContextMenuModelList(folderItem, mBookmarkModel);

        clickPlural(list, R.plurals.contextmenu_open_all_plural, 1);
        verify(mContextMenuDelegate).openAll(anyList());
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

        ModelList list = mMediator.buildContextMenuModelList(folderItem, mBookmarkModel);

        clickPlural(list, R.plurals.contextmenu_open_all_in_new_window_plural, 1);
        verify(mContextMenuDelegate).openAllInNewWindow(anyList());
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

        ModelList list = mMediator.buildContextMenuModelList(folderItem, mBookmarkModel);

        clickPlural(list, R.plurals.contextmenu_open_all_in_incognito_window_plural, 1);
        verify(mContextMenuDelegate).openAllInIncognitoWindow(anyList());
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

        ModelList list = mMediator.buildContextMenuModelList(folderItem, mBookmarkModel);

        clickPlural(list, R.plurals.contextmenu_open_all_in_new_tab_group_plural, 1);
        verify(mContextMenuDelegate).openAllInNewTabGroup(anyList(), eq("My Special Folder"));
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    public void testClickEdit() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
        ModelList list = mMediator.buildContextMenuModelList(bookmarkItem, mBookmarkModel);

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
        ModelList list = mMediator.buildContextMenuModelList(bookmarkItem, mBookmarkModel);

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
        ModelList list = mMediator.buildContextMenuModelList(bookmarkItem, mBookmarkModel);

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
        ModelList list = mMediator.buildContextMenuModelList(folderItem, mBookmarkModel);

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
        ModelList list = mMediator.buildContextMenuModelList(folderItem, mBookmarkModel);

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
        ModelList list = mMediator.buildContextMenuModelList(folderItem, mBookmarkModel);

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
    public void testClickAlwaysHideBookmarksBar() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();
        ModelList list = mMediator.buildBookmarksBarEmptySpaceContextMenuModelList(mBookmarkModel);

        click(list, R.string.contextmenu_always_hide_bookmarks_bar);
        verify(mContextMenuDelegate).toggleBookmarksBar();
        verify(mDismissRunnable).run();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.BOOKMARKS_BAR_NTP)
    public void testClickAlwaysShowBookmarksBar() {
        doReturn(JUnitTestGURLs.URL_1).when(mCurrentTab).getUrl();
        ModelList list = mMediator.buildBookmarksBarEmptySpaceContextMenuModelList(mBookmarkModel);

        click(list, R.string.contextmenu_always_show_bookmarks_bar);
        verify(mContextMenuDelegate).toggleBookmarksBar();
        verify(mDismissRunnable).run();
    }

    // Helper methods for performing actions on menu items.

    private void click(ModelList list, int titleResId) {
        PropertyModel item = getMenuItem(list, titleResId);
        String name = mActivity.getResources().getResourceEntryName(titleResId);
        assertNotNull("Cannot click '" + name + "' because the item is null", item);
        item.get(ListMenuItemProperties.CLICK_LISTENER).onClick(null);
    }

    private void clickPlural(ModelList list, int pluralResId, int quantity) {
        PropertyModel item = getMenuItem(list, pluralResId, quantity);
        String name = mActivity.getResources().getResourceEntryName(pluralResId);
        assertNotNull("Cannot click '" + name + "' because the item is null", item);
        item.get(ListMenuItemProperties.CLICK_LISTENER).onClick(null);
    }

    // Helper methods for fetching menu items.

    private PropertyModel getMenuItem(ModelList list, int titleResId) {
        return getMenuItemByTitle(list, mActivity.getString(titleResId));
    }

    private PropertyModel getMenuItem(ModelList list, int pluralResId, int quantity) {
        return getMenuItemByTitle(
                list, mActivity.getResources().getQuantityString(pluralResId, quantity, quantity));
    }

    private PropertyModel getMenuItemByTitle(ModelList list, String expectedTitle) {
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
        public final boolean isEnabled;

        ExpectedItem(String title, boolean isDivider, boolean isEnabled) {
            this.title = title;
            this.isDivider = isDivider;
            this.isEnabled = isEnabled;
        }
    }

    private ExpectedItem divider() {
        return new ExpectedItem(/* title= */ null, true, false);
    }

    private ExpectedItem enabled(int titleResId) {
        return new ExpectedItem(mActivity.getString(titleResId), false, true);
    }

    private ExpectedItem disabled(int titleResId) {
        return new ExpectedItem(mActivity.getString(titleResId), false, false);
    }

    private ExpectedItem enabledPlural(int pluralResId, int quantity) {
        return new ExpectedItem(
                mActivity.getResources().getQuantityString(pluralResId, quantity, quantity),
                false,
                true);
    }

    private void assertMenuStructure(ModelList list, ExpectedItem... expectedItems) {
        assertEquals("Menu item count mismatch!", expectedItems.length, list.size());

        for (int i = 0; i < expectedItems.length; i++) {
            ExpectedItem expected = expectedItems[i];
            ListItem actual = list.get(i);

            if (expected.isDivider) {
                assertEquals(
                        "Index " + i + " should be a DIVIDER", ListItemType.DIVIDER, actual.type);
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
}

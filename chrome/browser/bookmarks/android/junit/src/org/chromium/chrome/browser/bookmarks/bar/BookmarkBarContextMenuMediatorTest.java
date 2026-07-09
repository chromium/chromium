// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.FakeBookmarkModel;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for the {@link BookmarkBarContextMenuMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarContextMenuMediatorTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mCurrentTab;
    @Mock private Runnable mDismissRunnable;

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
                        mDismissRunnable);
    }

    @Test
    @SmallTest
    public void testBookmarkItem() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getDesktopFolderId(), 0, "Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);

        ModelList list = mMediator.buildContextMenuModelList(bookmarkItem, mBookmarkModel);
        assertNotNull(list);

        PropertyModel openTab = getMenuItem(list, R.string.contextmenu_open_in_new_tab);
        assertNotNull(openTab);
        assertTrue(openTab.get(ListMenuItemProperties.ENABLED));

        PropertyModel openNewWindow = getMenuItem(list, R.string.contextmenu_open_in_new_window);
        assertNotNull(openNewWindow);
        assertTrue(openNewWindow.get(ListMenuItemProperties.ENABLED));

        PropertyModel openIncognito =
                getMenuItem(list, R.string.contextmenu_open_in_incognito_window);
        assertNotNull(openIncognito);
        assertTrue(openIncognito.get(ListMenuItemProperties.ENABLED));

        PropertyModel edit = getMenuItem(list, R.string.contextmenu_edit_bookmark_ellipsis);
        assertNotNull(edit);
        assertTrue(edit.get(ListMenuItemProperties.ENABLED));

        PropertyModel move = getMenuItem(list, R.string.bookmark_item_move);
        assertNotNull(move);
        assertTrue(move.get(ListMenuItemProperties.ENABLED));

        PropertyModel delete = getMenuItem(list, R.string.bookmark_item_delete);
        assertNotNull(delete);
        assertTrue(delete.get(ListMenuItemProperties.ENABLED));
    }

    @Test
    @SmallTest
    public void testFolder_Empty() {
        BookmarkId folderId =
                mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "Empty Folder");
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);

        ModelList list = mMediator.buildContextMenuModelList(folderItem, mBookmarkModel);
        assertNotNull(list);

        // Verify "Open all" options are disabled.
        PropertyModel openTab = getMenuItem(list, R.string.contextmenu_open_all);
        assertNotNull(openTab);
        assertFalse(openTab.get(ListMenuItemProperties.ENABLED));

        // Verify "Edit" / "Move" / "Delete" are enabled for normal folders.
        PropertyModel edit = getMenuItem(list, R.string.contextmenu_edit_bookmark_ellipsis);
        assertNotNull(edit);
        assertTrue(edit.get(ListMenuItemProperties.ENABLED));

        PropertyModel delete = getMenuItem(list, R.string.bookmark_item_delete);
        assertNotNull(delete);
        assertTrue(delete.get(ListMenuItemProperties.ENABLED));
    }

    @Test
    @SmallTest
    public void testFolder_SingleBookmark() {
        BookmarkId folderId =
                mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "Folder");
        mBookmarkModel.addBookmark(folderId, 0, "Child Bookmark", JUnitTestGURLs.URL_1);
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);

        ModelList list = mMediator.buildContextMenuModelList(folderItem, mBookmarkModel);
        assertNotNull(list);

        PropertyModel openAll = getMenuItem(list, R.plurals.contextmenu_open_all_plural, 1);
        assertNotNull(openAll);
        assertTrue(openAll.get(ListMenuItemProperties.ENABLED));

        PropertyModel openNewWindow =
                getMenuItem(list, R.plurals.contextmenu_open_all_in_new_window_plural, 1);
        assertNotNull(openNewWindow);
        assertTrue(openNewWindow.get(ListMenuItemProperties.ENABLED));

        PropertyModel openIncognito =
                getMenuItem(list, R.plurals.contextmenu_open_all_in_incognito_window_plural, 1);
        assertNotNull(openIncognito);
        assertTrue(openIncognito.get(ListMenuItemProperties.ENABLED));

        PropertyModel openTabGroup =
                getMenuItem(list, R.plurals.contextmenu_open_all_in_new_tab_group_plural, 1);
        assertNotNull(openTabGroup);
        assertTrue(openTabGroup.get(ListMenuItemProperties.ENABLED));
    }

    @Test
    @SmallTest
    public void testFolder_MultipleBookmarks() {
        BookmarkId folderId =
                mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "Folder");
        mBookmarkModel.addBookmark(folderId, 0, "Child Bookmark 1", JUnitTestGURLs.URL_1);
        mBookmarkModel.addBookmark(folderId, 1, "Child Bookmark 2", JUnitTestGURLs.URL_2);
        BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);

        ModelList list = mMediator.buildContextMenuModelList(folderItem, mBookmarkModel);
        assertNotNull(list);

        PropertyModel openAll = getMenuItem(list, R.plurals.contextmenu_open_all_plural, 2);
        assertNotNull(openAll);
        assertTrue(openAll.get(ListMenuItemProperties.ENABLED));

        PropertyModel openNewWindow =
                getMenuItem(list, R.plurals.contextmenu_open_all_in_new_window_plural, 2);
        assertNotNull(openNewWindow);
        assertTrue(openNewWindow.get(ListMenuItemProperties.ENABLED));

        PropertyModel openIncognito =
                getMenuItem(list, R.plurals.contextmenu_open_all_in_incognito_window_plural, 2);
        assertNotNull(openIncognito);
        assertTrue(openIncognito.get(ListMenuItemProperties.ENABLED));

        PropertyModel openTabGroup =
                getMenuItem(list, R.plurals.contextmenu_open_all_in_new_tab_group_plural, 2);
        assertNotNull(openTabGroup);
        assertTrue(openTabGroup.get(ListMenuItemProperties.ENABLED));
    }

    @Test
    @SmallTest
    public void testDesktopRootFolder_DisabledActions() {
        BookmarkItem desktopItem =
                mBookmarkModel.getBookmarkById(mBookmarkModel.getDesktopFolderId());

        ModelList list = mMediator.buildContextMenuModelList(desktopItem, mBookmarkModel);
        assertNotNull(list);

        // Verify "Edit" / "Move" / "Delete" are disabled for Bookmarks Bar root folder.
        PropertyModel edit = getMenuItem(list, R.string.contextmenu_edit_bookmark_ellipsis);
        assertNotNull(edit);
        assertFalse(edit.get(ListMenuItemProperties.ENABLED));

        PropertyModel move = getMenuItem(list, R.string.bookmark_item_move);
        assertNotNull(move);
        assertFalse(move.get(ListMenuItemProperties.ENABLED));

        PropertyModel delete = getMenuItem(list, R.string.bookmark_item_delete);
        assertNotNull(delete);
        assertFalse(delete.get(ListMenuItemProperties.ENABLED));
    }

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
}

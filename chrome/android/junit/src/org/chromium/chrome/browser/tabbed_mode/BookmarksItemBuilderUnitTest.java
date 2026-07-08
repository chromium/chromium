// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tabbed_mode.AppMenuUnitTestUtils.assertMenuItemsAreEqual;
import static org.chromium.chrome.browser.tabbed_mode.AppMenuUnitTestUtils.assertMenuTitlesAreEqual;
import static org.chromium.chrome.browser.tabbed_mode.AppMenuUnitTestUtils.findItemById;
import static org.chromium.chrome.browser.tabbed_mode.AppMenuUnitTestUtils.item;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.view.ContextThemeWrapper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.appmenu.AppMenuItemTheme;
import org.chromium.chrome.browser.bookmarks.BookmarkImageFetcher;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.bookmarks.FakeBookmarkModel;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarConstants;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabbed_mode.AppMenuUnitTestUtils.MenuItem;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemWithSubmenuProperties;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link BookmarksItemBuilder}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
public class BookmarksItemBuilderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    @Mock private AppMenuItemTheme mAppMenuItemTheme;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Profile mProfile;
    private FakeBookmarkModel mBookmarkModel;
    @Mock private Tab mTab;

    @Mock private BookmarkImageFetcher mBookmarkImageFetcher;
    private boolean mIsXrFullSpaceMode;

    @Mock
    @SuppressWarnings("MockNotUsedInProduction")
    private PrefService mPrefService;

    @Mock
    @SuppressWarnings("MockNotUsedInProduction")
    private UserPrefs.Natives mUserPrefsNatives;

    private BookmarksItemBuilder mBookmarksItemBuilder;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        TabModel tabModel = Mockito.mock(TabModel.class);
        when(mTabModelSelector.getCurrentModel()).thenReturn(tabModel);
        when(mTabModelSelector.getModel(false)).thenReturn(tabModel);
        when(tabModel.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        mBookmarkModel = FakeBookmarkModel.createModel();
        mBookmarkModel.setEditBookmarksEnabled(true);
        mBookmarkModel.addBookmark(
                mBookmarkModel.getDesktopFolderId(), 0, "Bookmark 1", JUnitTestGURLs.URL_1);
        mBookmarkModel.addBookmark(
                mBookmarkModel.getDesktopFolderId(), 1, "Bookmark 2", JUnitTestGURLs.URL_2);
        BookmarkId folderId =
                mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 2, "Folder 1");
        mBookmarkModel.addBookmark(folderId, 0, "Bookmark in folder 1", JUnitTestGURLs.URL_3);
        mBookmarkModel.addBookmark(folderId, 1, "Bookmark in folder 2", JUnitTestGURLs.SEARCH_URL);

        UserPrefsJni.setInstanceForTesting(mUserPrefsNatives);
        when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);

        mBookmarksItemBuilder =
                new BookmarksItemBuilder(
                        mContext,
                        mAppMenuItemTheme,
                        () -> mBookmarkModel,
                        mTabModelSelector,
                        /* isMenuIconAtStart= */ false,
                        /* shouldShowIconBeforeItem= */ true,
                        /* isXrFullSpaceModeSupplier= */ () -> false);
        mBookmarksItemBuilder.setImageFetcherForTesting(mBookmarkImageFetcher);

        BookmarkBarUtils.setActivityStateBookmarkBarCompatibleForTesting(true);
        DeviceInfo.setIsDesktopForTesting(true);
    }

    private void verifyToggleItemTitle(int expectedTitleResId) {
        ListItem bookmarksParent = mBookmarksItemBuilder.buildBookmarksParentItem(mTab);
        assertNotNull(bookmarksParent);
        List<ListItem> subItems =
                bookmarksParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        ListItem toggleItem = findItemById(subItems, R.id.toggle_bookmarks_bar_menu_id);
        assertNotNull(toggleItem);
        assertEquals(
                ContextUtils.getApplicationContext().getString(expectedTitleResId),
                toggleItem.model.get(AppMenuItemProperties.TITLE));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    public void testToggleBookmarksBarMenuItemString_Desktop() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        DeviceInfo.setIsDesktopForTesting(true);

        // Bookmark bar is visible.
        when(mPrefService.getBoolean(Pref.SHOW_BOOKMARK_BAR)).thenReturn(true);
        verifyToggleItemTitle(R.string.menu_hide_bookmarks_bar);

        // Bookmark bar is hidden.
        when(mPrefService.getBoolean(Pref.SHOW_BOOKMARK_BAR)).thenReturn(false);
        verifyToggleItemTitle(R.string.menu_show_bookmarks_bar);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    public void testToggleBookmarksBarMenuItemString_NonDesktop() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        DeviceInfo.setIsDesktopForTesting(false);

        // Ensure no policy overrides.
        when(mPrefService.isManagedPreference(Pref.SHOW_BOOKMARK_BAR)).thenReturn(false);
        when(mPrefService.hasRecommendation(Pref.SHOW_BOOKMARK_BAR)).thenReturn(false);

        // Bookmark bar is visible (via SharedPreferences).
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR, true)
                .apply();
        verifyToggleItemTitle(R.string.menu_hide_bookmarks_bar);

        // Bookmark bar is hidden (via SharedPreferences).
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR, false)
                .apply();
        verifyToggleItemTitle(R.string.menu_show_bookmarks_bar);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    public void testBookmarkMenu_NoBookmarks() {
        mBookmarkModel.removeAllUserBookmarks();
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        ListItem bookmarksParent = mBookmarksItemBuilder.buildBookmarksParentItem(mTab);
        assertNotNull(bookmarksParent);

        List<ListItem> subItems =
                bookmarksParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        List<MenuItem> expectedSubItems =
                Arrays.asList(
                        item(R.id.bookmark_this_page_menu_id),
                        item(R.id.divider_line_id),
                        item(R.id.all_bookmarks_menu_id),
                        item(
                                R.id.reading_list_parent_menu_id,
                                item(R.id.show_reading_list_menu_id),
                                item(R.id.add_to_reading_list_menu_id)),
                        item(R.id.divider_line_id),
                        item(R.id.toggle_bookmarks_bar_menu_id),
                        item(R.id.divider_line_id),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_folder_menu_id, item(R.id.empty_item_menu_id))),
                        item(R.id.bookmark_folder_menu_id, item(R.id.empty_item_menu_id)));

        assertMenuItemsAreEqual(subItems, expectedSubItems);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    public void testBookmarkMenu_NestedFolders() {
        BookmarkId folderId =
                mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "Folder 2");
        mBookmarkModel.addBookmark(folderId, 0, "Bookmark 1", JUnitTestGURLs.URL_1);
        BookmarkId nestedFolderId = mBookmarkModel.addFolder(folderId, 1, "Nested Folder");
        mBookmarkModel.addBookmark(nestedFolderId, 0, "Nested Bookmark", JUnitTestGURLs.URL_2);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        ListItem bookmarksParent = mBookmarksItemBuilder.buildBookmarksParentItem(mTab);
        assertNotNull(bookmarksParent);

        List<ListItem> subItems =
                bookmarksParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        List<MenuItem> expectedSubItems =
                Arrays.asList(
                        item(R.id.bookmark_this_page_menu_id),
                        item(R.id.divider_line_id),
                        item(R.id.all_bookmarks_menu_id),
                        item(
                                R.id.reading_list_parent_menu_id,
                                item(R.id.show_reading_list_menu_id),
                                item(R.id.add_to_reading_list_menu_id)),
                        item(R.id.divider_line_id),
                        item(R.id.toggle_bookmarks_bar_menu_id),
                        item(R.id.divider_line_id),
                        item(R.id.bookmarks_header_menu_id),
                        item(R.id.bookmark_menu_id),
                        item(R.id.bookmark_menu_id),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_menu_id),
                                item(R.id.bookmark_menu_id)),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_menu_id),
                                item(R.id.bookmark_folder_menu_id, item(R.id.bookmark_menu_id))),
                        item(R.id.divider_line_id),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_folder_menu_id, item(R.id.empty_item_menu_id))),
                        item(R.id.bookmark_folder_menu_id, item(R.id.empty_item_menu_id)));

        assertMenuItemsAreEqual(subItems, expectedSubItems);

        List<MenuItem> expectedTitles =
                Arrays.asList(
                        item(R.string.menu_bookmark_this_page),
                        item(0),
                        item(R.string.menu_bookmarks),
                        item(
                                R.string.menu_reading_list,
                                item(R.string.menu_show_reading_list),
                                item(R.string.menu_add_to_reading_list)),
                        item(0),
                        item(R.string.menu_show_bookmarks_bar),
                        item(0),
                        item(R.string.bookmarks),
                        item("Bookmark 1"),
                        item("Bookmark 2"),
                        item(
                                "Folder 1",
                                item("Bookmark in folder 1"),
                                item("Bookmark in folder 2")),
                        item(
                                "Folder 2",
                                item("Bookmark 1"),
                                item("Nested Folder", item("Nested Bookmark"))),
                        item(0),
                        item(R.string.menu_mobile_bookmarks, item("Partner bookmarks", item(0))),
                        item(R.string.menu_other_bookmarks, item(0)));
        assertMenuTitlesAreEqual(subItems, expectedTitles);

        // Confirm we are using the correct item type.
        ListItem bookmarkItem = subItems.get(8);
        assertEquals(AppMenuHandler.AppMenuItemType.BOOKMARK, bookmarkItem.type);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    public void testBookmarkMenu_EmptyFolder() {
        mBookmarkModel.addFolder(mBookmarkModel.getDesktopFolderId(), 0, "Empty Folder");

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        ListItem bookmarksParent = mBookmarksItemBuilder.buildBookmarksParentItem(mTab);
        assertNotNull(bookmarksParent);

        List<ListItem> subItems =
                bookmarksParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        List<MenuItem> expectedSubItems =
                Arrays.asList(
                        item(R.id.bookmark_this_page_menu_id),
                        item(R.id.divider_line_id),
                        item(R.id.all_bookmarks_menu_id),
                        item(
                                R.id.reading_list_parent_menu_id,
                                item(R.id.show_reading_list_menu_id),
                                item(R.id.add_to_reading_list_menu_id)),
                        item(R.id.divider_line_id),
                        item(R.id.toggle_bookmarks_bar_menu_id),
                        item(R.id.divider_line_id),
                        item(R.id.bookmarks_header_menu_id),
                        item(R.id.bookmark_menu_id),
                        item(R.id.bookmark_menu_id),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_menu_id),
                                item(R.id.bookmark_menu_id)),
                        item(R.id.bookmark_folder_menu_id, item(R.id.empty_item_menu_id)),
                        item(R.id.divider_line_id),
                        item(
                                R.id.bookmark_folder_menu_id,
                                item(R.id.bookmark_folder_menu_id, item(R.id.empty_item_menu_id))),
                        item(R.id.bookmark_folder_menu_id, item(R.id.empty_item_menu_id)));

        assertMenuItemsAreEqual(subItems, expectedSubItems);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SUBMENUS_IN_APP_MENU})
    @SuppressWarnings("unchecked")
    public void testBookmarkMenu_Favicons() {
        BookmarkId bookmarkId = mBookmarkModel.getChildAt(mBookmarkModel.getDesktopFolderId(), 0);
        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        ListItem bookmarksParent = mBookmarksItemBuilder.buildBookmarksParentItem(mTab);
        List<ListItem> subItems =
                bookmarksParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();

        ListItem bookmarkListItem = findItemById(subItems, R.id.bookmark_menu_id);
        assertNotNull(bookmarkListItem);

        LazyOneshotSupplier<Drawable> iconSupplier =
                bookmarkListItem.model.get(AppMenuItemProperties.ICON_SUPPLIER);
        assertNotNull(iconSupplier);

        Drawable mockFavicon = mock(Drawable.class);
        doAnswer(
                        invocation -> {
                            ((Callback<Drawable>) invocation.getArgument(1)).onResult(mockFavicon);
                            return null;
                        })
                .when(mBookmarkImageFetcher)
                .fetchFaviconForBookmark(eq(bookmarkItem), any());

        // Accessing the supplier should trigger the fetch.
        iconSupplier.get();

        verify(mBookmarkImageFetcher).fetchFaviconForBookmark(eq(bookmarkItem), any());
        Drawable actualIcon = iconSupplier.get();
        if (actualIcon instanceof InsetDrawable insetDrawable) {
            actualIcon = insetDrawable.getDrawable();
        }
        assertEquals(mockFavicon, actualIcon);
    }

    @Test
    public void testReadingListMenuItem_Supported() {
        BookmarkUtils.setReadingListSupportedForTesting(true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        ListItem bookmarksParent = mBookmarksItemBuilder.buildBookmarksParentItem(mTab);
        List<ListItem> bookmarksSubItems =
                bookmarksParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        ListItem readingListParent =
                findItemById(bookmarksSubItems, R.id.reading_list_parent_menu_id);
        List<ListItem> subItems =
                readingListParent
                        .model
                        .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                        .get();
        assertNotNull(findItemById(subItems, R.id.add_to_reading_list_menu_id));
        BookmarkUtils.setReadingListSupportedForTesting(null);
    }

    @Test
    public void testReadingListMenuItem_NotSupported() {
        BookmarkUtils.setReadingListSupportedForTesting(false);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        ListItem bookmarksParent = mBookmarksItemBuilder.buildBookmarksParentItem(mTab);
        List<ListItem> bookmarksSubItems =
                bookmarksParent.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER).get();
        ListItem readingListParent =
                findItemById(bookmarksSubItems, R.id.reading_list_parent_menu_id);
        List<ListItem> subItems =
                readingListParent
                        .model
                        .get(AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER)
                        .get();
        assertNull(findItemById(subItems, R.id.add_to_reading_list_menu_id));
        BookmarkUtils.setReadingListSupportedForTesting(null);
    }
}

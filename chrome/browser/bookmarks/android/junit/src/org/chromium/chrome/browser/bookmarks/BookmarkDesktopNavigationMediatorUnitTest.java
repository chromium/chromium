// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.drawable.Drawable;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.navigation_pane.NavigationPaneProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/** Unit tests for {@link BookmarkDesktopNavigationMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, qualifiers = "w1000dp")
public class BookmarkDesktopNavigationMediatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BookmarkDelegate mBookmarkDelegate;

    private Context mContext;
    private FakeBookmarkModel mBookmarkModel;
    private ModelList mModelList;
    private BookmarkDesktopNavigationMediator mMediator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mBookmarkModel = (FakeBookmarkModel) FakeBookmarkModel.createModel();
        mModelList = new ModelList();
        mMediator =
                new BookmarkDesktopNavigationMediator(
                        mContext, mBookmarkModel, mModelList, mBookmarkDelegate);
    }

    @After
    public void tearDown() {
        mMediator.destroy();
        verify(mBookmarkDelegate).removeUiObserver(mMediator);
    }

    @Test
    public void testInitialLoad_localOnly_mobileNotEmpty() {
        // By default, FakeBookmarkModel has local folders, and partner folder in mobile folder.
        // So mobile folder is not empty.
        // Expected order: Bookmarks bar (Desktop), Other bookmarks, Reading list, Mobile bookmarks.
        assertEquals(4, mModelList.size());

        assertFolderItem(
                0,
                mBookmarkModel.getDesktopFolderId(),
                "Bookmarks bar",
                R.drawable.ic_folder_outline_24dp);
        assertFolderItem(
                1,
                mBookmarkModel.getOtherFolderId(),
                "Other bookmarks",
                R.drawable.ic_folder_outline_24dp);
        assertFolderItem(
                2,
                mBookmarkModel.getLocalOrSyncableReadingListFolder(),
                "Reading list",
                R.drawable.ic_reading_list_folder_24dp);
        assertFolderItem(
                3,
                mBookmarkModel.getMobileFolderId(),
                "Mobile bookmarks",
                R.drawable.ic_folder_outline_24dp);
    }

    @Test
    public void testInitialLoad_localOnly_mobileEmpty_hidden() {
        // Delete partner bookmark to make mobile folder empty.
        mBookmarkModel.deleteBookmark(mBookmarkModel.getPartnerFolderId());

        // Mobile folder should be empty now and hidden.
        assertEquals(3, mModelList.size());
        assertFolderItem(
                0,
                mBookmarkModel.getDesktopFolderId(),
                "Bookmarks bar",
                R.drawable.ic_folder_outline_24dp);
        assertFolderItem(
                1,
                mBookmarkModel.getOtherFolderId(),
                "Other bookmarks",
                R.drawable.ic_folder_outline_24dp);
        assertFolderItem(
                2,
                mBookmarkModel.getLocalOrSyncableReadingListFolder(),
                "Reading list",
                R.drawable.ic_reading_list_folder_24dp);
    }

    @Test
    public void testInitialLoad_withAccountFolders() {
        mBookmarkModel.setAreAccountBookmarkFoldersActive(true);
        // Trigger refresh manually because FakeBookmarkModel.setAreAccountBookmarkFoldersActive
        // might not notify observers if it doesn't trigger a model change event in the fake.
        mMediator.bookmarkModelChanged();

        // Expected list:
        // 0. Header: Account Bookmarks
        // 1. Folder: Account Desktop
        // 2. Folder: Account Other
        // 3. Folder: Account Reading List
        // (Account Mobile is empty, so hidden)
        // 4. Header: Local Bookmarks
        // 5. Folder: Local Desktop
        // 6. Folder: Local Other
        // 7. Folder: Local Reading List
        // 8. Folder: Local Mobile (non-empty due to partner)

        assertEquals(9, mModelList.size());

        assertHeaderItem(0, mContext.getString(R.string.account_bookmarks_section_header));
        assertFolderItem(
                1,
                mBookmarkModel.getAccountDesktopFolderId(),
                "Bookmarks bar",
                R.drawable.ic_folder_outline_24dp);
        assertFolderItem(
                2,
                mBookmarkModel.getAccountOtherFolderId(),
                "Other bookmarks",
                R.drawable.ic_folder_outline_24dp);
        assertFolderItem(
                3,
                mBookmarkModel.getAccountReadingListFolder(),
                "Reading list",
                R.drawable.ic_reading_list_folder_24dp);

        assertHeaderItem(4, mContext.getString(R.string.local_bookmarks_section_header));
        assertFolderItem(
                5,
                mBookmarkModel.getDesktopFolderId(),
                "Bookmarks bar",
                R.drawable.ic_folder_outline_24dp);
        assertFolderItem(
                6,
                mBookmarkModel.getOtherFolderId(),
                "Other bookmarks",
                R.drawable.ic_folder_outline_24dp);
        assertFolderItem(
                7,
                mBookmarkModel.getLocalOrSyncableReadingListFolder(),
                "Reading list",
                R.drawable.ic_reading_list_folder_24dp);
        assertFolderItem(
                8,
                mBookmarkModel.getMobileFolderId(),
                "Mobile bookmarks",
                R.drawable.ic_folder_outline_24dp);
    }

    @Test
    public void testClickHandling() {
        var userActionTester = new UserActionTester();
        assertEquals(4, mModelList.size());
        ListItem item = mModelList.get(0);
        Runnable onClick = item.model.get(NavigationPaneProperties.ON_CLICK_HANDLER);
        onClick.run();

        verify(mBookmarkDelegate).openFolder(mBookmarkModel.getDesktopFolderId());
        assertTrue(
                userActionTester
                        .getActions()
                        .contains("MobileBookmarkManagerSidePanelFolderOpened"));
    }

    @Test
    public void testSelectionHighlight() {
        // Initial state: nothing selected
        for (ListItem item : mModelList) {
            if (item.type == NavigationPaneProperties.ITEM_TYPE_NAVIGATION_ITEM) {
                assertFalse(item.model.get(NavigationPaneProperties.IS_SELECTED));
            }
        }

        // Set folder state
        BookmarkId desktopId = mBookmarkModel.getDesktopFolderId();
        mMediator.onFolderStateSet(desktopId);

        // Verify highlight
        assertTrue(mModelList.get(0).model.get(NavigationPaneProperties.IS_SELECTED));
        assertFalse(mModelList.get(1).model.get(NavigationPaneProperties.IS_SELECTED));
        assertFalse(mModelList.get(2).model.get(NavigationPaneProperties.IS_SELECTED));
        assertFalse(mModelList.get(3).model.get(NavigationPaneProperties.IS_SELECTED));
    }

    @Test
    public void testSearchModeClearsSelectionHighlight() {
        // Set folder state first to highlight it
        BookmarkId desktopId = mBookmarkModel.getDesktopFolderId();
        mMediator.onFolderStateSet(desktopId);
        assertTrue(mModelList.get(0).model.get(NavigationPaneProperties.IS_SELECTED));

        // Switch to search mode
        mMediator.onUiModeChanged(BookmarkUiMode.SEARCHING);

        // Verify highlight is cleared
        for (ListItem item : mModelList) {
            if (item.type == NavigationPaneProperties.ITEM_TYPE_NAVIGATION_ITEM) {
                assertFalse(item.model.get(NavigationPaneProperties.IS_SELECTED));
            }
        }

        // Switch back to folder mode (simulated by setting folder state)
        mMediator.onFolderStateSet(desktopId);
        assertTrue(mModelList.get(0).model.get(NavigationPaneProperties.IS_SELECTED));
    }

    @Test
    public void testSelectionHighlight_subFolder() {
        BookmarkId desktopId = mBookmarkModel.getDesktopFolderId();

        // Add sub-folder under Bookmarks Bar
        BookmarkId subFolderId = mBookmarkModel.addFolder(desktopId, 0, "Sub-folder");
        // Add nested sub-folder
        BookmarkId nestedSubFolderId =
                mBookmarkModel.addFolder(subFolderId, 0, "Nested Sub-folder");

        // Navigate to nested sub-folder
        mMediator.onFolderStateSet(nestedSubFolderId);

        // Verify that Bookmarks Bar (index 0) is still highlighted
        assertTrue(mModelList.get(0).model.get(NavigationPaneProperties.IS_SELECTED));
        assertFalse(mModelList.get(1).model.get(NavigationPaneProperties.IS_SELECTED));
        assertFalse(mModelList.get(2).model.get(NavigationPaneProperties.IS_SELECTED));
        assertFalse(mModelList.get(3).model.get(NavigationPaneProperties.IS_SELECTED));
    }

    @Test
    public void testInit_noRedirectFromRoot() {
        // Configure mock delegate for this test
        when(mBookmarkDelegate.getCurrentUiMode()).thenReturn(BookmarkUiMode.FOLDER);
        when(mBookmarkDelegate.getCurrentFolderId()).thenReturn(mBookmarkModel.getRootFolderId());

        // Create a new mediator instance with root folder.
        new BookmarkDesktopNavigationMediator(
                mContext, mBookmarkModel, mModelList, mBookmarkDelegate);

        // Verify that replaceFolder was NOT called.
        verify(mBookmarkDelegate, never()).replaceFolder(any());
        // Verify that while in root folder, nothing is selected.
        assertFalse(mModelList.get(0).model.get(NavigationPaneProperties.IS_SELECTED));
    }

    @Test
    public void testInit_withPreloadedFolder_highlightsImmediately() {
        when(mBookmarkDelegate.getCurrentUiMode()).thenReturn(BookmarkUiMode.FOLDER);
        when(mBookmarkDelegate.getCurrentFolderId())
                .thenReturn(mBookmarkModel.getDesktopFolderId());

        new BookmarkDesktopNavigationMediator(
                mContext, mBookmarkModel, mModelList, mBookmarkDelegate);

        assertTrue(mModelList.get(0).model.get(NavigationPaneProperties.IS_SELECTED));
        assertFalse(mModelList.get(1).model.get(NavigationPaneProperties.IS_SELECTED));
    }

    @Test
    public void testInit_noRedirectFromNonRoot() {
        // Configure mock delegate for this test to start at non-root folder.
        when(mBookmarkDelegate.getCurrentUiMode()).thenReturn(BookmarkUiMode.FOLDER);
        when(mBookmarkDelegate.getCurrentFolderId()).thenReturn(mBookmarkModel.getOtherFolderId());

        new BookmarkDesktopNavigationMediator(
                mContext, mBookmarkModel, mModelList, mBookmarkDelegate);

        verify(mBookmarkDelegate, never()).replaceFolder(any());
        assertTrue(mModelList.get(1).model.get(NavigationPaneProperties.IS_SELECTED));
    }

    @Test
    public void testOnFolderStateSet_noRedirectFromRoot() {
        mMediator.onFolderStateSet(mBookmarkModel.getRootFolderId());
        verify(mBookmarkDelegate, never()).replaceFolder(any());
        assertFalse(mModelList.get(0).model.get(NavigationPaneProperties.IS_SELECTED));

        mMediator.onFolderStateSet(mBookmarkModel.getDesktopFolderId());
        assertTrue(mModelList.get(0).model.get(NavigationPaneProperties.IS_SELECTED));
    }

    @Test
    public void testOnFolderStateSet_rootFolder_nothingSelected() {
        mMediator.onFolderStateSet(mBookmarkModel.getDesktopFolderId());
        assertTrue(mModelList.get(0).model.get(NavigationPaneProperties.IS_SELECTED));

        mMediator.onFolderStateSet(mBookmarkModel.getRootFolderId());
        for (ListItem item : mModelList) {
            if (item.type == NavigationPaneProperties.ITEM_TYPE_NAVIGATION_ITEM) {
                assertFalse(item.model.get(NavigationPaneProperties.IS_SELECTED));
            }
        }
    }

    @Test
    @Config(qualifiers = "w600dp")
    public void testOnFolderStateSet_noRedirectOnSmallScreen() {
        mMediator.onFolderStateSet(mBookmarkModel.getRootFolderId());
        verify(mBookmarkDelegate, never()).replaceFolder(any());
        assertFalse(mModelList.get(0).model.get(NavigationPaneProperties.IS_SELECTED));
    }

    @Test
    @Config(qualifiers = "w600dp")
    public void testOnConfigurationChanged_noRedirectWhenTransitioningToWide() {
        mMediator.onFolderStateSet(mBookmarkModel.getRootFolderId());
        verify(mBookmarkDelegate, never()).replaceFolder(any());

        Configuration wideConfig = new Configuration();
        wideConfig.screenWidthDp = 1000;
        mMediator.onConfigurationChanged(wideConfig);
        verify(mBookmarkDelegate, never()).replaceFolder(any());
        assertFalse(mModelList.get(0).model.get(NavigationPaneProperties.IS_SELECTED));
    }

    @Test
    @Config(qualifiers = "w600dp")
    public void testOnConfigurationChanged_noRedirectWhenRemainingOnSmallScreen() {
        mMediator.onFolderStateSet(mBookmarkModel.getRootFolderId());
        verify(mBookmarkDelegate, never()).replaceFolder(any());

        Configuration smallConfig = new Configuration();
        smallConfig.screenWidthDp = 600;
        mMediator.onConfigurationChanged(smallConfig);
        verify(mBookmarkDelegate, never()).replaceFolder(any());
    }

    @Test
    @Config(qualifiers = "w600dp")
    public void testOnConfigurationChanged_noRedirectFromNonRoot() {
        mMediator.onFolderStateSet(mBookmarkModel.getOtherFolderId());

        Configuration wideConfig = new Configuration();
        wideConfig.screenWidthDp = 1000;
        mMediator.onConfigurationChanged(wideConfig);
        verify(mBookmarkDelegate, never()).replaceFolder(any());
    }

    @Test
    public void testOnFolderStateSet_withStaleAccountFolder_noRedirect() {
        // Start with active account folders.
        mBookmarkModel.setAreAccountBookmarkFoldersActive(true);
        mMediator.bookmarkModelChanged();

        // Account desktop folder is currently the first folder item.
        BookmarkId accountDesktopId = mBookmarkModel.getAccountDesktopFolderId();
        assertEquals(
                accountDesktopId,
                mModelList.get(1).model.get(BookmarkDesktopNavigationProperties.BOOKMARK_ID));

        // Simulate sign-out: account bookmark folders become inactive/removed in the model.
        mBookmarkModel.setAreAccountBookmarkFoldersActive(false);

        // BookmarkManagerMediator falls back to root folder and calls onFolderStateSet(root).
        mMediator.onFolderStateSet(mBookmarkModel.getRootFolderId());

        // Verify that replaceFolder was NOT called.
        verify(mBookmarkDelegate, never()).replaceFolder(any());
    }

    @Test
    public void testInit_whenModelNotLoaded_doesNotCrashAndInitializesOnModelLoaded() {
        BookmarkModel mockBookmarkModel = mock(BookmarkModel.class);
        when(mockBookmarkModel.isBookmarkModelLoaded()).thenReturn(false);

        ModelList modelList = new ModelList();
        BookmarkDesktopNavigationMediator mediator =
                new BookmarkDesktopNavigationMediator(
                        mContext, mockBookmarkModel, modelList, mBookmarkDelegate);

        // When model is not loaded, modelList is empty and no native methods are invoked.
        assertTrue(modelList.isEmpty());
        verify(mockBookmarkModel, never()).getRootFolderId();

        // When model finishes loading:
        when(mockBookmarkModel.isBookmarkModelLoaded()).thenReturn(true);
        when(mockBookmarkModel.getRootFolderId())
                .thenReturn(new BookmarkId(0, BookmarkType.NORMAL));
        mediator.bookmarkModelChanged();

        verify(mockBookmarkModel).getTopLevelFolderIds(anyInt());
    }

    private void assertFolderItem(
            int index, BookmarkId expectedId, String expectedTitle, int expectedIconRes) {
        ListItem item = mModelList.get(index);
        assertEquals(NavigationPaneProperties.ITEM_TYPE_NAVIGATION_ITEM, item.type);
        assertEquals(expectedId, item.model.get(BookmarkDesktopNavigationProperties.BOOKMARK_ID));
        assertEquals(expectedTitle, item.model.get(NavigationPaneProperties.TITLE));
        Drawable icon = item.model.get(NavigationPaneProperties.ICON);
        assertEquals(expectedIconRes, Shadows.shadowOf(icon).getCreatedFromResId());
    }

    private void assertHeaderItem(int index, String expectedTitle) {
        ListItem item = mModelList.get(index);
        assertEquals(NavigationPaneProperties.ITEM_TYPE_HEADER, item.type);
        assertEquals(expectedTitle, item.model.get(NavigationPaneProperties.HEADER_TITLE));
    }
}

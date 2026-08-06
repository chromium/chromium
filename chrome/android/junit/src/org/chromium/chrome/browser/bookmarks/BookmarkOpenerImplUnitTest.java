// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Browser;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link BookmarkOpenerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkOpenerImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BookmarkModel mBookmarkModel;

    private Activity mActivity;
    private BookmarkOpenerImpl mOpener;
    private BookmarkId mBookmarkId1;
    private BookmarkItem mBookmarkItem1;
    private BookmarkId mBookmarkId2;
    private BookmarkItem mBookmarkItem2;
    private BookmarkId mBookmarkId3;
    private BookmarkItem mBookmarkItem3;
    private BookmarkId mFolderId;
    private BookmarkId mNestedFolderId;
    private BookmarkItem mNestedFolderItem;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();

        mBookmarkId1 = new BookmarkId(1, BookmarkType.NORMAL);
        mBookmarkItem1 =
                new BookmarkItem(
                        mBookmarkId1,
                        "Title",
                        new GURL("https://example.com"),
                        /* isFolder= */ false,
                        /* parentId= */ null,
                        /* isEditable= */ true,
                        /* isManaged= */ false,
                        /* dateAdded= */ 0,
                        /* read= */ false,
                        /* dateLastOpened= */ 0,
                        /* isAccountBookmark= */ false);

        mBookmarkId2 = new BookmarkId(2, BookmarkType.NORMAL);
        mBookmarkItem2 =
                new BookmarkItem(
                        mBookmarkId2,
                        "Title 2",
                        new GURL("https://example2.com"),
                        /* isFolder= */ false,
                        /* parentId= */ null,
                        /* isEditable= */ true,
                        /* isManaged= */ false,
                        /* dateAdded= */ 0,
                        /* read= */ false,
                        /* dateLastOpened= */ 0,
                        /* isAccountBookmark= */ false);

        mBookmarkId3 = new BookmarkId(3, BookmarkType.NORMAL);
        mBookmarkItem3 =
                new BookmarkItem(
                        mBookmarkId3,
                        "Title 3",
                        new GURL("https://example3.com"),
                        /* isFolder= */ false,
                        /* parentId= */ null,
                        /* isEditable= */ true,
                        /* isManaged= */ false,
                        /* dateAdded= */ 0,
                        /* read= */ false,
                        /* dateLastOpened= */ 0,
                        /* isAccountBookmark= */ false);

        mFolderId = new BookmarkId(4, BookmarkType.NORMAL);
        mNestedFolderId = new BookmarkId(5, BookmarkType.NORMAL);
        mNestedFolderItem =
                new BookmarkItem(
                        mNestedFolderId,
                        "Nested Folder",
                        null,
                        /* isFolder= */ true,
                        mFolderId,
                        /* isEditable= */ true,
                        /* isManaged= */ false,
                        /* dateAdded= */ 0,
                        /* read= */ false,
                        /* dateLastOpened= */ 0,
                        /* isAccountBookmark= */ false);

        when(mBookmarkModel.getBookmarkById(mBookmarkId1)).thenReturn(mBookmarkItem1);
        when(mBookmarkModel.getBookmarkById(mBookmarkId2)).thenReturn(mBookmarkItem2);
        when(mBookmarkModel.getBookmarkById(mBookmarkId3)).thenReturn(mBookmarkItem3);
        when(mBookmarkModel.getBookmarkById(mNestedFolderId)).thenReturn(mNestedFolderItem);
        when(mBookmarkModel.getChildIds(mFolderId))
                .thenReturn(
                        Arrays.asList(mBookmarkId1, mBookmarkId2, mNestedFolderId, mBookmarkId3));

        mOpener =
                new BookmarkOpenerImpl(
                        () -> mBookmarkModel,
                        mActivity,
                        new ComponentName(mActivity, "TestActivity"));
    }

    @Test
    public void testOpenBookmarkInCurrentTab() {
        assertTrue(mOpener.openBookmarkInCurrentTab(mBookmarkId1, false));

        Intent startedIntent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(startedIntent);
    }

    @Test
    public void testOpenBookmarksInNewTabs() {
        assertTrue(
                mOpener.openBookmarksInNewTabs(
                        Collections.singletonList(mBookmarkId1), /* incognito= */ false));

        Intent startedIntent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(startedIntent);
    }

    @Test
    public void testOpenBookmarksInNewTabGroupWithTitle() {
        String testTitle = "Custom Folder Title";
        assertTrue(
                mOpener.openBookmarksInNewTabGroup(
                        Collections.singletonList(mBookmarkId1),
                        /* incognito= */ false,
                        testTitle));

        Intent startedIntent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(startedIntent);
        assertEquals(
                (Integer) TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                IntentHandler.getTabLaunchType(startedIntent));
        assertTrue(
                startedIntent.getBooleanExtra(
                        IntentHandler.EXTRA_OPEN_ADDITIONAL_URLS_IN_TAB_GROUP, false));
        assertEquals(testTitle, startedIntent.getStringExtra(IntentHandler.EXTRA_TAB_GROUP_TITLE));
    }

    @Test
    public void testOpenBookmarksInNewTabGroupWithoutTitle() {
        assertTrue(
                mOpener.openBookmarksInNewTabGroup(
                        Collections.singletonList(mBookmarkId1),
                        /* incognito= */ false,
                        /* title= */ null));

        Intent startedIntent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(startedIntent);
        assertEquals(
                (Integer) TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                IntentHandler.getTabLaunchType(startedIntent));
        assertTrue(
                startedIntent.getBooleanExtra(
                        IntentHandler.EXTRA_OPEN_ADDITIONAL_URLS_IN_TAB_GROUP, false));
        assertTrue(!startedIntent.hasExtra(IntentHandler.EXTRA_TAB_GROUP_TITLE));
    }

    @Test
    public void testOpenBookmarksInNewTabs_WithTitleButNonGroupLaunchType() {
        String testTitle = "Custom Folder Title";
        Bundle extras = new Bundle();
        extras.putString(IntentHandler.EXTRA_TAB_GROUP_TITLE, testTitle);
        assertTrue(
                mOpener.openBookmarksInNewTabs(
                        Collections.singletonList(mBookmarkId1),
                        /* incognito= */ false,
                        TabLaunchType.FROM_LINK,
                        extras));

        Intent startedIntent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(startedIntent);
        assertEquals(
                (Integer) TabLaunchType.FROM_LINK, IntentHandler.getTabLaunchType(startedIntent));
        assertFalse(
                startedIntent.getBooleanExtra(
                        IntentHandler.EXTRA_OPEN_ADDITIONAL_URLS_IN_TAB_GROUP, false));
        assertEquals(testTitle, startedIntent.getStringExtra(IntentHandler.EXTRA_TAB_GROUP_TITLE));
    }

    @Test
    public void testOpenBookmarksInNewWindow() {
        assertTrue(
                mOpener.openBookmarksInNewWindow(
                        Collections.singletonList(mBookmarkId1), /* incognito= */ false));

        Intent startedIntent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(startedIntent);
        assertTrue(startedIntent.getBooleanExtra(Browser.EXTRA_CREATE_NEW_TAB, false));
        assertFalse(
                startedIntent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW, true));
        assertFalse(
                startedIntent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true));

        assertTrue(
                mOpener.openBookmarksInNewWindow(
                        Collections.singletonList(mBookmarkId1), /* incognito= */ true));

        startedIntent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(startedIntent);
        assertTrue(startedIntent.getBooleanExtra(Browser.EXTRA_CREATE_NEW_TAB, false));
        assertTrue(
                startedIntent.getBooleanExtra(
                        IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW, false));
        assertTrue(
                startedIntent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false));
    }

    @Test
    public void testOpenFolderBookmarksInNewTabs() {
        assertTrue(
                mOpener.openFolderBookmarksInNewTabs(
                        mFolderId,
                        /* incognito= */ false,
                        TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND));

        Intent startedIntent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(startedIntent);
        assertTrue(
                startedIntent.getBooleanExtra(
                        IntentHandler.EXTRA_DISABLE_INITIALIZE_RENDERER, false));
    }

    @Test
    public void testExtractBookmarkChildrenFromFolder() {
        List<BookmarkId> children = mOpener.extractBookmarkChildrenFromFolder(mFolderId);
        // Original order: mBookmarkId1, mBookmarkId2, mNestedFolderId, mBookmarkId3
        // Non-folders: mBookmarkId1, mBookmarkId2, mBookmarkId3
        // Result should be reversed order
        assertEquals(3, children.size());
        assertEquals(mBookmarkId3, children.get(0));
        assertEquals(mBookmarkId2, children.get(1));
        assertEquals(mBookmarkId1, children.get(2));
    }
}

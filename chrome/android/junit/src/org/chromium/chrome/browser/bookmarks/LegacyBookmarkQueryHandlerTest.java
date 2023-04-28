// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.sync.SyncService.SyncStateChangedListener;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link LegacyBookmarkQueryHandler}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LegacyBookmarkQueryHandlerTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    private SyncService mSyncService;
    @Mock
    Tracker mTracker;
    @Mock
    Profile mProfile;

    @Captor
    private ArgumentCaptor<Runnable> mFinishLoadingBookmarkModelCaptor;
    @Captor
    private ArgumentCaptor<SyncStateChangedListener> mSyncStateChangedListenerCaptor;

    // Used to make sure all the bookmark ids are different.
    private static int sId;

    // TODO(https://crbug.com/1439403): Make this BookmarkModel setup shareable between tests.
    private static final BookmarkId ROOT_BOOKMARK_ID = new BookmarkId(sId++, BookmarkType.NORMAL);
    private static final BookmarkId DESKTOP_BOOKMARK_ID =
            new BookmarkId(sId++, BookmarkType.NORMAL);
    private static final BookmarkId OTHER_BOOKMARK_ID = new BookmarkId(sId++, BookmarkType.NORMAL);
    private static final BookmarkId MOBILE_BOOKMARK_ID = new BookmarkId(sId++, BookmarkType.NORMAL);
    private static final BookmarkId READING_LIST_BOOKMARK_ID =
            new BookmarkId(sId++, BookmarkType.READING_LIST);

    private static final BookmarkId FOLDER_BOOKMARK_ID_A =
            new BookmarkId(sId++, BookmarkType.NORMAL);
    private static final BookmarkId URL_BOOKMARK_ID_A = new BookmarkId(sId++, BookmarkType.NORMAL);
    private static final BookmarkId URL_BOOKMARK_ID_B = new BookmarkId(sId++, BookmarkType.NORMAL);
    private static final BookmarkId URL_BOOKMARK_ID_C = new BookmarkId(sId++, BookmarkType.NORMAL);
    private static final BookmarkId URL_BOOKMARK_ID_D =
            new BookmarkId(sId++, BookmarkType.READING_LIST);
    private static final BookmarkId URL_BOOKMARK_ID_E =
            new BookmarkId(sId++, BookmarkType.READING_LIST);

    private static final GURL URL_A = new GURL("https://www.a.com/");
    private static final GURL URL_B = new GURL("https://www.b.com/");
    private static final GURL URL_C = new GURL("https://www.c.com/");
    private static final GURL URL_D = new GURL("https://www.d.com/");
    private static final GURL URL_E = new GURL("https://www.e.com/");

    private static final BookmarkItem DESKTOP_BOOKMARK_ITEM = new BookmarkItem(DESKTOP_BOOKMARK_ID,
            "Bookmarks bar", null, true, ROOT_BOOKMARK_ID, false, false, 0, false);
    private static final BookmarkItem OTHER_BOOKMARK_ITEM = new BookmarkItem(OTHER_BOOKMARK_ID,
            "Other bookmarks", null, true, ROOT_BOOKMARK_ID, false, false, 0, false);
    private static final BookmarkItem MOBILE_BOOKMARK_ITEM = new BookmarkItem(MOBILE_BOOKMARK_ID,
            "Mobile bookmarks", null, true, ROOT_BOOKMARK_ID, false, false, 0, false);
    private static final BookmarkItem READING_LIST_ITEM = new BookmarkItem(READING_LIST_BOOKMARK_ID,
            "Reading list", null, true, ROOT_BOOKMARK_ID, false, false, 0, false);
    private static final BookmarkItem FOLDER_ITEM_A = new BookmarkItem(FOLDER_BOOKMARK_ID_A,
            "Folder A", null, true, MOBILE_BOOKMARK_ID, true, false, 0, false);
    private static final BookmarkItem URL_ITEM_A = new BookmarkItem(
            URL_BOOKMARK_ID_A, "Url A", URL_A, false, MOBILE_BOOKMARK_ID, true, false, 0, false);
    private static final BookmarkItem URL_ITEM_B = new BookmarkItem(
            URL_BOOKMARK_ID_B, "Url B", URL_B, false, FOLDER_BOOKMARK_ID_A, true, false, 0, false);
    private static final BookmarkItem URL_ITEM_C = new BookmarkItem(
            URL_BOOKMARK_ID_C, "Url C", URL_C, false, FOLDER_BOOKMARK_ID_A, true, false, 0, false);
    private static final BookmarkItem URL_ITEM_D = new BookmarkItem(URL_BOOKMARK_ID_D, "Url D",
            URL_D, false, READING_LIST_BOOKMARK_ID, true, false, 0, true);
    private static final BookmarkItem URL_ITEM_E = new BookmarkItem(URL_BOOKMARK_ID_E, "Url E",
            URL_E, false, READING_LIST_BOOKMARK_ID, true, false, 0, false);

    @Before
    public void setup() {
        SyncService.overrideForTests(mSyncService);
        TrackerFactory.setTrackerForTests(mTracker);
        Profile.setLastUsedProfileForTesting(mProfile);

        doReturn(ROOT_BOOKMARK_ID).when(mBookmarkModel).getRootFolderId();
        doReturn(DESKTOP_BOOKMARK_ID).when(mBookmarkModel).getDesktopFolderId();
        doReturn(OTHER_BOOKMARK_ID).when(mBookmarkModel).getOtherFolderId();
        doReturn(MOBILE_BOOKMARK_ID).when(mBookmarkModel).getMobileFolderId();
        doReturn(Collections.singletonList(READING_LIST_BOOKMARK_ID))
                .when(mBookmarkModel)
                .getTopLevelFolderIds(/*getSpecial*/ true, /*getNormal*/ false);

        doReturn(DESKTOP_BOOKMARK_ITEM).when(mBookmarkModel).getBookmarkById(DESKTOP_BOOKMARK_ID);
        doReturn(OTHER_BOOKMARK_ITEM).when(mBookmarkModel).getBookmarkById(OTHER_BOOKMARK_ID);
        doReturn(MOBILE_BOOKMARK_ITEM).when(mBookmarkModel).getBookmarkById(MOBILE_BOOKMARK_ID);
        doReturn(READING_LIST_ITEM).when(mBookmarkModel).getBookmarkById(READING_LIST_BOOKMARK_ID);
        doReturn(FOLDER_ITEM_A).when(mBookmarkModel).getBookmarkById(FOLDER_BOOKMARK_ID_A);
        doReturn(URL_ITEM_A).when(mBookmarkModel).getBookmarkById(URL_BOOKMARK_ID_A);
        doReturn(URL_ITEM_B).when(mBookmarkModel).getBookmarkById(URL_BOOKMARK_ID_B);
        doReturn(URL_ITEM_C).when(mBookmarkModel).getBookmarkById(URL_BOOKMARK_ID_C);
        doReturn(URL_ITEM_D).when(mBookmarkModel).getBookmarkById(URL_BOOKMARK_ID_D);
        doReturn(URL_ITEM_E).when(mBookmarkModel).getBookmarkById(URL_BOOKMARK_ID_E);

        doReturn(true).when(mBookmarkModel).isFolderVisible(DESKTOP_BOOKMARK_ID);
        doReturn(false).when(mBookmarkModel).isFolderVisible(OTHER_BOOKMARK_ID);
        doReturn(true).when(mBookmarkModel).isFolderVisible(MOBILE_BOOKMARK_ID);
        doReturn(Arrays.asList(FOLDER_BOOKMARK_ID_A, URL_BOOKMARK_ID_A))
                .when(mBookmarkModel)
                .getChildIds(MOBILE_BOOKMARK_ID);
        doReturn(Arrays.asList(URL_BOOKMARK_ID_B, URL_BOOKMARK_ID_C))
                .when(mBookmarkModel)
                .getChildIds(BookmarkId.SHOPPING_FOLDER);
        ShoppingSpecifics shoppingSpecifics =
                ShoppingSpecifics.newBuilder().setIsPriceTracked(true).build();
        PowerBookmarkMeta powerBookmarkMeta =
                PowerBookmarkMeta.newBuilder().setShoppingSpecifics(shoppingSpecifics).build();
        doReturn(powerBookmarkMeta).when(mBookmarkModel).getPowerBookmarkMeta(URL_BOOKMARK_ID_B);
        doReturn(Arrays.asList(URL_BOOKMARK_ID_D, URL_BOOKMARK_ID_E))
                .when(mBookmarkModel)
                .getChildIds(READING_LIST_BOOKMARK_ID);
    }

    @Test
    public void testDestroy() {
        BookmarkQueryHandler bookmarkQueryHandler = new LegacyBookmarkQueryHandler(mBookmarkModel);
        verify(mSyncService).addSyncStateChangedListener(any());

        bookmarkQueryHandler.destroy();
        verify(mSyncService).removeSyncStateChangedListener(any());
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder_isFolderVisible() {
        BookmarkQueryHandler bookmarkQueryHandler = new LegacyBookmarkQueryHandler(mBookmarkModel);
        verify(mBookmarkModel)
                .finishLoadingBookmarkModel(mFinishLoadingBookmarkModelCaptor.capture());
        doReturn(true).when(mBookmarkModel).isBookmarkModelLoaded();
        mFinishLoadingBookmarkModelCaptor.getValue().run();

        List<BookmarkListEntry> result =
                bookmarkQueryHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID);
        Assert.assertEquals(3, result.size());

        doReturn(true).when(mBookmarkModel).isFolderVisible(OTHER_BOOKMARK_ID);
        verify(mSyncService).addSyncStateChangedListener(mSyncStateChangedListenerCaptor.capture());
        mSyncStateChangedListenerCaptor.getValue().syncStateChanged();

        List<BookmarkListEntry> updatedResult =
                bookmarkQueryHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID);
        Assert.assertEquals(4, updatedResult.size());
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder_isBookmarkModelLoaded() {
        BookmarkQueryHandler bookmarkQueryHandler = new LegacyBookmarkQueryHandler(mBookmarkModel);
        verify(mSyncService).addSyncStateChangedListener(mSyncStateChangedListenerCaptor.capture());
        mSyncStateChangedListenerCaptor.getValue().syncStateChanged();

        Assert.assertTrue(
                bookmarkQueryHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID).isEmpty());

        doReturn(true).when(mBookmarkModel).isBookmarkModelLoaded();
        mSyncStateChangedListenerCaptor.getValue().syncStateChanged();

        Assert.assertFalse(
                bookmarkQueryHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID).isEmpty());
    }

    @Test
    public void testBuildBookmarkListForParent_nonRootFolder() {
        BookmarkQueryHandler bookmarkQueryHandler = new LegacyBookmarkQueryHandler(mBookmarkModel);
        List<BookmarkListEntry> result =
                bookmarkQueryHandler.buildBookmarkListForParent(MOBILE_BOOKMARK_ID);

        Assert.assertEquals(2, result.size());
        Assert.assertEquals(FOLDER_BOOKMARK_ID_A, result.get(0).getBookmarkItem().getId());
        Assert.assertEquals(URL_BOOKMARK_ID_A, result.get(1).getBookmarkItem().getId());
    }

    @Test
    public void testBuildBookmarkListForParent_shopping() {
        BookmarkQueryHandler bookmarkQueryHandler = new LegacyBookmarkQueryHandler(mBookmarkModel);
        List<BookmarkListEntry> result =
                bookmarkQueryHandler.buildBookmarkListForParent(BookmarkId.SHOPPING_FOLDER);

        // Both URL_BOOKMARK_ID_B and URL_BOOKMARK_ID_C will be returned as children of
        // BookmarkId.SHOPPING_FOLDER , but only URL_BOOKMARK_ID_B will have a correct meta.
        Assert.assertEquals(1, result.size());
        Assert.assertEquals(URL_BOOKMARK_ID_B, result.get(0).getBookmarkItem().getId());
    }

    @Test
    public void testBuildBookmarkListForParent_readingList() {
        BookmarkQueryHandler bookmarkQueryHandler = new LegacyBookmarkQueryHandler(mBookmarkModel);
        List<BookmarkListEntry> result =
                bookmarkQueryHandler.buildBookmarkListForParent(READING_LIST_BOOKMARK_ID);

        Assert.assertEquals(4, result.size());
        // While the getChildIds call will return [D, E], due to the read status, they should get
        // flipped around to show the unread E first. Headers will also be inserted.
        Assert.assertEquals(ViewType.SECTION_HEADER, result.get(0).getViewType());
        Assert.assertEquals(URL_BOOKMARK_ID_E, result.get(1).getBookmarkItem().getId());
        Assert.assertEquals(ViewType.SECTION_HEADER, result.get(2).getViewType());
        Assert.assertEquals(URL_BOOKMARK_ID_D, result.get(3).getBookmarkItem().getId());
    }

    @Test
    public void testBuildBookmarkListForSearch() {
        BookmarkQueryHandler bookmarkQueryHandler = new LegacyBookmarkQueryHandler(mBookmarkModel);
        verify(mBookmarkModel)
                .finishLoadingBookmarkModel(mFinishLoadingBookmarkModelCaptor.capture());
        mFinishLoadingBookmarkModelCaptor.getValue().run();

        doReturn(Arrays.asList(FOLDER_BOOKMARK_ID_A, URL_BOOKMARK_ID_A))
                .when(mBookmarkModel)
                .searchBookmarks("A", 500);
        List<BookmarkListEntry> result = bookmarkQueryHandler.buildBookmarkListForSearch("A");
        Assert.assertEquals(2, result.size());
    }
}

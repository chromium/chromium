// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.DESKTOP_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.FOLDER_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.READING_LIST_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.ROOT_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_B;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_C;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_D;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_E;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_F;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_G;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_H;

import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowSortOrder;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.PowerBookmarkType;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link ImprovedBookmarkQueryHandler}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImprovedBookmarkQueryHandlerTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    private Tracker mTracker;
    @Mock
    private Profile mProfile;
    @Mock
    private BookmarkUiPrefs mBookmarkUiPrefs;

    private ImprovedBookmarkQueryHandler mHandler;

    @Before
    public void setup() {
        Profile.setLastUsedProfileForTesting(mProfile);
        TrackerFactory.setTrackerForTests(mTracker);
        SharedBookmarkModelMocks.initMocks(mBookmarkModel);

        mHandler = new ImprovedBookmarkQueryHandler(mBookmarkModel, mBookmarkUiPrefs);
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder_chronological() {
        doReturn(BookmarkRowSortOrder.CHRONOLOGICAL)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();

        List<BookmarkListEntry> result = mHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID);
        List<BookmarkId> expected =
                Arrays.asList(DESKTOP_BOOKMARK_ID, READING_LIST_BOOKMARK_ID, FOLDER_BOOKMARK_ID_A,
                        URL_BOOKMARK_ID_A, URL_BOOKMARK_ID_F, URL_BOOKMARK_ID_G, URL_BOOKMARK_ID_H);
        verifyBookmarkIds(expected, result);
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder_reverseChronological() {
        doReturn(BookmarkRowSortOrder.REVERSE_CHRONOLOGICAL)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();

        List<BookmarkListEntry> result = mHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID);
        List<BookmarkId> expected =
                Arrays.asList(FOLDER_BOOKMARK_ID_A, READING_LIST_BOOKMARK_ID, DESKTOP_BOOKMARK_ID,
                        URL_BOOKMARK_ID_H, URL_BOOKMARK_ID_G, URL_BOOKMARK_ID_F, URL_BOOKMARK_ID_A);
        verifyBookmarkIds(expected, result);
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder_alphabetical() {
        doReturn(BookmarkRowSortOrder.ALPHABETICAL)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();

        List<BookmarkListEntry> result = mHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID);
        List<BookmarkId> expected =
                Arrays.asList(DESKTOP_BOOKMARK_ID, FOLDER_BOOKMARK_ID_A, READING_LIST_BOOKMARK_ID,
                        URL_BOOKMARK_ID_A, URL_BOOKMARK_ID_F, URL_BOOKMARK_ID_G, URL_BOOKMARK_ID_H);
        verifyBookmarkIds(expected, result);
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder_reverseAlphabetical() {
        doReturn(BookmarkRowSortOrder.REVERSE_ALPHABETICAL)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();

        List<BookmarkListEntry> result = mHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID);
        List<BookmarkId> expected =
                Arrays.asList(READING_LIST_BOOKMARK_ID, FOLDER_BOOKMARK_ID_A, DESKTOP_BOOKMARK_ID,
                        URL_BOOKMARK_ID_H, URL_BOOKMARK_ID_G, URL_BOOKMARK_ID_F, URL_BOOKMARK_ID_A);
        verifyBookmarkIds(expected, result);
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder_recentlyUsed() {
        doReturn(BookmarkRowSortOrder.RECENTLY_USED)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();

        List<BookmarkListEntry> result = mHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID);
        List<BookmarkId> expected =
                Arrays.asList(DESKTOP_BOOKMARK_ID, READING_LIST_BOOKMARK_ID, FOLDER_BOOKMARK_ID_A,
                        URL_BOOKMARK_ID_H, URL_BOOKMARK_ID_G, URL_BOOKMARK_ID_F, URL_BOOKMARK_ID_A);
        verifyBookmarkIds(expected, result);
    }

    @Test
    public void testBuildBookmarkListForParent_readingList() {
        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForParent(READING_LIST_BOOKMARK_ID);
        List<BookmarkId> expected = Arrays.asList(null, URL_BOOKMARK_ID_E, null, URL_BOOKMARK_ID_D);
        verifyBookmarkIds(expected, result);
        verify(mBookmarkUiPrefs, never()).getBookmarkRowSortOrder();
    }

    @Test
    public void testBuildBookmarkListForParent() {
        doReturn(BookmarkRowSortOrder.ALPHABETICAL)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();
        // Order these initially in a non-alphabetical order.
        List<BookmarkId> queryIds = Arrays.asList(URL_BOOKMARK_ID_D, URL_BOOKMARK_ID_A,
                URL_BOOKMARK_ID_C, URL_BOOKMARK_ID_B, URL_BOOKMARK_ID_E);
        doReturn(queryIds)
                .when(mBookmarkModel)
                .searchBookmarks(ArgumentMatchers.any(), ArgumentMatchers.anyInt());

        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForSearch("Url", Collections.emptySet());
        List<BookmarkId> expected = Arrays.asList(URL_BOOKMARK_ID_A, URL_BOOKMARK_ID_B,
                URL_BOOKMARK_ID_C, URL_BOOKMARK_ID_D, URL_BOOKMARK_ID_E);
        verifyBookmarkIds(expected, result);
    }

    @Test
    public void testSearchWithShoppingFilter() {
        List<BookmarkId> queryIds = Arrays.asList(URL_BOOKMARK_ID_A, URL_BOOKMARK_ID_B);
        doReturn(queryIds)
                .when(mBookmarkModel)
                .searchBookmarks(ArgumentMatchers.any(), ArgumentMatchers.anyInt());
        PowerBookmarkMeta metaWithShopping =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(ShoppingSpecifics.newBuilder().build())
                        .build();
        doReturn(metaWithShopping).when(mBookmarkModel).getPowerBookmarkMeta(URL_BOOKMARK_ID_A);
        PowerBookmarkMeta metaWithoutShopping = PowerBookmarkMeta.newBuilder().build();
        doReturn(metaWithoutShopping).when(mBookmarkModel).getPowerBookmarkMeta(URL_BOOKMARK_ID_B);

        List<BookmarkListEntry> result = mHandler.buildBookmarkListForSearch(
                "", Collections.singleton(PowerBookmarkType.SHOPPING));
        verifyBookmarkIds(Collections.singletonList(URL_BOOKMARK_ID_A), result);
    }

    private void verifyBookmarkIds(
            List<BookmarkId> expectedList, List<BookmarkListEntry> actualList) {
        assertEquals("Lists differ in size", expectedList.size(), actualList.size());
        for (int i = 0; i < expectedList.size(); ++i) {
            final @Nullable BookmarkId expectedId = expectedList.get(i);
            final @Nullable BookmarkItem actualBookmarkItem = actualList.get(i).getBookmarkItem();
            final @Nullable BookmarkId actualBookmarkId =
                    actualBookmarkItem == null ? null : actualBookmarkItem.getId();
            assertEquals("Mismatch at index: " + i, expectedId, actualBookmarkId);
        }
    }
}

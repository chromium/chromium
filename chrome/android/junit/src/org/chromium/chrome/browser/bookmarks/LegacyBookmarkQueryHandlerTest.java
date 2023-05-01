// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.FOLDER_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.MOBILE_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.OTHER_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.READING_LIST_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.ROOT_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_B;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_D;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_E;

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
import org.chromium.components.feature_engagement.Tracker;

import java.util.Arrays;
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

    @Before
    public void setup() {
        SyncService.overrideForTests(mSyncService);
        TrackerFactory.setTrackerForTests(mTracker);
        Profile.setLastUsedProfileForTesting(mProfile);
        SharedBookmarkModelMocks.initMocks(mBookmarkModel);
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

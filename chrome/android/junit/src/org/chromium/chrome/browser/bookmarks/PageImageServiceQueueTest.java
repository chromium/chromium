// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

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

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link PageImageServiceQueue}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PageImageServiceQueueTest {
    private static final GURL URL_1 = new GURL("https://test1.com");
    private static final GURL URL_2 = new GURL("https://test2.com");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BookmarkModel mBookmarkModel;
    @Mock private Callback<GURL> mBookmarkUrlCallback;
    @Mock private Callback<GURL> mQueuedBookmarkUrlCallback;
    @Captor private ArgumentCaptor<Callback<GURL>> mGurlCallbackCaptor;

    private PageImageServiceQueue mPageImageServiceQueue;

    private BookmarkItem mBookmarkItem1 =
            new BookmarkItem(
                    new BookmarkId(1, BookmarkType.NORMAL),
                    "Bookmark1",
                    URL_1,
                    false,
                    null,
                    true,
                    false,
                    0,
                    false,
                    0,
                    false);
    private BookmarkItem mBookmarkItem2 =
            new BookmarkItem(
                    new BookmarkId(2, BookmarkType.NORMAL),
                    "Bookmark2",
                    URL_2,
                    false,
                    null,
                    true,
                    false,
                    0,
                    false,
                    0,
                    false);

    private BookmarkItem mAccountBookmarkItem =
            new BookmarkItem(
                    new BookmarkId(3, BookmarkType.NORMAL),
                    "Bookmark1",
                    URL_1,
                    false,
                    null,
                    true,
                    false,
                    0,
                    false,
                    0,
                    false);

    @Before
    public void setUp() {
        mPageImageServiceQueue =
                new PageImageServiceQueue(mBookmarkModel, /* maxFetchRequests= */ 1);
    }

    @Test
    public void testQueuedRequests() {
        // Our limit is 1 for testing.
        mPageImageServiceQueue.getSalientImageUrl(mBookmarkItem1, mBookmarkUrlCallback);

        // Then add one more.
        mPageImageServiceQueue.getSalientImageUrl(mBookmarkItem2, mQueuedBookmarkUrlCallback);

        verify(mBookmarkModel)
                .getImageUrlForBookmark(
                        eq(mBookmarkItem1.getUrl()),
                        eq(mBookmarkItem1.isAccountBookmark()),
                        mGurlCallbackCaptor.capture());
        // Run the 1st callback and verify that the queued one is executed.
        mGurlCallbackCaptor.getValue().onResult(null); // value here doesn't matter.
        verify(mBookmarkModel)
                .getImageUrlForBookmark(
                        eq(mBookmarkItem2.getUrl()), eq(mBookmarkItem2.isAccountBookmark()), any());
    }

    @Test
    public void testCachedRequest() {
        doCallback(2, (Callback<GURL> callback) -> callback.onResult(JUnitTestGURLs.EXAMPLE_URL))
                .when(mBookmarkModel)
                .getImageUrlForBookmark(any(), anyBoolean(), any());

        mPageImageServiceQueue.getSalientImageUrl(mBookmarkItem1, mBookmarkUrlCallback);
        verify(mBookmarkUrlCallback).onResult(any());
        verify(mBookmarkModel, times(1)).getImageUrlForBookmark(any(), anyBoolean(), any());

        // The result from URL_1 should be in the cache.
        mPageImageServiceQueue.getSalientImageUrl(mBookmarkItem1, mQueuedBookmarkUrlCallback);
        verify(mQueuedBookmarkUrlCallback).onResult(any());
        // The value should have been cached and bookmark model only queried for the 1st request.
        verify(mBookmarkModel, times(1)).getImageUrlForBookmark(any(), anyBoolean(), any());
    }

    @Test
    public void testCachedRunsPending() {
        reset(mBookmarkModel);
        mPageImageServiceQueue.getSalientImageUrl(mBookmarkItem1, mBookmarkUrlCallback);
        mPageImageServiceQueue.getSalientImageUrl(mBookmarkItem1, mBookmarkUrlCallback);
        mPageImageServiceQueue.getSalientImageUrl(mBookmarkItem2, mBookmarkUrlCallback);
        verify(mBookmarkModel)
                .getImageUrlForBookmark(
                        eq(mBookmarkItem1.getUrl()),
                        eq(mBookmarkItem1.isAccountBookmark()),
                        mGurlCallbackCaptor.capture());

        // As soon as the first request runs, the second should synchronously execute as well since
        // it's satisfiable by just the cache. The third request should show up in the callbacks.
        mGurlCallbackCaptor.getValue().onResult(JUnitTestGURLs.EXAMPLE_URL);
        verify(mBookmarkUrlCallback, times(2)).onResult(any());
        verify(mBookmarkModel)
                .getImageUrlForBookmark(
                        eq(mBookmarkItem2.getUrl()),
                        eq(mBookmarkItem2.isAccountBookmark()),
                        mGurlCallbackCaptor.capture());

        mGurlCallbackCaptor.getValue().onResult(JUnitTestGURLs.EXAMPLE_URL);
        verify(mBookmarkUrlCallback, times(3)).onResult(any());
    }

    @Test
    public void testAccountAndLocalRequestSameUrl() {
        mPageImageServiceQueue.getSalientImageUrl(mAccountBookmarkItem, mBookmarkUrlCallback);
        verify(mBookmarkModel)
                .getImageUrlForBookmark(
                        eq(mAccountBookmarkItem.getUrl()),
                        eq(mAccountBookmarkItem.isAccountBookmark()),
                        mGurlCallbackCaptor.capture());
        mGurlCallbackCaptor.getValue().onResult(null); // value here doesn't matter.

        mPageImageServiceQueue.getSalientImageUrl(mBookmarkItem1, mBookmarkUrlCallback);
        // The cached account result shouldn't be used.
        verify(mBookmarkModel, times(2))
                .getImageUrlForBookmark(
                        eq(mBookmarkItem1.getUrl()),
                        eq(mBookmarkItem1.isAccountBookmark()),
                        mGurlCallbackCaptor.capture());
    }
}

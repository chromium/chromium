// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.any;
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
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link PageImageServiceQueue}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PageImageServiceQueueTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BookmarkModel mBookmarkModel;
    @Mock private Callback<GURL> mBookmarkUrlCallback;
    @Mock private Callback<GURL> mQueuedBookmarkUrlCallback;
    @Captor private ArgumentCaptor<Callback<GURL>> mGurlCallbackCaptor;

    private PageImageServiceQueue mPageImageServiceQueue;

    @Before
    public void setUp() {
        mPageImageServiceQueue =
                new PageImageServiceQueue(mBookmarkModel, /* maxFetchRequests= */ 1);
    }

    @Test
    public void testQueuedRequests() {
        // Our limit is 1 for testing.
        mPageImageServiceQueue.getSalientImageUrl(JUnitTestGURLs.URL_1, mBookmarkUrlCallback);

        // Then add one more.
        mPageImageServiceQueue.getSalientImageUrl(JUnitTestGURLs.URL_2, mQueuedBookmarkUrlCallback);

        verify(mBookmarkModel)
                .getImageUrlForBookmark(eq(JUnitTestGURLs.URL_1), mGurlCallbackCaptor.capture());
        // Run the 1st callback and verify that the queued one is executed.
        mGurlCallbackCaptor.getValue().onResult(null); // value here doesn't matter.
        verify(mBookmarkModel).getImageUrlForBookmark(eq(JUnitTestGURLs.URL_2), any());
    }

    @Test
    public void testCachedRequest() {
        doCallback(1, (Callback<GURL> callback) -> callback.onResult(JUnitTestGURLs.EXAMPLE_URL))
                .when(mBookmarkModel)
                .getImageUrlForBookmark(any(), any());

        mPageImageServiceQueue.getSalientImageUrl(JUnitTestGURLs.URL_1, mBookmarkUrlCallback);
        verify(mBookmarkUrlCallback).onResult(any());
        verify(mBookmarkModel, times(1)).getImageUrlForBookmark(any(), any());

        // The result from URL_1 should be in the cache.
        mPageImageServiceQueue.getSalientImageUrl(JUnitTestGURLs.URL_1, mQueuedBookmarkUrlCallback);
        verify(mQueuedBookmarkUrlCallback).onResult(any());
        // The value should have been cached and bookmark model only queried for the 1st request.
        verify(mBookmarkModel, times(1)).getImageUrlForBookmark(any(), any());
    }

    @Test
    public void testCachedRunsPending() {
        reset(mBookmarkModel);
        mPageImageServiceQueue.getSalientImageUrl(JUnitTestGURLs.URL_1, mBookmarkUrlCallback);
        mPageImageServiceQueue.getSalientImageUrl(JUnitTestGURLs.URL_1, mBookmarkUrlCallback);
        mPageImageServiceQueue.getSalientImageUrl(JUnitTestGURLs.URL_2, mBookmarkUrlCallback);
        verify(mBookmarkModel)
                .getImageUrlForBookmark(eq(JUnitTestGURLs.URL_1), mGurlCallbackCaptor.capture());

        // As soon as the first request runs, the second should synchronously execute as well since
        // it's satisfiable by just the cache. The third request should show up in the callbacks.
        mGurlCallbackCaptor.getValue().onResult(JUnitTestGURLs.EXAMPLE_URL);
        verify(mBookmarkUrlCallback, times(2)).onResult(any());
        verify(mBookmarkModel)
                .getImageUrlForBookmark(eq(JUnitTestGURLs.URL_2), mGurlCallbackCaptor.capture());

        mGurlCallbackCaptor.getValue().onResult(JUnitTestGURLs.EXAMPLE_URL);
        verify(mBookmarkUrlCallback, times(3)).onResult(any());
    }
}

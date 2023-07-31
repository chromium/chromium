// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.test.util.MockitoHelper;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link PageImageServiceQueue}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PageImageServiceQueueTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    private Callback<GURL> mBookmarkUrlCallback;
    @Mock
    private Callback<GURL> mQueuedBookmarkUrlCallback;

    private PageImageServiceQueue mPageImageServiceQueue;

    @Before
    public void setUp() {
        // Setup bookmark url fetching.
        MockitoHelper
                .doCallback(1,
                        (Callback<GURL> callback) -> {
                            callback.onResult(JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL));
                        })
                .when(mBookmarkModel)
                .getImageUrlForBookmark(any(), any());

        mPageImageServiceQueue = new PageImageServiceQueue(mBookmarkModel);
    }

    @Test
    public void testQueuedRequests() {
        List<Callback<GURL>> urlCallbacks = new ArrayList<>();
        // Intentionally do nothing so the requests stack up.
        MockitoHelper.doCallback(1, (Callback<GURL> callback) -> {
                         urlCallbacks.add(callback);
                     }).when(mBookmarkModel).getImageUrlForBookmark(any(), any());

        // Add requests up to the limit.
        for (int i = 0; i < PageImageServiceQueue.MAX_FETCH_REQUESTS; i++) {
            mPageImageServiceQueue.getSalientImageUrl(
                    JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL), mBookmarkUrlCallback);
        }

        // Then add one more.
        mPageImageServiceQueue.getSalientImageUrl(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL), mQueuedBookmarkUrlCallback);

        verify(mBookmarkModel, times(PageImageServiceQueue.MAX_FETCH_REQUESTS))
                .getImageUrlForBookmark(any(), any());
        // Run the 1st callback and verify that the queued one is executed.
        urlCallbacks.get(0).onResult(null); // value here doesn't matter.
        verify(mBookmarkModel, times(PageImageServiceQueue.MAX_FETCH_REQUESTS + 1))
                .getImageUrlForBookmark(any(), any());
    }
}

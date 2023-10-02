// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
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
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.test.util.MockitoHelper;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Collections;
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
    @Mock
    private SyncService mSyncService;

    private PageImageServiceQueue mPageImageServiceQueue;

    @Before
    public void setUp() {
        // Setup bookmark url fetching.
        MockitoHelper
                .doCallback(1,
                        (Callback<GURL> callback) -> {
                            callback.onResult(JUnitTestGURLs.EXAMPLE_URL);
                        })
                .when(mBookmarkModel)
                .getImageUrlForBookmark(any(), any());

        doReturn(true).when(mSyncService).isSyncFeatureActive();
        doReturn(Collections.singleton(ModelType.BOOKMARKS))
                .when(mSyncService)
                .getActiveDataTypes();
        mPageImageServiceQueue =
                new PageImageServiceQueue(mBookmarkModel, /*maxFetchRequests*/ 1, mSyncService);
    }

    @Test
    public void testQueuedRequests() {
        List<Callback<GURL>> urlCallbacks = new ArrayList<>();
        // Intentionally do nothing so the requests stack up.
        MockitoHelper.doCallback(1, (Callback<GURL> callback) -> {
                         urlCallbacks.add(callback);
                     }).when(mBookmarkModel).getImageUrlForBookmark(any(), any());

        // Our limit is 1 for testing.
        mPageImageServiceQueue.getSalientImageUrl(JUnitTestGURLs.URL_1, mBookmarkUrlCallback);

        // Then add one more.
        mPageImageServiceQueue.getSalientImageUrl(JUnitTestGURLs.URL_2, mQueuedBookmarkUrlCallback);

        verify(mBookmarkModel, times(1)).getImageUrlForBookmark(any(), any());
        // Run the 1st callback and verify that the queued one is executed.
        urlCallbacks.get(0).onResult(null); // value here doesn't matter.
        verify(mBookmarkModel, times(2)).getImageUrlForBookmark(any(), any());
    }

    @Test
    public void testCachedRequest() {
        mPageImageServiceQueue.getSalientImageUrl(JUnitTestGURLs.URL_1, mBookmarkUrlCallback);

        // The result from URL_1 should be in the queue
        mPageImageServiceQueue.getSalientImageUrl(JUnitTestGURLs.URL_1, mQueuedBookmarkUrlCallback);

        verify(mBookmarkUrlCallback).onResult(any());
        verify(mQueuedBookmarkUrlCallback).onResult(any());
        // The value should have been cached and bookmark model only queried for the 1st request.
        verify(mBookmarkModel, times(1)).getImageUrlForBookmark(any(), any());
    }

    @Test
    public void testRequest_syncNotEnabled() {
        doReturn(false).when(mSyncService).isSyncFeatureActive();
        mPageImageServiceQueue.getSalientImageUrl(JUnitTestGURLs.URL_1, mBookmarkUrlCallback);
        verify(mBookmarkUrlCallback).onResult(null);
    }

    @Test
    public void testRequest_bookmarksDataTypeNotActive() {
        doReturn(Collections.emptySet()).when(mSyncService).getActiveDataTypes();
        mPageImageServiceQueue.getSalientImageUrl(JUnitTestGURLs.URL_1, mBookmarkUrlCallback);
        verify(mBookmarkUrlCallback).onResult(null);
    }
}

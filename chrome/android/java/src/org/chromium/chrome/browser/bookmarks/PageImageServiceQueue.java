// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;


import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.LinkedList;
import java.util.Map;
import java.util.Queue;

/** Class which encapsulates making/queuing requests to {@link PageImageService}. */
public class PageImageServiceQueue {
    // Max number of parallel requests we send to the page image service.
    @VisibleForTesting static final int DEFAULT_MAX_FETCH_REQUESTS = 30;

    private class Request {
        public final BookmarkItem mBookmarkItem;
        public final Callback<GURL> mCallback;

        public Request(@NonNull BookmarkItem bookmarkItem, @NonNull Callback<GURL> callback) {
            mBookmarkItem = bookmarkItem;
            mCallback = callback;
        }
    }

    // Cache the results for repeated queries to avoid extra calls through the JNI/network.
    private final Map<BookmarkId, GURL> mSalientImageUrlCache = new HashMap<>();
    private final CallbackController mCallbackController = new CallbackController();
    private final Queue<Request> mQueuedRequests = new LinkedList<>();
    private final BookmarkModel mBookmarkModel;
    private final int mMaxFetchRequests;

    private int mOutstandingRequestCount;

    public PageImageServiceQueue(BookmarkModel bookmarkModel) {
        this(bookmarkModel, DEFAULT_MAX_FETCH_REQUESTS);
    }

    public PageImageServiceQueue(BookmarkModel bookmarkModel, int maxFetchRequests) {
        mBookmarkModel = bookmarkModel;
        mMaxFetchRequests = maxFetchRequests;
    }

    public void destroy() {
        mCallbackController.destroy();
    }

    /**
     * Returns the url for the salient image for the given {@link BookmarkItem}. Salient images are
     * only available if the client is syncing.
     *
     * @param bookmarkItem The item to fetch the salient image url for.
     * @param callback The callback to receive the result.
     */
    public void getSalientImageUrl(
            @NonNull BookmarkItem bookmarkItem, @NonNull Callback<GURL> callback) {
        BookmarkId bookmarkId = bookmarkItem.getId();
        if (mSalientImageUrlCache.containsKey(bookmarkId)) {
            callback.onResult(mSalientImageUrlCache.get(bookmarkId));
            return;
        }

        if (fullOnOutstandingRequests()) {
            mQueuedRequests.add(new Request(bookmarkItem, callback));
            return;
        }

        mOutstandingRequestCount++;
        mBookmarkModel.getImageUrlForBookmark(
                bookmarkItem.getUrl(),
                bookmarkItem.isAccountBookmark(),
                mCallbackController.makeCancelable(
                        salientImageUrl -> {
                            mSalientImageUrlCache.put(bookmarkId, salientImageUrl);
                            callback.onResult(salientImageUrl);

                            mOutstandingRequestCount--;
                            startNextPending();
                        }));
    }

    private void startNextPending() {
        // If a pending request doesn't trigger an outstanding request, such as from a cache hit,
        // then keep going.
        while (!fullOnOutstandingRequests() && !mQueuedRequests.isEmpty()) {
            Request queuedRequest = mQueuedRequests.poll();
            getSalientImageUrl(queuedRequest.mBookmarkItem, queuedRequest.mCallback);
        }
    }

    private boolean fullOnOutstandingRequests() {
        return mOutstandingRequestCount >= mMaxFetchRequests;
    }
}

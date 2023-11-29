// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.LinkedList;
import java.util.Map;
import java.util.Queue;

/** Class which encapsulates making/queuing requests to {@link PageImageService}. */
public class PageImageServiceQueue {
    // Max number of parallel requests we send to the page image service.
    @VisibleForTesting static final int DEFAULT_MAX_FETCH_REQUESTS = 30;

    // Cache the results for repeated queries to avoid extra calls through the JNI/network.
    private final Map<GURL, GURL> mSalientImageUrlCache = new HashMap<>();
    private final CallbackController mCallbackController = new CallbackController();
    private final Queue<Pair<GURL, Callback<GURL>>> mQueuedRequests = new LinkedList<>();
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
     * Given the {@link GURL} for a webpage, returns a {@link GURL} for the salient image. If the
     * url is cached, the callback is invoked immediately.
     */
    public void getSalientImageUrl(@NonNull GURL url, @NonNull Callback<GURL> callback) {
        if (mSalientImageUrlCache.containsKey(url)) {
            callback.onResult(mSalientImageUrlCache.get(url));
            return;
        }

        if (fullOnOutstandingRequests()) {
            mQueuedRequests.add(new Pair<>(url, callback));
            return;
        }

        mOutstandingRequestCount++;
        mBookmarkModel.getImageUrlForBookmark(
                url,
                mCallbackController.makeCancelable(
                        salientImageUrl -> {
                            mSalientImageUrlCache.put(url, salientImageUrl);
                            callback.onResult(salientImageUrl);

                            mOutstandingRequestCount--;
                            startNextPending();
                        }));
    }

    private void startNextPending() {
        // If a pending request doesn't trigger an outstanding request, such as from a cache hit,
        // then keep going.
        while (!fullOnOutstandingRequests() && !mQueuedRequests.isEmpty()) {
            Pair<GURL, Callback<GURL>> queuedRequest = mQueuedRequests.poll();
            getSalientImageUrl(queuedRequest.first, queuedRequest.second);
        }
    }

    private boolean fullOnOutstandingRequests() {
        return mOutstandingRequestCount >= mMaxFetchRequests;
    }
}

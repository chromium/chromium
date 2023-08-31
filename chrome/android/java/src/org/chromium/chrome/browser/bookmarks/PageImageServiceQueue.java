// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.util.Pair;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Queue;

/** Class which encapsulates making/queuing requests to {@link PageImageService}. */
public class PageImageServiceQueue {
    // Max number of parallel requests we send to the page image service.
    @VisibleForTesting
    static final int DEFAULT_MAX_FETCH_REQUESTS = 30;

    // Cache the results for repeated queries to avoid extra calls through the JNI/network.
    private final Map<GURL, GURL> mSalientImageUrlCache = new HashMap<>();
    private final CallbackController mCallbackController = new CallbackController();
    private final Queue<Pair<GURL, Callback<GURL>>> mQueuedRequests = new LinkedList<>();
    private final List<Callback<GURL>> mRequests = new LinkedList<>();
    private final BookmarkModel mBookmarkModel;
    private final int mMaxFetchRequests;

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
    public void getSalientImageUrl(GURL url, Callback<GURL> callback) {
        if (mSalientImageUrlCache.containsKey(url)) {
            callback.onResult(mSalientImageUrlCache.get(url));
            return;
        }

        if (mRequests.size() >= mMaxFetchRequests) {
            mQueuedRequests.add(new Pair<>(url, callback));
            return;
        }

        mRequests.add(callback);
        mBookmarkModel.getImageUrlForBookmark(
                url, mCallbackController.makeCancelable(salientImageUrl -> {
                    mSalientImageUrlCache.put(url, salientImageUrl);
                    callback.onResult(salientImageUrl);

                    mRequests.remove(callback);
                    Pair<GURL, Callback<GURL>> queuedRequest = mQueuedRequests.poll();
                    if (queuedRequest != null) {
                        getSalientImageUrl(queuedRequest.first, queuedRequest.second);
                    }
                }));
    }
}

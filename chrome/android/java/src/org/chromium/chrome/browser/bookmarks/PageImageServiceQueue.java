// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.util.Pair;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.url.GURL;

import java.util.LinkedList;
import java.util.List;
import java.util.Queue;

/** Class which encapsulates making/queuing requests to {@link PageImageService}. */
public class PageImageServiceQueue {
    // Max number of parallel requests we send to the page image service.
    @VisibleForTesting
    static final int MAX_FETCH_REQUESTS = 30;

    private final CallbackController mCallbackController = new CallbackController();
    private final Queue<Pair<GURL, Callback<GURL>>> mQueuedRequests = new LinkedList<>();
    private final List<Callback<GURL>> mRequests = new LinkedList<>();
    private final BookmarkModel mBookmarkModel;

    public PageImageServiceQueue(BookmarkModel bookmarkModel) {
        mBookmarkModel = bookmarkModel;
    }

    public void destroy() {
        mCallbackController.destroy();
    }

    /**
     * Given the {@link GURL} for a webpage, returns a {@link GURL} for the salient image.
     */
    public void getSalientImageUrl(GURL url, Callback<GURL> callback) {
        if (mRequests.size() >= MAX_FETCH_REQUESTS) {
            mQueuedRequests.add(new Pair<>(url, callback));
            return;
        }

        mRequests.add(callback);
        mBookmarkModel.getImageUrlForBookmark(
                url, mCallbackController.makeCancelable(salientImageUrl -> {
                    callback.onResult(salientImageUrl);

                    mRequests.remove(callback);
                    Pair<GURL, Callback<GURL>> queuedRequest = mQueuedRequests.poll();
                    if (queuedRequest != null) {
                        getSalientImageUrl(queuedRequest.first, queuedRequest.second);
                    }
                }));
    }
}

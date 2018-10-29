// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.graphics.Bitmap;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.chromium.base.Callback;

/** Provides thumbnails that represent different files. */
public interface ThumbnailProvider {
    /**
     * Used to request the retrieval of a thumbnail.
     */
    public static interface ThumbnailRequest {
        /** Local storage path to the file. */
        @Nullable
        String getFilePath();

        /** The mime type of the file. */
        @Nullable
        String getMimeType();

        /** Content ID that uniquely identifies the file. */
        @Nullable
        String getContentId();

        /** Called when a requested thumbnail is ready. */
        void onThumbnailRetrieved(@NonNull String contentId, @Nullable Bitmap thumbnail);

        /** The requested size (maximum dimension (pixel) of the smaller side) of the thumbnail to
         * be retrieved. */
        int getIconSize();

        /**
         * Provides an independent way of getting thumbnail for offline items, thereby bypassing the
         * thumbnail disk storage and thumbnail generator. The implementer can override this method
         * and provide a special way to fetch the thumbnail by themselves, after which they must
         * return true and invoke the |callback|. Otherwise the fallback mechanism provided by
         * {@link ThumbnailProviderImpl} is executed. For the fallback scenario, this method must
         * return false and the |callback| shouldn't be invoked.
         * @param callback The callback to be invoked after getting the thumbnail.
         * @return True, if the request can directly provide the thumbnail, false otherwise.
         */
        default boolean
            getThumbnail(Callback<Bitmap> callback) {
                return false;
            }
    }

    /** Destroys the class. */
    void destroy();

    /**
     * Calls {@link ThumbnailRequest#onThumbnailRetrieved} immediately if the thumbnail is cached.
     * Otherwise, asynchronously fetches the thumbnail from the provider and calls
     * {@link ThumbnailRequest#onThumbnailRetrieved} when the result is ready.
     * @param request Parameters that describe the thumbnail being retrieved.
     */
    void getThumbnail(ThumbnailRequest request);

    /**
     * Removes the thumbnails (different sizes) with {@code contentId} from disk (if disk-cached).
     * @param contentId The content ID of the thumbnail to remove.
     */
    void removeThumbnailsFromDisk(String contentId);

    /** Removes a particular request from the pending queue. */
    void cancelRetrieval(ThumbnailRequest request);
}

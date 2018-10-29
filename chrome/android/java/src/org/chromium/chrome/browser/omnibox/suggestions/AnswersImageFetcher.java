// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.graphics.Bitmap;
import android.support.v4.util.LruCache;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.ConversionUtils;

/**
 * Provides access to images used by Answers in Suggest.
 */
public class AnswersImageFetcher {
    // Matches the value in BitmapFetcherService.
    private static final int INVALID_IMAGE_REQUEST_ID = 0;
    private static final int MAX_CACHE_SIZE = 500 * ConversionUtils.BYTES_PER_KILOBYTE;

    /**
     * Observer for updating an image when it is available.
     */
    public interface AnswersImageObserver {
        /**
         * Called when the image is updated.
         *
         * @param bitmap the image
         */
        @CalledByNative("AnswersImageObserver")
        void onAnswersImageChanged(Bitmap bitmap);
    }

    // Intentionally not using BitmapCache as that does not cache for low end devices (it ensures
    // the bitmaps are de-dups across instances, but discards them if there is not an active
    // reference to one).
    private final LruCache<String, Bitmap> mBitmapCache =
            new LruCache<String, Bitmap>(MAX_CACHE_SIZE) {
                @Override
                protected int sizeOf(String key, Bitmap value) {
                    return value.getByteCount();
                }
            };

    /**
     * Clears the cached answer images.
     */
    public void clearCache() {
        mBitmapCache.evictAll();
    }

    /**
     * Request image, observer is notified when image is loaded.
     * @param profile     Profile that the request is for.
     * @param imageUrl    URL for image data.
     * @param observer    Observer to be notified when image is updated. The C++ side will hold a
     *                    strong reference to this.
     * @return            A request_id.
     */
    public int requestAnswersImage(
            Profile profile, String imageUrl, AnswersImageObserver observer) {
        if (!profile.isOffTheRecord()) {
            Bitmap bitmap = mBitmapCache.get(imageUrl);
            if (bitmap != null) {
                observer.onAnswersImageChanged(bitmap);
                return INVALID_IMAGE_REQUEST_ID;
            }
        }
        AnswersImageObserver cacheObserver = observer;
        if (!profile.isOffTheRecord()) {
            cacheObserver = new AnswersImageObserver() {
                @Override
                public void onAnswersImageChanged(Bitmap bitmap) {
                    if (bitmap == null) return;
                    mBitmapCache.put(imageUrl, bitmap);
                    observer.onAnswersImageChanged(bitmap);
                }
            };
        }
        return nativeRequestAnswersImage(profile, imageUrl, cacheObserver);
    }

    /**
     * Cancel a pending image request.
     * @param profile    Profile the request was issued for.
     * @param requestId  The ID of the request to be cancelled.
     */
    public void cancelAnswersImageRequest(Profile profile, int requestId) {
        nativeCancelAnswersImageRequest(profile, requestId);
    }

    /**
     * Requests an image at |imageUrl| for the given |profile| with |observer| being notified.
     * @returns an AnswersImageRequest
     */
    private static native int nativeRequestAnswersImage(
            Profile profile, String imageUrl, AnswersImageObserver observer);

    /**
     * Cancels a pending request.
     */
    private static native void nativeCancelAnswersImageRequest(Profile profile, int requestId);
}

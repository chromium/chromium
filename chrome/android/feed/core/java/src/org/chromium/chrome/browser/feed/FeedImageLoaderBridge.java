// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Provides access to native implementations of image loader.
 */
@JNINamespace("feed")
public class FeedImageLoaderBridge {
    private long mNativeFeedImageLoaderBridge;

    /**
     * Creates a FeedImageLoaderBridge for accessing native image loader implementation for the
     * current user.
     */
    public FeedImageLoaderBridge() {}

    /**
     * Inits native side bridge.
     *
     * @param profile Profile of the user we are rendering the Feed for.
     */
    public void init(Profile profile) {
        mNativeFeedImageLoaderBridge = nativeInit(profile);
    }

    /** Cleans up native half of this bridge. */
    public void destroy() {
        assert mNativeFeedImageLoaderBridge != 0;
        nativeDestroy(mNativeFeedImageLoaderBridge);
        mNativeFeedImageLoaderBridge = 0;
    }

    /**
     * Fetches images for feed. A {@code null} Bitmap is returned if no image is available. The
     * callback is never called synchronously.
     */
    public void fetchImage(String url, int widthPx, int heightPx, Callback<Bitmap> callback) {
        assert mNativeFeedImageLoaderBridge != 0;
        nativeFetchImage(mNativeFeedImageLoaderBridge, url, widthPx, heightPx, callback);
    }

    // Native methods
    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeFeedImageLoaderBridge);
    private native void nativeFetchImage(long nativeFeedImageLoaderBridge, String url, int widthPx,
            int heightPx, Callback<Bitmap> callback);
}

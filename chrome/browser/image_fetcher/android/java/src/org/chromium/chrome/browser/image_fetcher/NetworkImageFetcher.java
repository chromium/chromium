// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_fetcher;

import android.graphics.Bitmap;

import org.chromium.base.Callback;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * Image Fetcher implementation that fetches from the network.
 */
public class NetworkImageFetcher extends ImageFetcher {
    // The native bridge.
    private ImageFetcherBridge mImageFetcherBridge;

    /**
     * Creates a NetworkImageFetcher.
     *
     * @param bridge Bridge used to interact with native.
     */
    NetworkImageFetcher(ImageFetcherBridge bridge) {
        mImageFetcherBridge = bridge;
    }

    @Override
    public void destroy() {
        // Do nothing, this lives for the lifetime of the application.
    }

    @Override
    public void fetchGif(String url, String clientName, Callback<BaseGifImage> callback) {
        mImageFetcherBridge.fetchGif(getConfig(), url, clientName, callback);
    }

    @Override
    public void fetchImage(
            String url, String clientName, int width, int height, Callback<Bitmap> callback) {
        long startTimeMillis = System.currentTimeMillis();
        mImageFetcherBridge.fetchImage(
                getConfig(), url, clientName, width, height, (Bitmap bitmapFromNative) -> {
                    callback.onResult(bitmapFromNative);
                    mImageFetcherBridge.reportTotalFetchTimeFromNative(clientName, startTimeMillis);
                });
    }

    @Override
    public void clear() {}

    @Override
    public @ImageFetcherConfig int getConfig() {
        return ImageFetcherConfig.NETWORK_ONLY;
    }
}

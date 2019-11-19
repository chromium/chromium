// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_fetcher;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * Provides access to native implementations of ImageFetcher for the given profile.
 */
@JNINamespace("image_fetcher")
public class ImageFetcherBridge {
    private static ImageFetcherBridge sImageFetcherBridge;

    private long mNativeImageFetcherBridge;

    // Get the instance of the ImageFetcherBridge. If used before browser initialization, this will
    // throw an IllegalStateException.
    public static ImageFetcherBridge getInstance() {
        if (sImageFetcherBridge == null) {
            Profile profile = Profile.getLastUsedProfile();
            sImageFetcherBridge = new ImageFetcherBridge(profile);
        }

        return sImageFetcherBridge;
    }

    /**
     * Creates a ImageFetcherBridge for accessing the native ImageFetcher implementation.
     */
    public ImageFetcherBridge(Profile profile) {
        mNativeImageFetcherBridge = ImageFetcherBridgeJni.get().init(profile);
    }

    /** Cleans up native half of bridge. */
    public void destroy() {
        assert mNativeImageFetcherBridge != 0;
        ImageFetcherBridgeJni.get().destroy(mNativeImageFetcherBridge, ImageFetcherBridge.this);
        mNativeImageFetcherBridge = 0;
    }

    /**
     * Get the full path of the given url on disk.
     *
     * @param url The url to hash.
     * @return The full path to the resource on disk.
     */
    public String getFilePath(String url) {
        assert mNativeImageFetcherBridge != 0;
        return ImageFetcherBridgeJni.get().getFilePath(
                mNativeImageFetcherBridge, ImageFetcherBridge.this, url);
    }

    /**
     * Fetch a gif from native or null if the gif can't be fetched or decoded.
     *
     * @param url The url to fetch.
     * @param clientName The UMA client name to report the metrics to.
     * @param callback The callback to call when the gif is ready. The callback will be invoked on
     *      the same thread it was called on.
     */
    public void fetchGif(@ImageFetcherConfig int config, String url, String clientName,
            Callback<BaseGifImage> callback) {
        assert mNativeImageFetcherBridge != 0;
        ImageFetcherBridgeJni.get().fetchImageData(mNativeImageFetcherBridge,
                ImageFetcherBridge.this, config, url, clientName, (byte[] data) -> {
                    if (data == null || data.length == 0) {
                        callback.onResult(null);
                    }

                    callback.onResult(new BaseGifImage(data));
                });
    }

    /**
     * Fetch the image from native, then resize it to the given dimensions.
     *
     * @param config The configuration of the image fetcher.
     * @param url The url to fetch.
     * @param width The desired width of the image.
     * @param height The desired height of the image.
     * @param clientName The UMA client name to report the metrics to.
     * @param callback The callback to call when the image is ready. The callback will be invoked on
     *      the same thread that it was called on.
     */
    public void fetchImage(@ImageFetcherConfig int config, String url, String clientName, int width,
            int height, Callback<Bitmap> callback) {
        assert mNativeImageFetcherBridge != 0;
        ImageFetcherBridgeJni.get().fetchImage(mNativeImageFetcherBridge, ImageFetcherBridge.this,
                config, url, clientName, (bitmap) -> {
                    callback.onResult(ImageFetcher.tryToResizeImage(bitmap, width, height));
                });
    }

    /**
     * Report a metrics event.
     *
     * @param clientName The UMA client name to report the metrics to.
     * @param eventId The event to report.
     */
    public void reportEvent(String clientName, @ImageFetcherEvent int eventId) {
        assert mNativeImageFetcherBridge != 0;
        ImageFetcherBridgeJni.get().reportEvent(
                mNativeImageFetcherBridge, ImageFetcherBridge.this, clientName, eventId);
    }

    /**
     * Report a timing event for a cache hit.
     *
     * @param clientName The UMA client name to report the metrics to.
     * @param startTimeMillis The start time (in milliseconds) of the request, used to measure the
     *      total duration.
     */
    public void reportCacheHitTime(String clientName, long startTimeMillis) {
        assert mNativeImageFetcherBridge != 0;
        ImageFetcherBridgeJni.get().reportCacheHitTime(
                mNativeImageFetcherBridge, ImageFetcherBridge.this, clientName, startTimeMillis);
    }

    /**
     * Report a timing event for a call to native
     *
     * @param clientName The UMA client name to report the metrics to.
     * @param startTimeMillis The start time (in milliseconds) of the request, used to measure the
     *      total duration.
     */
    public void reportTotalFetchTimeFromNative(String clientName, long startTimeMillis) {
        assert mNativeImageFetcherBridge != 0;
        ImageFetcherBridgeJni.get().reportTotalFetchTimeFromNative(
                mNativeImageFetcherBridge, ImageFetcherBridge.this, clientName, startTimeMillis);
    }

    /**
     * Setup the bridge for testing.
     * @param imageFetcherBridge The bridge used for testing.
     */
    public static void setupForTesting(ImageFetcherBridge imageFetcherBridge) {
        sImageFetcherBridge = imageFetcherBridge;
    }

    @NativeMethods
    interface Natives {
        // Native methods
        long init(Profile profile);

        void destroy(long nativeImageFetcherBridge, ImageFetcherBridge caller);
        String getFilePath(long nativeImageFetcherBridge, ImageFetcherBridge caller, String url);
        void fetchImageData(long nativeImageFetcherBridge, ImageFetcherBridge caller,
                @ImageFetcherConfig int config, String url, String clientName,
                Callback<byte[]> callback);
        void fetchImage(long nativeImageFetcherBridge, ImageFetcherBridge caller,
                @ImageFetcherConfig int config, String url, String clientName,
                Callback<Bitmap> callback);
        void reportEvent(long nativeImageFetcherBridge, ImageFetcherBridge caller,
                String clientName, int eventId);
        void reportCacheHitTime(long nativeImageFetcherBridge, ImageFetcherBridge caller,
                String clientName, long startTimeMillis);
        void reportTotalFetchTimeFromNative(long nativeImageFetcherBridge,
                ImageFetcherBridge caller, String clientName, long startTimeMillis);
    }
}

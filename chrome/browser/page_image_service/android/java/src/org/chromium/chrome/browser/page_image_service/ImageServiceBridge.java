// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_image_service;

import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.page_image_service.mojom.ClientId;
import org.chromium.page_image_service.mojom.ClientId.EnumType;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

/** Allows java access to the native ImageService. */
public class ImageServiceBridge {
    private final @EnumType int mClientId;
    private final String mImageFetcherClientName;
    // Cache the results for repeated queries to avoid extra calls through the JNI/network.
    private final Map<GURL, GURL> mSalientImageUrlCache = new HashMap<>();
    private final CallbackController mCallbackController = new CallbackController();
    private final ImageFetcher mImageFetcher;

    private long mNativeImageServiceBridge;

    /**
     * @param clientId The ImageService client the salient image url is being fetched for.
     * @param imageFetcherClientName The string name of the ImageFetcher client.
     * @param profile Used to acquire dependencies in native.
     * @param imageFetcher The fetcher to fetch the image.
     */
    public ImageServiceBridge(
            @EnumType int clientId,
            @NonNull String imageFetcherClientName,
            @NonNull Profile profile,
            @NonNull ImageFetcher imageFetcher) {
        mClientId = clientId;
        mImageFetcherClientName = imageFetcherClientName;
        mNativeImageServiceBridge = ImageServiceBridgeJni.get().init(profile);
        mImageFetcher = imageFetcher;
    }

    public void destroy() {
        ImageServiceBridgeJni.get().destroy(mNativeImageServiceBridge);
        mSalientImageUrlCache.clear();
        mCallbackController.destroy();
        mImageFetcher.destroy();
    }

    /** Cleanup any cached bitmap or URLs in memory. */
    public void clear() {
        mSalientImageUrlCache.clear();
        mImageFetcher.clear();
    }

    /**
     * Fetch the salient image {@link GURL} for the given page {@link GURL}.
     *
     * @param isAccountData Whether the underlying datatype being fetched for is account-bound.
     * @param pageUrl The url of the page the salient image is being fetched for.
     * @param imageSize The size of the salient image.
     * @param callback The callback to receive the salient image url.
     */
    public void fetchImageFor(
            boolean isAccountData,
            @NonNull GURL pageUrl,
            int imageSize,
            Callback<Bitmap> callback) {
        Callback<GURL> imageUrlCallback =
                mCallbackController.makeCancelable(
                        (imageUrl) -> {
                            if (imageUrl == null) {
                                callback.onResult(null);
                                return;
                            }

                            mImageFetcher.fetchImage(
                                    ImageFetcher.Params.create(
                                            imageUrl,
                                            mImageFetcherClientName,
                                            imageSize,
                                            imageSize),
                                    callback);
                        });
        fetchImageUrlFor(isAccountData, pageUrl, imageUrlCallback);
    }

    /**
     * Fetches the URL of the salient image and pass to the callback. The URL of the salient image
     * will be cached.
     */
    @VisibleForTesting
    void fetchImageUrlFor(
            boolean isAccountData, @NonNull GURL pageUrl, @NonNull Callback<GURL> callback) {
        if (mSalientImageUrlCache.containsKey(pageUrl)) {
            callback.onResult(mSalientImageUrlCache.get(pageUrl));
            return;
        }

        ImageServiceBridgeJni.get()
                .fetchImageUrlFor(
                        mNativeImageServiceBridge,
                        isAccountData,
                        mClientId,
                        pageUrl,
                        mCallbackController.makeCancelable(
                                (salientImageUrl) -> {
                                    mSalientImageUrlCache.put(pageUrl, salientImageUrl);
                                    callback.onResult(salientImageUrl);
                                }));
    }

    boolean isUrlCachedForTesting(GURL pageUrl, GURL imageUrl) {
        return mSalientImageUrlCache.containsKey(pageUrl)
                && mSalientImageUrlCache.containsValue(imageUrl);
    }

    @NativeMethods
    public interface Natives {
        long init(@JniType("Profile*") Profile profile);

        void destroy(long nativeImageServiceBridge);

        void fetchImageUrlFor(
                long nativeImageServiceBridge,
                boolean isAccountData,
                @ClientId.EnumType int clientId,
                @JniType("GURL") GURL pageUrl,
                Callback<GURL> callback);
    }
}

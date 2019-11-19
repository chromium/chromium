// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_fetcher;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * ImageFetcher implementation that uses a disk cache.
 */
public class CachedImageFetcher extends ImageFetcher {
    private static final String TAG = "CachedImageFetcher";

    // The native bridge.
    private ImageFetcherBridge mImageFetcherBridge;

    /**
     * Creates a CachedImageFetcher with the given bridge.
     *
     * @param bridge Bridge used to interact with native.
     */
    CachedImageFetcher(ImageFetcherBridge bridge) {
        mImageFetcherBridge = bridge;
    }

    @Override
    public void destroy() {
        // Do nothing, this lives for the lifetime of the application.
    }

    /**
     * Tries to load the gif from disk, if not it falls back to the bridge.
     */
    @Override
    public void fetchGif(String url, String clientName, Callback<BaseGifImage> callback) {
        long startTimeMillis = System.currentTimeMillis();
        PostTask.postTask(TaskTraits.USER_VISIBLE, () -> {
            // Try to read the gif from disk, then post back to the ui thread.
            String filePath = mImageFetcherBridge.getFilePath(url);
            BaseGifImage cachedGif = tryToLoadGifFromDisk(filePath);
            PostTask.postTask(UiThreadTaskTraits.USER_VISIBLE, () -> {
                continueFetchGifAfterDisk(url, clientName, callback, cachedGif, startTimeMillis);
            });
        });
    }

    @VisibleForTesting
    void continueFetchGifAfterDisk(String url, String clientName, Callback<BaseGifImage> callback,
            BaseGifImage cachedGif, long startTimeMillis) {
        if (cachedGif != null) {
            callback.onResult(cachedGif);
            reportEvent(clientName, ImageFetcherEvent.JAVA_DISK_CACHE_HIT);
            mImageFetcherBridge.reportCacheHitTime(clientName, startTimeMillis);
        } else {
            mImageFetcherBridge.fetchGif(
                    getConfig(), url, clientName, (BaseGifImage gifFromNative) -> {
                        callback.onResult(gifFromNative);
                        mImageFetcherBridge.reportTotalFetchTimeFromNative(
                                clientName, startTimeMillis);
                    });
        }
    }

    /**
     * Tries to load the gif from disk, if not it falls back to the bridge.
     */
    @Override
    public void fetchImage(
            String url, String clientName, int width, int height, Callback<Bitmap> callback) {
        long startTimeMillis = System.currentTimeMillis();
        PostTask.postTask(TaskTraits.USER_VISIBLE, () -> {
            // Try to read the bitmap from disk, then post back to the ui thread.
            String filePath = mImageFetcherBridge.getFilePath(url);
            Bitmap bitmap = tryToLoadImageFromDisk(filePath);
            PostTask.postTask(UiThreadTaskTraits.USER_VISIBLE, () -> {
                continueFetchImageAfterDisk(
                        url, clientName, width, height, callback, bitmap, startTimeMillis);
            });
        });
    }

    @VisibleForTesting
    void continueFetchImageAfterDisk(String url, String clientName, int width, int height,
            Callback<Bitmap> callback, Bitmap cachedBitmap, long startTimeMillis) {
        if (cachedBitmap != null) {
            callback.onResult(cachedBitmap);
            reportEvent(clientName, ImageFetcherEvent.JAVA_DISK_CACHE_HIT);
            mImageFetcherBridge.reportCacheHitTime(clientName, startTimeMillis);
        } else {
            mImageFetcherBridge.fetchImage(
                    getConfig(), url, clientName, width, height, (Bitmap bitmapFromNative) -> {
                        callback.onResult(bitmapFromNative);
                        mImageFetcherBridge.reportTotalFetchTimeFromNative(
                                clientName, startTimeMillis);
                    });
        }
    }

    @Override
    public void clear() {}

    @Override
    public @ImageFetcherConfig int getConfig() {
        return ImageFetcherConfig.DISK_CACHE_ONLY;
    }

    /** Wrapper function to decode a file for disk, useful for testing. */
    @VisibleForTesting
    Bitmap tryToLoadImageFromDisk(String filePath) {
        if (new File(filePath).exists()) {
            return BitmapFactory.decodeFile(filePath, null);
        } else {
            return null;
        }
    }

    @VisibleForTesting
    BaseGifImage tryToLoadGifFromDisk(String filePath) {
        try {
            File file = new File(filePath);
            byte[] fileBytes = new byte[(int) file.length()];
            FileInputStream fileInputStream = new FileInputStream(filePath);

            int bytesRead = fileInputStream.read(fileBytes);
            if (bytesRead != fileBytes.length) return null;

            return new BaseGifImage(fileBytes);
        } catch (IOException e) {
            Log.w(TAG, "Failed to read: %s", filePath, e);
            return null;
        }
    }
}

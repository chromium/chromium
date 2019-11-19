// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_fetcher;

import android.graphics.Bitmap;
import android.media.ThumbnailUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * Blueprint and some implementation for image fetching. Use ImageFetcherFactory for any
 * ImageFetcher instantiation.
 */
public abstract class ImageFetcher {
    // All UMA client names collected here to prevent duplicates.
    public static final String ANSWER_SUGGESTIONS_UMA_CLIENT_NAME = "AnswerSuggestions";
    public static final String ASSISTANT_DETAILS_UMA_CLIENT_NAME = "AssistantDetails";
    public static final String ASSISTANT_INFO_BOX_UMA_CLIENT_NAME = "AssistantInfoBox";
    public static final String ENTITY_SUGGESTIONS_UMA_CLIENT_NAME = "EntitySuggestions";
    public static final String FEED_UMA_CLIENT_NAME = "Feed";
    public static final String NTP_ANIMATED_LOGO_UMA_CLIENT_NAME = "NewTabPageAnimatedLogo";

    /**
     * Try to resize the given image if the conditions are met.
     *
     * @param bitmap The input bitmap, will be recycled if scaled.
     * @param width The desired width of the output.
     * @param height The desired height of the output.
     *
     * @return The resized image, or the original image if the  conditions aren't met.
     */
    @VisibleForTesting
    public static Bitmap tryToResizeImage(@Nullable Bitmap bitmap, int width, int height) {
        if (bitmap != null && width > 0 && height > 0 && bitmap.getWidth() != width
                && bitmap.getHeight() != height) {
            /* The resizing rules are the as follows:
               (1) The image will be scaled up (if smaller) in a way that maximizes the area of the
               source bitmap that's in the destination bitmap.
               (2) A crop is made in the middle of the bitmap for the given size (width, height).
               The x/y are placed appropriately (conceptually just think of it as a properly sized
               chunk taken from the middle). */
            return ThumbnailUtils.extractThumbnail(
                    bitmap, width, height, ThumbnailUtils.OPTIONS_RECYCLE_INPUT);
        } else {
            return bitmap;
        }
    }

    /**
     * Report an event metric.
     *
     * @param clientName Name of the cached image fetcher client to report UMA metrics for.
     * @param eventId The event to be reported
     */
    public void reportEvent(String clientName, @ImageFetcherEvent int eventId) {
        ImageFetcherBridge.getInstance().reportEvent(clientName, eventId);
    }

    /**
     * Fetch the gif for the given url.
     *
     * @param url The url to fetch the image from.
     * @param clientName The UMA client name to report the metrics to. If using CachedImageFetcher
     *         to fetch images and gifs, use separate clientNames for them.
     * @param callback The function which will be called when the image is ready; will be called
     *         with null result if fetching fails.
     */
    public abstract void fetchGif(String url, String clientName, Callback<BaseGifImage> callback);

    /**
     * Fetches the image at url with the desired size. Image is null if not found or fails decoding.
     *
     * @param url The url to fetch the image from.
     * @param clientName Name of the cached image fetcher client to report UMA metrics for.
     * @param width The new bitmap's desired width (in pixels). If the given value is <= 0, the
     *         image won't be scaled.
     * @param height The new bitmap's desired height (in pixels). If the given value is <= 0, the
     *         image won't be scaled.
     * @param callback The function which will be called when the image is ready; will be called
     *         with null result if fetching fails;
     */
    public abstract void fetchImage(
            String url, String clientName, int width, int height, Callback<Bitmap> callback);

    /**
     * Alias of fetchImage that ignores scaling.
     *
     * @param url The url to fetch the image from.
     * @param clientName Name of the cached image fetcher client to report UMA metrics for.
     * @param callback The function which will be called when the image is ready; will be called
     *         with null result if fetching fails;
     */
    public void fetchImage(String url, String clientName, Callback<Bitmap> callback) {
        fetchImage(url, clientName, 0, 0, callback);
    }

    /**
     * Clear the cache of any bitmaps that may be in-memory.
     */
    public abstract void clear();

    /**
     * Returns the type of Image Fetcher this is based on class arrangements. See
     * image_fetcher_service.h for a detailed description of the available configurations.
     *
     * @return the type of the image fetcher this class maps to in native.
     */
    public abstract @ImageFetcherConfig int getConfig();

    /**
     * Destroy method, called to clear resources to prevent leakage.
     */
    public abstract void destroy();
}

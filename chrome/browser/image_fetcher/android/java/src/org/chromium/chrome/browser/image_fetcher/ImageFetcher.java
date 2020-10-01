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
    // All UMA client names collected here to prevent duplicates. While adding a new client, please
    // update the histogram suffix ImageFetcherClients in histograms.xml as well.
    public static final String ANSWER_SUGGESTIONS_UMA_CLIENT_NAME = "AnswerSuggestions";
    public static final String ASSISTANT_DETAILS_UMA_CLIENT_NAME = "AssistantDetails";
    public static final String ASSISTANT_INFO_BOX_UMA_CLIENT_NAME = "AssistantInfoBox";
    public static final String CRYPTIDS_UMA_CLIENT_NAME = "Cryptids";
    public static final String ENTITY_SUGGESTIONS_UMA_CLIENT_NAME = "EntitySuggestions";
    public static final String FEED_UMA_CLIENT_NAME = "Feed";
    public static final String NTP_ANIMATED_LOGO_UMA_CLIENT_NAME = "NewTabPageAnimatedLogo";
    public static final String QUERY_TILE_UMA_CLIENT_NAME = "QueryTiles";
    public static final String VIDEO_TUTORIALS_IPH_UMA_CLIENT_NAME = "VideoTutorialsIPH";
    public static final String VIDEO_TUTORIALS_LIST_UMA_CLIENT_NAME = "VideoTutorialsList";

    /**
     * Encapsulates image fetching customization options. Supports a subset of the native
     * ImageFetcherParams. The image resizing is done in Java.
     */
    public static class Params {
        static final int DEFAULT_IMAGE_SIZE = 0;
        static final int INVALID_EXPIRATION_INTERVAL = 0;

        /**
         * Creates image fetcher parameters. The image will not be resized.
         * @See {@link #Params(String, String, int, int, int)}.
         */
        public static Params create(final String url, String clientName) {
            return new Params(url, clientName, DEFAULT_IMAGE_SIZE, DEFAULT_IMAGE_SIZE,
                    INVALID_EXPIRATION_INTERVAL);
        }

        /**
         * Creates image fetcher parameters with image size specified.
         * @See {@link #Params(String, String, int, int, int)}.
         */
        public static Params create(final String url, String clientName, int width, int height) {
            return new Params(url, clientName, width, height, INVALID_EXPIRATION_INTERVAL);
        }

        /**
         * Only used in rare cases. Creates image fetcher parameters that keeps the cache file for a
         * certain period of time.
         * @See {@link #Params(String, String, int, int, int)}.
         */
        public static Params createWithExpirationInterval(final String url, String clientName,
                int width, int height, int expirationIntervalMinutes) {
            assert expirationIntervalMinutes > INVALID_EXPIRATION_INTERVAL
                : "Must specify a positive expiration interval, or use other constructors.";
            return new Params(url, clientName, width, height, expirationIntervalMinutes);
        }

        private Params(String url, String clientName, int width, int height,
                int expirationIntervalMinutes) {
            assert expirationIntervalMinutes >= INVALID_EXPIRATION_INTERVAL
                : "Expiration interval should be non negative.";

            this.url = url;
            this.clientName = clientName;
            this.width = width;
            this.height = height;
            this.expirationIntervalMinutes = expirationIntervalMinutes;
        }

        @Override
        public boolean equals(Object other) {
            if (other == this) return true;
            if (!(other instanceof ImageFetcher.Params)) return false;

            ImageFetcher.Params otherParams = (ImageFetcher.Params) other;
            return url.equals(otherParams.url) && clientName.equals(otherParams.clientName)
                    && width == otherParams.width && height == otherParams.height
                    && expirationIntervalMinutes == otherParams.expirationIntervalMinutes;
        }

        @Override
        public int hashCode() {
            int result = (url != null) ? url.hashCode() : 0;
            result = 31 * result + ((clientName != null) ? clientName.hashCode() : 0);
            result = 31 * result + width;
            result = 31 * result + height;
            result = 31 * result + expirationIntervalMinutes;
            return result;
        }

        /**
         * The url to fetch the image from.
         */
        public final String url;

        /**
         * Name of the cached image fetcher client to report UMA metrics for.
         */
        public final String clientName;

        /**
         * The new bitmap's desired width (in pixels). If the given value is <= 0, the image won't
         * be scaled.
         */
        public final int width;

        /**
         * The new bitmap's desired height (in pixels). If the given value is <= 0, the image won't
         * be scaled.
         */
        public final int height;

        /**
         * Only specifies in rare cases to keep the cache file on disk for certain period of time.
         * Measured in minutes. Any value <= 0 will be ignored.
         */
        public final int expirationIntervalMinutes;
    }

    /** Base class that can be used for testing. */
    public abstract static class ImageFetcherForTesting extends ImageFetcher {
        public ImageFetcherForTesting() {}
    }

    // Singleton ImageFetcherBridge.
    private ImageFetcherBridge mImageFetcherBridge;

    /** Copy-constructor to support composite instances of ImageFetcher. */
    public ImageFetcher(ImageFetcher imageFetcher) {
        mImageFetcherBridge = imageFetcher.getImageFetcherBridge();
    }

    /** Base constructor that takes an ImageFetcherBridge. */
    public ImageFetcher(ImageFetcherBridge imageFetcherBridge) {
        mImageFetcherBridge = imageFetcherBridge;
    }

    /** Test constructor */
    private ImageFetcher() {}

    protected ImageFetcherBridge getImageFetcherBridge() {
        return mImageFetcherBridge;
    }

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
    public static Bitmap resizeImage(@Nullable Bitmap bitmap, int width, int height) {
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
        mImageFetcherBridge.reportEvent(clientName, eventId);
    }

    /**
     * Fetch the gif for the given url.
     *
     * @param params The parameters to specify image fetching details. If using CachedImageFetcher
     *         to fetch images and gifs, use separate {@link Params#clientName} for them.
     * @param callback The function which will be called when the image is ready; will be called
     *         with null result if fetching fails.
     */
    public abstract void fetchGif(
            final ImageFetcher.Params params, Callback<BaseGifImage> callback);

    /**
     * Fetches the image based on customized parameters specified.
     *
     * @param params The parameters to specify image fetching details.
     * @param callback The function which will be called when the image is ready; will be called
     *         with null result if fetching fails;
     */
    public abstract void fetchImage(final Params params, Callback<Bitmap> callback);

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

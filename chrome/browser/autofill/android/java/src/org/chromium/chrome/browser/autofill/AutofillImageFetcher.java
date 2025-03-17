// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.CardIconSpecs;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.embedder_support.simple_factory_key.SimpleFactoryKeyHandle;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;
import java.util.Optional;

/** Fetches, and caches credit card art images. */
public class AutofillImageFetcher {
    private final Map<String, Bitmap> mImagesCache = new HashMap<>();
    private ImageFetcher mImageFetcher;

    @CalledByNative
    private static AutofillImageFetcher create(SimpleFactoryKeyHandle key) {
        return new AutofillImageFetcher(
                ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.DISK_CACHE_ONLY, key));
    }

    AutofillImageFetcher(ImageFetcher imageFetcher) {
        mImageFetcher = imageFetcher;
    }

    /**
     * Fetches images for the passed in URLs and stores them in cache.
     *
     * @param urls The URLs to fetch the images.
     * @param imageSizes The list of image sizes that should be fetched for each of the above URLs.
     */
    @CalledByNative
    void prefetchImages(
            @JniType("base::span<const GURL>") GURL[] urls, @ImageSize int[] imageSizes) {
        Context context = ContextUtils.getApplicationContext();

        for (GURL url : urls) {
            // Capital One card art image is stored in Chrome binary.
            if (url == null || url.getSpec().equals(AutofillUiUtils.CAPITAL_ONE_ICON_URL)) {
                continue;
            }

            for (@ImageSize int size : imageSizes) {
                CardIconSpecs cardIconSpecs = CardIconSpecs.create(context, size);
                fetchImage(url, cardIconSpecs);
            }
        }
    }

    /**
     * Fetches images for the passed in Pix account image URLs, treats and stores them in cache.
     *
     * @param urls The URLs to fetch the images.
     */
    @CalledByNative
    void prefetchPixAccountImages(@JniType("base::span<const GURL>") GURL[] urls) {
        for (GURL url : urls) {
            if (url == null || !url.isValid()) {
                continue;
            }

            GURL urlWithParams = AutofillImageFetcherUtils.getPixAccountImageUrlWithParams(url);
            fetchImage(
                    urlWithParams, bitmap -> treatAndCachePixAccountImage(bitmap, urlWithParams));
        }
    }

    /**
     * Returns the Pix bank account icon. Prefers Pix account specific image if it exists in cache,
     * else a generic bank icon is returned.
     *
     * @param context {@link Context} to get the resources.
     * @param url The URL for the image.
     * @return {@link Drawable} to be displayed for the Pix account.
     */
    public Drawable getPixAccountIcon(Context context, @Nullable GURL url) {
        GURL cachedUrl = new GURL("");
        if (url != null && url.isValid()) {
            cachedUrl = AutofillImageFetcherUtils.getPixAccountImageUrlWithParams(url);
        }

        return getIcon(context, cachedUrl, R.drawable.ic_account_balance);
    }

    /**
     * Returns the required image if it exists in the image cache, empty object otherwise.
     *
     * @param url The URL of the image.
     * @param cardIconSpecs The sizing specifications for the image.
     * @return Bitmap image for the passed in URL if it exists in cache, an empty object otherwise.
     */
    Optional<Bitmap> getImageIfAvailable(GURL url, CardIconSpecs cardIconSpecs) {
        GURL urlToCache =
                AutofillUiUtils.getCreditCardIconUrlWithParams(
                        url, cardIconSpecs.getWidth(), cardIconSpecs.getHeight());
        // If the card art image exists in the cache, return it.
        if (mImagesCache.containsKey(urlToCache.getSpec())) {
            return Optional.of(mImagesCache.get(urlToCache.getSpec()));
        }

        return Optional.empty();
    }

    /**
     * Fetches image for the given URL.
     *
     * @param url The URL to fetch the image.
     */
    private void fetchImage(GURL url, CardIconSpecs cardIconSpecs) {
        if (!url.isValid()) {
            return;
        }

        GURL urlToFetch =
                AutofillUiUtils.getCreditCardIconUrlWithParams(
                        url, cardIconSpecs.getWidth(), cardIconSpecs.getHeight());
        if (mImagesCache.containsKey(urlToFetch.getSpec())) {
            return;
        }

        ImageFetcher.Params params =
                ImageFetcher.Params.create(
                        urlToFetch.getSpec(), ImageFetcher.AUTOFILL_CARD_ART_UMA_CLIENT_NAME);
        mImageFetcher.fetchImage(
                params, bitmap -> treatAndCacheImage(bitmap, urlToFetch, cardIconSpecs));
    }

    private void treatAndCacheImage(Bitmap bitmap, GURL urlToCache, CardIconSpecs cardIconSpecs) {
        RecordHistogram.recordBooleanHistogram("Autofill.ImageFetcher.Result", bitmap != null);

        // If the image fetching was unsuccessful, silently return.
        if (bitmap == null) {
            return;
        }
        // When adding new sizes for card icons, check if the corner radius needs to be added as
        // a suffix for caching (crbug.com/1431283).
        mImagesCache.put(
                urlToCache.getSpec(),
                AutofillUiUtils.resizeAndAddRoundedCornersAndGreyBorder(
                        bitmap, cardIconSpecs, true));
    }

    /**
     * Fetches image for the given URL and passes it to the callback.
     *
     * @param customUrl The final URL including any params to fetch the image.
     * @param onImageFetched The callback to be called with the fetched image.
     */
    private void fetchImage(GURL customUrl, Callback<Bitmap> onImageFetched) {
        if (mImagesCache.containsKey(customUrl.getSpec())) {
            return;
        }

        ImageFetcher.Params params =
                ImageFetcher.Params.create(
                        customUrl.getSpec(), ImageFetcher.AUTOFILL_CARD_ART_UMA_CLIENT_NAME);
        mImageFetcher.fetchImage(params, onImageFetched);
    }

    /**
     * Adds enhancements to Pix account image, and caches it.
     *
     * @param bitmap The Bitmap fetched from server.
     * @param urlToCache The key against which the treated Bitmap is cached.
     */
    private void treatAndCachePixAccountImage(Bitmap bitmap, GURL urlToCache) {
        RecordHistogram.recordBooleanHistogram("Autofill.ImageFetcher.Result", bitmap != null);

        if (bitmap == null) {
            return;
        }

        mImagesCache.put(
                urlToCache.getSpec(), AutofillImageFetcherUtils.treatPixAccountImage(bitmap));
    }

    /**
     * Returns a custom image cached with `cachedUrl` as key if it exists. Else returns resource
     * corresponding to `defaultIconId`.
     *
     * @param context {@link Context} to get the resources.
     * @param cachedUrl The key for the cached custom image.
     * @param defaultIconId Resource id of the default fallback icon.
     * @return {@link Drawable} which is either the custom icon corresponding to `cachedUrl` from
     *     cache or the fallback icon corresponding to `defaultIconId` from resources. Prefers
     *     former over latter.
     */
    private Drawable getIcon(Context context, GURL cachedUrl, int defaultIconId) {
        if (cachedUrl.isValid() && mImagesCache.containsKey(cachedUrl.getSpec())) {
            return new BitmapDrawable(
                    context.getResources(), mImagesCache.get(cachedUrl.getSpec()));
        }

        return AppCompatResources.getDrawable(context, defaultIconId);
    }

    /**
     * Add an image to the in-memory cache of images.
     *
     * @param url The URL that should be used as the key for the cache.
     * @param bitmap The actual image bitmap that should be cached.
     * @param cardIconSpecs The {@link CardIconSpecs} for the bitmap. This is used for generating
     *     the URL params.
     */
    public void addImageToCacheForTesting(GURL url, Bitmap bitmap, CardIconSpecs cardIconSpecs) {
        GURL urlToCache =
                AutofillUiUtils.getCreditCardIconUrlWithParams(
                        url, cardIconSpecs.getWidth(), cardIconSpecs.getHeight());
        mImagesCache.put(urlToCache.getSpec(), bitmap);
    }

    /**
     * Add an image to the in-memory cache of images.
     *
     * @param url The URL that should be used as the key for the cache.
     * @param bitmap The image to be cached.
     */
    public void addImageToCacheForTesting(GURL url, Bitmap bitmap) {
        mImagesCache.put(url.getSpec(), bitmap);
    }

    Map<String, Bitmap> getCachedImagesForTesting() {
        return mImagesCache;
    }

    /** Clears the in-memory cache of images. */
    public void clearCachedImagesForTesting() {
        mImagesCache.clear();
    }
}

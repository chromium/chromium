// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.CardIconSize;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.CardIconSpecs;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.embedder_support.simple_factory_key.SimpleFactoryKeyHandle;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;
import java.util.Optional;

/**
 * Fetches, and caches credit card art images.
 */
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
     * @param urls The URLs to fetch the images.
     */
    @CalledByNative
    void prefetchImages(GURL[] urls) {
        Context context = ContextUtils.getApplicationContext();

        for (GURL url : urls) {
            // Credit card art images are shown in 2 different sizes depending on the surface.
            // Prefetch and cache images in both sizes.
            for (@CardIconSize int size = 0; size < CardIconSize.NUM_SIZES; size++) {
                CardIconSpecs cardIconSpecs = CardIconSpecs.create(context, size);
                fetchImage(url, cardIconSpecs);
            }
        }
    }

    /**
     * Returns the required image if it exists in the image cache. If not, makes a call to fetch and
     * cache the image for next time.
     *
     * @param url The URL of the image.
     * @param cardIconSpecs The sizing specifications for the image.
     * @return Bitmap image for the passed in URL if it exists in cache, an empty object otherwise.
     */
    Optional<Bitmap> getImageIfAvailable(GURL url, CardIconSpecs cardIconSpecs) {
        GURL urlToCache = AutofillUiUtils.getCreditCardIconUrlWithParams(
                url, cardIconSpecs.getWidth(), cardIconSpecs.getHeight());
        // If the card art image exists in the cache, return it.
        if (mImagesCache.containsKey(urlToCache.getSpec())) {
            return Optional.of(mImagesCache.get(urlToCache.getSpec()));
        }

        // If not, fetch the image from the server, and cache for next time. Return empty object.
        fetchImage(url, cardIconSpecs);
        return Optional.empty();
    }

    /**
     * Fetches image for the given URL.
     * @param url The URL to fetch the image.
     */
    private void fetchImage(GURL url, CardIconSpecs cardIconSpecs) {
        if (!url.isValid()) {
            return;
        }

        // The Capital One icon for virtual cards is available in a single size via a static
        // URL. Cache this image at different sizes so it can be used by different surfaces.
        GURL urlToCache = AutofillUiUtils.getCreditCardIconUrlWithParams(
                url, cardIconSpecs.getWidth(), cardIconSpecs.getHeight());
        GURL urlToFetch =
                url.getSpec().equals(AutofillUiUtils.CAPITAL_ONE_ICON_URL) ? url : urlToCache;

        // If the image already exists in the cache, return.
        if (mImagesCache.containsKey(urlToCache.getSpec())) {
            return;
        }

        ImageFetcher.Params params = ImageFetcher.Params.create(
                urlToFetch.getSpec(), ImageFetcher.AUTOFILL_CARD_ART_UMA_CLIENT_NAME);
        mImageFetcher.fetchImage(
                params, bitmap -> treatAndCacheImage(bitmap, urlToCache, cardIconSpecs));
    }

    private void treatAndCacheImage(Bitmap bitmap, GURL urlToCache, CardIconSpecs cardIconSpecs) {
        RecordHistogram.recordBooleanHistogram("Autofill.ImageFetcher.Result", bitmap != null);

        // If the image fetching was unsuccessful, silently return.
        if (bitmap == null) {
            return;
        }

        // When adding new sizes for card icons, check if the corner radius needs to be added as
        // a suffix for caching (crbug.com/1431283).
        mImagesCache.put(urlToCache.getSpec(),
                AutofillUiUtils.resizeAndAddRoundedCornersAndGreyBorder(bitmap, cardIconSpecs,
                        ChromeFeatureList.isEnabled(
                                ChromeFeatureList
                                        .AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES)));
    }

    Map<String, Bitmap> getCachedImagesForTesting() {
        return mImagesCache;
    }

    void clearCachedImagesForTesting() {
        mImagesCache.clear();
    }
}

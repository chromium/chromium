// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.graphics.Bitmap;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.embedder_support.simple_factory_key.SimpleFactoryKeyHandle;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

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
        for (GURL url : urls) {
            fetchImage(url);
        }
    }

    /**
     * Fetches image for the given URL.
     * @param url The URL to fetch the image.
     */
    private void fetchImage(GURL url) {
        if (!url.isValid()) {
            return;
        }
        // If the image already exists in the cache, return.
        if (mImagesCache.containsKey(url.getSpec())) {
            return;
        }

        ImageFetcher.Params params = ImageFetcher.Params.create(
                url.getSpec(), ImageFetcher.AUTOFILL_CARD_ART_UMA_CLIENT_NAME);
        mImageFetcher.fetchImage(params, bitmap -> {
            // If the image fetching was unsuccessful, silently return.
            if (bitmap == null) {
                return;
            }

            // Save images in the cache along with the URL.
            mImagesCache.put(url.getSpec(), bitmap);
        });
    }

    Map<String, Bitmap> getCachedImagesForTesting() {
        return mImagesCache;
    }

    void clearCachedImagesForTesting() {
        mImagesCache.clear();
    }
}

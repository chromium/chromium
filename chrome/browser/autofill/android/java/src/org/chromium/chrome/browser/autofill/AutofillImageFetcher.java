// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.IconSpecs;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.autofill.ImageType;
import org.chromium.components.embedder_support.simple_factory_key.SimpleFactoryKeyHandle;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;
import java.util.Optional;
import java.util.function.Function;

/** Fetches, and caches credit card art images. */
@NullMarked
public class AutofillImageFetcher {
    private static final long REFETCH_DELAY_MS = 120000; // 2 mins.
    private static final int MAX_FETCH_ATTEMPTS = 2;
    // Logs the overall success rate of fetching credit card art images. For a given credit card art
    // URL, logs "true" if image was fetched, "false" if the image was not fetched after {@link
    // #MAX_FETCH_ATTEMPTS} attempts.
    private static final String CREDIT_CARD_ART_OVERALL_SUCCESS_HISTOGRAM =
            "Autofill.ImageFetcher.CreditCardArt.OverallResultOnBrowserStart";
    // Logs the overall success rate of fetching Pix account images. For a given Pix account image
    // URL, logs "true" if image was fetched, "false" if the image was not fetched after {@link
    // #MAX_FETCH_ATTEMPTS} attempts.
    private static final String PIX_ACCOUNT_IMAGE_OVERALL_SUCCESS_HISTOGRAM =
            "Autofill.ImageFetcher.PixAccountImage.OverallResultOnBrowserStart";
    // Logs the overall success rate of fetching valuable images. For a given valuable image URL,
    // logs "true" if image was fetched, "false" if the image was not fetched after {@link
    // #MAX_FETCH_ATTEMPTS} attempts.
    private static final String VALUABLE_IMAGE_OVERALL_SUCCESS_HISTOGRAM =
            "Autofill.ImageFetcher.ValuableImage.OverallResultOnBrowserStart";
    // Valuable images should be cached in small and large size on Android.
    public static final int[] VALUABLE_IMAGE_SIZES = new int[] {ImageSize.SMALL, ImageSize.LARGE};

    private final Map<String, Integer> mFetchAttemptCounter = new HashMap<>();
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
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void prefetchCardArtImages(
            @JniType("base::span<const GURL>") GURL[] urls, @ImageSize int[] imageSizes) {
        Context context = ContextUtils.getApplicationContext();

        for (GURL url : urls) {
            // Capital One card art image is stored in Chrome binary.
            if (url == null
                    || !url.isValid()
                    || url.getSpec().equals(AutofillUiUtils.CAPITAL_ONE_ICON_URL)) {
                continue;
            }

            for (@ImageSize int size : imageSizes) {
                IconSpecs iconSpecs =
                        IconSpecs.create(context, ImageType.CREDIT_CARD_ART_IMAGE, size);
                String resolvedUrl = iconSpecs.getResolvedIconUrl(url).getSpec();
                Function<Bitmap, Bitmap> treatImageFunction =
                        bitmap ->
                                AutofillUiUtils.resizeAndAddRoundedCornersAndGreyBorder(
                                        bitmap, iconSpecs, true);
                Callback<@Nullable Bitmap> onImageFetched =
                        bitmap ->
                                treatAndCacheImage(
                                        bitmap,
                                        resolvedUrl,
                                        treatImageFunction,
                                        CREDIT_CARD_ART_OVERALL_SUCCESS_HISTOGRAM);
                fetchImage(resolvedUrl, onImageFetched);
            }
        }
    }

    /**
     * Fetches images for the passed in Pix account image URLs, treats and stores them in cache.
     *
     * @param urls The URLs to fetch the images.
     */
    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void prefetchPixAccountImages(@JniType("base::span<const GURL>") GURL[] urls) {
        for (GURL url : urls) {
            if (url == null || !url.isValid()) {
                continue;
            }

            String resolvedUrl =
                    AutofillImageFetcherUtils.getPixAccountImageUrlWithParams(url).getSpec();
            Function<Bitmap, Bitmap> treatImageFunction =
                    bitmap -> AutofillImageFetcherUtils.treatPixAccountImage(bitmap);
            Callback<@Nullable Bitmap> onImageFetched =
                    bitmap ->
                            treatAndCacheImage(
                                    bitmap,
                                    resolvedUrl,
                                    treatImageFunction,
                                    PIX_ACCOUNT_IMAGE_OVERALL_SUCCESS_HISTOGRAM);
            fetchImage(resolvedUrl, onImageFetched);
        }
    }

    /**
     * Fetches images for the passed in valuable image URLs, treats and stores them in cache.
     *
     * @param urls The URLs to fetch the images.
     */
    @CalledByNative
    void prefetchValuableImages(@JniType("base::span<const GURL>") GURL[] urls) {
        Context context = ContextUtils.getApplicationContext();

        for (@ImageSize int size : VALUABLE_IMAGE_SIZES) {
            IconSpecs iconSpecs = IconSpecs.create(context, ImageType.VALUABLE_IMAGE, size);
            for (GURL url : urls) {
                if (url == null || !url.isValid()) {
                    continue;
                }
                String resolvedUrl = iconSpecs.getResolvedIconUrl(url).getSpec();
                // TODO: crbug.com/404437211 - Make sure the valuable images are post-processed
                // properly.
                Callback<@Nullable Bitmap> onImageFetched =
                        bitmap ->
                                treatAndCacheImage(
                                        bitmap,
                                        resolvedUrl,
                                        imageBitmap -> imageBitmap,
                                        VALUABLE_IMAGE_OVERALL_SUCCESS_HISTOGRAM);
                fetchImage(resolvedUrl, onImageFetched);
            }
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
        GURL resolvedUrl = new GURL("");
        if (url != null && url.isValid()) {
            resolvedUrl = AutofillImageFetcherUtils.getPixAccountImageUrlWithParams(url);
        }

        return getIcon(context, resolvedUrl, R.drawable.ic_account_balance);
    }

    /**
     * Returns the required image if it exists in the image cache, empty object otherwise.
     *
     * @param url The URL of the image.
     * @param iconSpecs The sizing specifications for the image.
     * @return Bitmap image for the passed in URL if it exists in cache, an empty object otherwise.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public Optional<Bitmap> getImageIfAvailable(GURL url, IconSpecs iconSpecs) {
        GURL resolvedUrl = iconSpecs.getResolvedIconUrl(url);
        // If the card art image exists in the cache, return it.
        if (mImagesCache.containsKey(resolvedUrl.getSpec())) {
            return Optional.of(mImagesCache.get(resolvedUrl.getSpec()));
        }

        return Optional.empty();
    }

    /**
     * Fetches image for the given URL and passes it to the callback.
     *
     * @param resolvedUrl The final URL including any params to fetch the image.
     * @param onImageFetched The callback to be called with the fetched image.
     */
    private void fetchImage(String resolvedUrl, Callback<@Nullable Bitmap> onImageFetched) {
        if (mImagesCache.containsKey(resolvedUrl)) {
            return;
        }

        // Update the attempt count for fetching the image.
        int fetchAttemptCount = mFetchAttemptCounter.getOrDefault(resolvedUrl, 0);
        mFetchAttemptCounter.put(resolvedUrl, fetchAttemptCount + 1);

        ImageFetcher.Params params =
                ImageFetcher.Params.create(
                        resolvedUrl, ImageFetcher.AUTOFILL_IMAGE_FETCHER_UMA_CLIENT_NAME);
        mImageFetcher.fetchImage(params, onImageFetched);
    }

    /**
     * Adds enhancements to {@code bitmap} by applying {@code treatImageFunction}, and caches it. If
     * image fetching fails, retries fetching {@code MAX_FETCH_ATTEMPTS - 1} times with a delay of
     * {@code REFETCH_DELAY_MS} between each attempt.
     *
     * @param bitmap The Bitmap fetched from server.
     * @param resolvedUrl The key against which the treated Bitmap is cached.
     * @param treatImageFunction Imagetreatment function.
     * @param overallSuccessHistogramName Histogram name to measure the success rate of a specific
     *     image type. Logs whether or not the image was fetched after a maximum of {@code
     *     MAX_FETCH_ATTEMPTS} attempts.
     */
    private void treatAndCacheImage(
            @Nullable Bitmap bitmap,
            String resolvedUrl,
            Function<Bitmap, Bitmap> treatImageFunction,
            String overallSuccessHistogramName) {
        RecordHistogram.recordBooleanHistogram("Autofill.ImageFetcher.Result", bitmap != null);

        if (bitmap != null) {
            RecordHistogram.recordBooleanHistogram(overallSuccessHistogramName, /* sample= */ true);

            mImagesCache.put(resolvedUrl, treatImageFunction.apply(bitmap));
            return;
        }

        // Image fetching failed, and max retry attempts reached.
        if (mFetchAttemptCounter.getOrDefault(resolvedUrl, 0) >= MAX_FETCH_ATTEMPTS) {
            RecordHistogram.recordBooleanHistogram(
                    overallSuccessHistogramName, /* sample= */ false);
            return;
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_RETRY_IMAGE_FETCH_ON_FAILURE)) {
            // Image fetching failed, and max retry attempts not reached -> retry fetch after a
            // delay.
            Callback<@Nullable Bitmap> onImageFetched =
                    fetchedBitmap ->
                            treatAndCacheImage(
                                    fetchedBitmap,
                                    resolvedUrl,
                                    treatImageFunction,
                                    overallSuccessHistogramName);
            Handler handler = new Handler();
            handler.postDelayed(() -> fetchImage(resolvedUrl, onImageFetched), REFETCH_DELAY_MS);
        }
    }

    /**
     * Returns a custom image cached with `cachedUrl` as key if it exists. Else returns resource
     * corresponding to `defaultIconId`.
     *
     * @param context {@link Context} to get the resources.
     * @param resolvedUrl The key for the cached custom image.
     * @param defaultIconId Resource id of the default fallback icon.
     * @return {@link Drawable} which is either the custom icon corresponding to `cachedUrl` from
     *     cache or the fallback icon corresponding to `defaultIconId` from resources. Prefers
     *     former over latter.
     */
    private Drawable getIcon(Context context, GURL resolvedUrl, int defaultIconId) {
        if (resolvedUrl.isValid() && mImagesCache.containsKey(resolvedUrl.getSpec())) {
            return new BitmapDrawable(
                    context.getResources(), mImagesCache.get(resolvedUrl.getSpec()));
        }

        return AppCompatResources.getDrawable(context, defaultIconId);
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

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.graphics.Bitmap;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.url.GURL;

/**
 * Fetches a Web Feed's favicon.
 *
 * If the Web Feed provides a Favicon URL, we first try to fetch the favicon image directly.
 * If that fails, we use LargeIconBridge to fetch the favicon for the page.
 * If that fails, we generate a monogram.
 */
@NullMarked
public class WebFeedFaviconFetcher {
    private final LargeIconBridge mLargeIconBridge;
    private final ImageFetcher mImageFetcher;

    public static WebFeedFaviconFetcher createDefault() {
        Profile profile = ProfileManager.getLastUsedRegularProfile();
        return new WebFeedFaviconFetcher(
                new LargeIconBridge(profile),
                ImageFetcherFactory.createImageFetcher(
                        ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                        profile.getProfileKey(),
                        GlobalDiscardableReferencePool.getReferencePool()));
    }

    public WebFeedFaviconFetcher(LargeIconBridge largeIconBridge, ImageFetcher imageFetcher) {
        mLargeIconBridge = largeIconBridge;
        mImageFetcher = imageFetcher;
    }

    /**
     * Begins fetching a favicon image. Calls `callback` when an image is obtained. The returned
     * `Bitmap` may be null if the URL is empty, or the domain cannot be resolved. See
     * https://crbug.com/987101.
     */
    public void beginFetch(
            int iconSizePx,
            int textSizePx,
            GURL siteUrl,
            @Nullable GURL faviconUrl,
            Callback<@Nullable Bitmap> callback) {
        Request request = new Request(siteUrl, faviconUrl, iconSizePx, textSizePx, callback);
        request.begin();
    }

    private static RoundedIconGenerator createRoundedIconGenerator(
            @ColorInt int iconColor, int iconSizePx, int textSizePx) {
        int cornerRadius = iconSizePx / 2;
        return new RoundedIconGenerator(
                iconSizePx, iconSizePx, cornerRadius, iconColor, textSizePx);
    }

    private class Request {
        public final GURL siteUrl;
        public final @Nullable GURL faviconUrl;
        public final int iconSizePx;
        public final int textSizePx;
        public final Callback<@Nullable Bitmap> callback;

        public Request(
                GURL siteUrl,
                @Nullable GURL faviconUrl,
                int iconSizePx,
                int textSizePx,
                Callback<@Nullable Bitmap> callback) {
            this.siteUrl = siteUrl;
            this.faviconUrl = faviconUrl;
            this.iconSizePx = iconSizePx;
            this.textSizePx = textSizePx;
            this.callback = callback;
        }

        void begin() {
            if (faviconUrl == null || !faviconUrl.isValid()) {
                fetchImageWithSiteUrl();
            } else {
                fetchImageWithFaviconUrl();
            }
        }

        private void fetchImageWithFaviconUrl() {
            assert faviconUrl != null && faviconUrl.isValid();
            mImageFetcher.fetchImage(
                    ImageFetcher.Params.create(
                            faviconUrl.getSpec(),
                            ImageFetcher.FEED_UMA_CLIENT_NAME,
                            iconSizePx,
                            iconSizePx),
                    this::onFaviconFetchedWithFaviconUrl);
        }

        private void fetchImageWithSiteUrl() {
            mLargeIconBridge.getLargeIconForUrl(
                    siteUrl, iconSizePx, this::onFaviconFetchedWithSiteUrl);
        }

        private void onFaviconFetchedWithFaviconUrl(@Nullable Bitmap bitmap) {
            if (bitmap == null) {
                fetchImageWithSiteUrl();
            } else {
                callback.onResult(bitmap);
            }
        }

        private void onFaviconFetchedWithSiteUrl(
                @Nullable Bitmap icon,
                @ColorInt int fallbackColor,
                boolean isColorDefault,
                @IconType int iconType) {
            if (icon == null) {
                // TODO(crbug.com/40158714): Update monogram according to specs.
                RoundedIconGenerator iconGenerator =
                        WebFeedFaviconFetcher.createRoundedIconGenerator(
                                fallbackColor, iconSizePx, textSizePx);
                icon = iconGenerator.generateIconForUrl(siteUrl);
            }
            callback.onResult(icon);
        }
    }
}

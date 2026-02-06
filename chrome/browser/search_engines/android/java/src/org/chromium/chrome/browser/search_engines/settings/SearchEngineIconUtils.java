// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import android.content.Context;
import android.graphics.Bitmap;
import android.widget.ImageView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.GoogleFaviconServerCallback;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.net.NetworkTrafficAnnotationTag;
import org.chromium.url.GURL;

import java.util.Map;

/** Utilities for fetching search engine icons. */
@NullMarked
public class SearchEngineIconUtils {
    private static final NetworkTrafficAnnotationTag TRAFFIC_ANNOTATION =
            NetworkTrafficAnnotationTag.createComplete(
                    "search_engine_adapter",
                    """
                    semantics {
                        sender: 'SearchEngineAdapter'
                        description: 'Sends a request to a Google server to retrieve the favicon bitmap.'
                        trigger:
                            'A request is sent when the user opens search engine settings and Chrome does '
                            'not have a favicon.'
                        data: 'Search engine URL and desired icon size.'
                        destination: GOOGLE_OWNED_SERVICE
                        internal {
                            contacts {
                                email: 'chrome-signin-team@google.com'
                            }
                            contacts {
                                email: 'triploblastic@google.com'
                            }
                        }
                        user_data {
                            type: NONE
                        }
                        last_reviewed: '2023-12-04'
                    }
                    policy {
                        cookies_allowed: NO
                        policy_exception_justification: 'Not implemented.'
                        setting: 'This feature cannot be disabled by settings.'
                    }\
                    """);

    /**
     * Updates the provided ImageView with the Search Engine icon. It checks the cache (if exists),
     * built-in resources, and falls back to fetching from the Google Server.
     *
     * @param context Context for resources.
     * @param logoView The ImageView to update.
     * @param templateUrl The search engine template.
     * @param faviconUrl The specific GURL for the favicon.
     * @param largeIconBridge The bridge to fetch icons.
     * @param iconCache Optional: A map to store/retrieve fetched bitmaps.
     */
    public static void updateIcon(
            Context context,
            ImageView logoView,
            TemplateUrl templateUrl,
            GURL faviconUrl,
            LargeIconBridge largeIconBridge,
            @Nullable Map<GURL, Bitmap> iconCache) {

        if (iconCache != null && iconCache.containsKey(faviconUrl)) {
            logoView.setImageBitmap(iconCache.get(faviconUrl));
            return;
        }

        @Nullable Bitmap bitmap = templateUrl.getBuiltInSearchEngineIcon();
        if (bitmap != null) {
            if (iconCache != null) {
                iconCache.put(faviconUrl, bitmap);
            }
            logoView.setImageBitmap(bitmap);
            return;
        }

        // Use a placeholder image while trying to fetch the logo.
        int uiElementSizeInPx =
                context.getResources().getDimensionPixelSize(R.dimen.search_engine_favicon_size);
        logoView.setImageBitmap(
                FaviconUtils.createGenericFaviconBitmap(context, uiElementSizeInPx, null));
        LargeIconCallback onFaviconAvailable =
                (icon, fallbackColor, isFallbackColorDefault, iconType) -> {
                    if (icon != null) {
                        logoView.setImageBitmap(icon);
                        if (iconCache != null) {
                            iconCache.put(faviconUrl, icon);
                        }
                    }
                };

        GoogleFaviconServerCallback googleServerCallback =
                (status) -> {
                    // Update the time the icon was last requested to avoid automatic eviction
                    // from cache.
                    largeIconBridge.touchIconFromGoogleServer(faviconUrl);
                    // The search engine logo will be fetched from google servers, so the actual
                    // size of the image is controlled by LargeIconService configuration.
                    // minSizePx=1 is used to accept logo of any size.
                    largeIconBridge.getLargeIconForUrl(
                            faviconUrl,
                            /* minSizePx= */ 1,
                            /* desiredSizePx= */ uiElementSizeInPx,
                            onFaviconAvailable);
                };
        // If the icon already exists in the cache no network request will be made, but the
        // callback will be triggered nonetheless.
        largeIconBridge.getLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                faviconUrl,
                /* shouldTrimPageUrlPath= */ true,
                TRAFFIC_ANNOTATION,
                googleServerCallback);
    }
}

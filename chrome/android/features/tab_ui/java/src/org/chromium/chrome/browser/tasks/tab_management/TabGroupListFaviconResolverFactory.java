// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.ViewUtils;
import org.chromium.url.GURL;

/** Builds the favicon resolver for a Tab Group List. */
@NullMarked
public class TabGroupListFaviconResolverFactory {

    /** Defines a strategy for resolving a favicon image for a given URL. */
    @FunctionalInterface
    interface ResolveFavicon {
        /**
         * Asynchronously resolves the favicon for the specified URL using a particular strategy.
         *
         * @param profile The user profile.
         * @param url The url for which the favicon should be resolved.
         * @param faviconSizePx The desired size of the favicon bitmap in pixels.
         * @param callback An observer to be notified asynchronously using the now resolved favicon.
         */
        void resolve(Profile profile, GURL url, int faviconSizePx, FaviconImageCallback callback);
    }

    /**
     * Builds a {@link FaviconResolver} that resolves favicons for a tab group list.
     *
     * @param context The context to use for resources.
     * @param profile The profile to use for favicon lookups.
     * @param fallbackProvider The provider to use for fallback favicons.
     */
    public static FaviconResolver build(
            Context context, Profile profile, TabListFaviconProvider fallbackProvider) {
        return buildResolver(
                context,
                profile,
                fallbackProvider,
                TabGroupListFaviconResolverFactory::resolveForeignFavicon);
    }

    /**
     * Builds a {@link FaviconResolver} that resolves favicons for a tab group list. Retrieves
     * favicons only for pages the user has visited on the current device,
     *
     * @param context The context to use for resources.
     * @param profile The profile to use for favicon lookups.
     * @param fallbackProvider The provider to use for fallback favicons.
     */
    public static FaviconResolver buildLocal(
            Context context, Profile profile, TabListFaviconProvider fallbackProvider) {
        return buildResolver(
                context,
                profile,
                fallbackProvider,
                TabGroupListFaviconResolverFactory::resolveLocalFavicon);
    }

    private static void resolveForeignFavicon(
            Profile profile, GURL url, int faviconSizePx, FaviconImageCallback callback) {
        FaviconHelper faviconHelper = new FaviconHelper();
        faviconHelper.getForeignFaviconImageForURL(profile, url, faviconSizePx, callback);
    }

    private static void resolveLocalFavicon(
            Profile profile, GURL url, int faviconSizePx, FaviconImageCallback callback) {
        FaviconHelper faviconHelper = new FaviconHelper();
        faviconHelper.getLocalFaviconImageForURL(profile, url, faviconSizePx, callback);
    }

    @NonNull
    private static FaviconResolver buildResolver(
            Context context,
            Profile profile,
            TabListFaviconProvider fallbackProvider,
            ResolveFavicon resolveFavicon) {
        return (GURL url, Callback<Drawable> callback) -> {
            if (UrlUtilities.isInternalScheme(url)) {
                callback.onResult(
                        fallbackProvider
                                .getRoundedChromeFavicon(/* isIncognito= */ false)
                                .getDefaultDrawable());
            } else {
                resolveFavicon(context, profile, fallbackProvider, url, callback, resolveFavicon);
            }
        };
    }

    private static void resolveFavicon(
            Context context,
            Profile profile,
            TabListFaviconProvider fallbackProvider,
            GURL url,
            Callback<Drawable> callback,
            ResolveFavicon resolveFavicon) {
        Resources resources = context.getResources();
        int faviconSizePixels = resources.getDimensionPixelSize(R.dimen.tab_grid_favicon_size);
        FaviconHelper faviconHelper = new FaviconHelper();
        FaviconImageCallback faviconImageCallback =
                (Bitmap bitmap, GURL ignored) -> {
                    onFavicon(context, fallbackProvider, callback, bitmap);
                    faviconHelper.destroy();
                };
        resolveFavicon.resolve(profile, url, faviconSizePixels, faviconImageCallback);
    }

    private static void onFavicon(
            Context context,
            TabListFaviconProvider fallbackProvider,
            Callback<Drawable> callback,
            Bitmap bitmap) {
        final Drawable drawable;
        if (bitmap == null) {
            drawable =
                    fallbackProvider
                            .getDefaultFavicon(/* isIncognito= */ false)
                            .getDefaultDrawable();
        } else {
            Resources resources = context.getResources();
            int cornerRadiusPixels =
                    resources.getDimensionPixelSize(R.dimen.default_favicon_corner_radius);
            drawable = ViewUtils.createRoundedBitmapDrawable(resources, bitmap, cornerRadiusPixels);
        }
        callback.onResult(drawable);
    }
}

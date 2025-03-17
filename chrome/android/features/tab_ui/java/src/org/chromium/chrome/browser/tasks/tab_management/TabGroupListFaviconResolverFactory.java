// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;

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

    /**
     * Builds a {@link FaviconResolver} that resolves favicons for a tab group list.
     *
     * @param context The context to use for resources.
     * @param profile The profile to use for favicon lookups.
     * @param fallbackProvider The provider to use for fallback favicons.
     */
    public static FaviconResolver build(
            Context context, Profile profile, TabListFaviconProvider fallbackProvider) {
        return (GURL url, Callback<Drawable> callback) -> {
            if (UrlUtilities.isInternalScheme(url)) {
                callback.onResult(
                        fallbackProvider
                                .getRoundedChromeFavicon(/* isIncognito= */ false)
                                .getDefaultDrawable());
            } else {
                resolveForeignFavicon(context, profile, fallbackProvider, url, callback);
            }
        };
    }

    private static void resolveForeignFavicon(
            Context context,
            Profile profile,
            TabListFaviconProvider fallbackProvider,
            GURL url,
            Callback<Drawable> callback) {
        Resources resources = context.getResources();
        int faviconSizePixels = resources.getDimensionPixelSize(R.dimen.tab_grid_favicon_size);
        FaviconHelper faviconHelper = new FaviconHelper();
        FaviconImageCallback faviconImageCallback =
                (Bitmap bitmap, GURL ignored) -> {
                    onForeignFavicon(context, fallbackProvider, callback, bitmap);
                    faviconHelper.destroy();
                };
        faviconHelper.getForeignFaviconImageForURL(
                profile, url, faviconSizePixels, faviconImageCallback);
    }

    private static void onForeignFavicon(
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

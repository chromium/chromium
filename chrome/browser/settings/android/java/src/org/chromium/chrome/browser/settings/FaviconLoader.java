// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.url.GURL;

/**
 * A helper class that groups a FaviconHelper with its corresponding Callback
 * to load favicons for settings views.
 *
 * This object is kept alive by being passed to the native
 * FaviconHelper.getLocalFaviconImageForURL. Its reference will be released after the callback
 * has been called.
 */
public class FaviconLoader {
    /** Loads a favicon or creates a fallback icon. */
    public static void loadFavicon(
            Context context,
            LargeIconBridge largeIconBridge,
            GURL faviconUrl,
            Callback<Drawable> callback) {
        Resources resources = context.getResources();
        int iconSize = resources.getDimensionPixelSize(R.dimen.default_favicon_size);
        int minFaviconSize = resources.getDimensionPixelSize(R.dimen.default_favicon_min_size);
        LargeIconBridge.LargeIconCallback largeIconCallback =
                (icon, fallbackColor, isFallbackColorDefault, iconType) -> {
                    Drawable iconDrawable =
                            FaviconUtils.getIconDrawableWithoutFilter(
                                    icon,
                                    faviconUrl,
                                    fallbackColor,
                                    FaviconUtils.createCircularIconGenerator(context),
                                    resources,
                                    iconSize);
                    callback.onResult(iconDrawable);
                };
        largeIconBridge.getLargeIconForUrl(faviconUrl, minFaviconSize, largeIconCallback);
    }
}

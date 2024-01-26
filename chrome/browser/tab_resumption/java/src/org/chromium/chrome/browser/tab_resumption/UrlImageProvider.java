// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.url.GURL;

/** Helper to retrieve or create an image to represent a URL. */
public class UrlImageProvider {
    /** Callback to pass URL image Bitmap fetch results. */
    public interface UrlImageCallback {
        void onBitmap(Bitmap bitmap);
    }

    /** Factory methods for inner objects, abstracted to enable testing. */
    public interface UrlImageSource {
        LargeIconBridge createLargeIconBridge();

        RoundedIconGenerator createIconGenerator();
    }

    protected final int mMinIconSizePx;
    protected final int mDesiredIconSizePx;
    protected final LargeIconBridge mLargeIconBridge;
    protected final RoundedIconGenerator mIconGenerator;

    UrlImageProvider(UrlImageSource source, Context context) {
        Resources res = context.getResources();
        mMinIconSizePx = (int) res.getDimensionPixelSize(R.dimen.default_favicon_min_size);
        mDesiredIconSizePx =
                (int) res.getDimensionPixelSize(R.dimen.tab_resumption_module_icon_source_size);
        mLargeIconBridge = source.createLargeIconBridge();
        mIconGenerator = source.createIconGenerator();
    }

    UrlImageProvider(Profile profile, Context context) {
        this(
                new UrlImageSource() {
                    @Override
                    public LargeIconBridge createLargeIconBridge() {
                        return new LargeIconBridge(profile);
                    }

                    @Override
                    public RoundedIconGenerator createIconGenerator() {
                        return FaviconUtils.createRoundedRectangleIconGenerator(context);
                    }
                },
                context);
    }

    public void destroy() {}

    /**
     * Asynchronously fetches a large favicon for a URL, and passes it to `callback`. If
     * unavailable, then passes a solid filled circular icon with a single letter as fallback.
     *
     * @param pageUrl URL for favicon.
     * @param callback Destination to pass resulting Bitmap.
     */
    public void fetchImageForUrl(GURL pageUrl, UrlImageCallback callback) {
        mLargeIconBridge.getLargeIconForUrl(
                pageUrl,
                mMinIconSizePx,
                mDesiredIconSizePx,
                (Bitmap icon, int fallbackColor, boolean isFallbackColorDefault, int iconType) -> {
                    if (icon == null) {
                        mIconGenerator.setBackgroundColor(fallbackColor);
                        icon = mIconGenerator.generateIconForUrl(pageUrl);
                    }
                    callback.onBitmap(icon);
                });
    }
}

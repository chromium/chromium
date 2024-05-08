// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridge;
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

    private final boolean mUseSalientImage;

    @Nullable private ImageServiceBridge mImageServiceBridge;

    private int mSalientImageSizeBigPx;

    private int mSalientImageSizeSmallPx;

    UrlImageProvider(
            UrlImageSource source,
            Context context,
            @Nullable ImageServiceBridge imageServiceBridge) {
        Resources res = context.getResources();
        mMinIconSizePx = res.getDimensionPixelSize(R.dimen.default_favicon_min_size);
        mDesiredIconSizePx =
                res.getDimensionPixelSize(R.dimen.tab_resumption_module_icon_source_size);
        mLargeIconBridge = source.createLargeIconBridge();
        mIconGenerator = source.createIconGenerator();

        mImageServiceBridge = imageServiceBridge;
        mUseSalientImage = TabResumptionModuleUtils.TAB_RESUMPTION_USE_SALIENT_IMAGE.getValue();
        if (mUseSalientImage) {
            mSalientImageSizeBigPx =
                    res.getDimensionPixelSize(R.dimen.tab_resumption_module_single_icon_size);
            mSalientImageSizeSmallPx = mDesiredIconSizePx;
        }
    }

    UrlImageProvider(
            Profile profile, Context context, @Nullable ImageServiceBridge imageServiceBridge) {
        this(
                // TODO(b/339269597): Moves the UrlImageSource into an separate java file and is
                // owned by the TabResumptionModuleBuilder.
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
                context,
                imageServiceBridge);
    }

    /**
     * Clean up the C++ side of this class. After the call, this class instance shouldn't be used.
     */
    public void destroy() {
        mLargeIconBridge.destroy();

        // The ImageServiceBridge is owned by the TabResumptionModuleBuilder, and will be destroyed
        // by TabResumptionModuleBuilder.
        if (mImageServiceBridge != null) {
            mImageServiceBridge = null;
        }
    }

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

    /**
     * Asynchronously fetches a salient image for a URL, and fallback to fetch the favicon if there
     * isn't any salient image available.
     */
    public void fetchSalientImageWithFallback(
            @NonNull GURL pageUrl,
            boolean showBigImage,
            Callback<Bitmap> onSalientImageReadyCallback,
            UrlImageCallback fallback) {
        assert mUseSalientImage && mImageServiceBridge != null;
        int imageSize = showBigImage ? mSalientImageSizeBigPx : mSalientImageSizeSmallPx;

        mImageServiceBridge.fetchImageFor(
                /* isAccountData= */ true,
                pageUrl,
                imageSize,
                (bitmap) -> {
                    if (bitmap != null) {
                        onSalientImageReadyCallback.onResult((Bitmap) bitmap);
                    } else {
                        // Fallback to fetch the favicon.
                        fetchImageForUrl(pageUrl, fallback);
                    }
                });
    }
}

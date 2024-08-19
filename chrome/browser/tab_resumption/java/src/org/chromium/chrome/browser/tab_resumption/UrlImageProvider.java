// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.util.Size;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridge;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.url.GURL;

/** Helper to retrieve or create an image to represent a URL or a tab. */
public class UrlImageProvider {
    /** Callback to pass URL image Bitmap fetch results. */
    public interface UrlImageCallback {
        void onBitmap(Bitmap bitmap);
    }

    /** Factory methods for image creation objects, abstracted to enable testing. */
    public interface UrlImageSource {
        ThumbnailProvider createThumbnailProvider();

        RoundedIconGenerator createIconGenerator();
    }

    protected final int mMinIconSizePx;
    protected final int mDesiredIconSizePx;
    protected final ThumbnailProvider mThumbnailProvider;
    protected final RoundedIconGenerator mIconGenerator;

    private final boolean mUseSalientImage;

    @Nullable private ImageServiceBridge mImageServiceBridge;
    private LargeIconBridge mLargeIconBridge;

    private int mSalientImageSizeBigPx;

    private int mSalientImageSizeSmallPx;

    UrlImageProvider(
            Context context,
            UrlImageSource source,
            @Nullable ImageServiceBridge imageServiceBridge,
            LargeIconBridge largeIconBridge) {
        Resources res = context.getResources();
        mMinIconSizePx = res.getDimensionPixelSize(R.dimen.default_favicon_min_size);
        mDesiredIconSizePx =
                res.getDimensionPixelSize(R.dimen.tab_resumption_module_icon_source_size);
        mThumbnailProvider = source.createThumbnailProvider();
        mIconGenerator = source.createIconGenerator();

        mImageServiceBridge = imageServiceBridge;
        mLargeIconBridge = largeIconBridge;
        mUseSalientImage = TabResumptionModuleUtils.TAB_RESUMPTION_USE_SALIENT_IMAGE.getValue();
        if (mUseSalientImage) {
            mSalientImageSizeBigPx =
                    res.getDimensionPixelSize(R.dimen.tab_resumption_module_single_icon_size);
            mSalientImageSizeSmallPx = mDesiredIconSizePx;
        }
    }

    /**
     * Clean up the C++ side of this class. After the call, this class instance shouldn't be used.
     */
    public void destroy() {
        // The ImageServiceBridge and mLargeIconBridge are owned by the TabResumptionModuleBuilder,
        // and will be destroyed by TabResumptionModuleBuilder.
        if (mLargeIconBridge != null) {
            mLargeIconBridge = null;
        }
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
        assert mLargeIconBridge != null;
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
    public void fetchSalientImage(
            @NonNull GURL pageUrl,
            boolean showBigImage,
            Callback<Bitmap> onSalientImageReadyCallback) {
        assert mUseSalientImage && mImageServiceBridge != null;
        int imageSize = showBigImage ? mSalientImageSizeBigPx : mSalientImageSizeSmallPx;

        mImageServiceBridge.fetchImageFor(
                /* isAccountData= */ true, pageUrl, imageSize, onSalientImageReadyCallback);
    }

    /** Asynchronously fetches a thumbnail image for a tab. */
    public void getTabThumbnail(
            int tabId, Size thumbnailSize, Callback<Drawable> tabThumbnailCallback) {
        mThumbnailProvider.getTabThumbnailWithCallback(
                tabId, thumbnailSize, /* isSelected= */ false, tabThumbnailCallback);
    }

    /** Returns whether this UrlImageProvider instance has been destroyed. */
    public boolean isDestroyed() {
        return mLargeIconBridge == null;
    }

    LargeIconBridge getLargeIconBridgeForTesting() {
        return mLargeIconBridge;
    }

    ImageServiceBridge getImageServiceBridgeForTesting() {
        return mImageServiceBridge;
    }
}

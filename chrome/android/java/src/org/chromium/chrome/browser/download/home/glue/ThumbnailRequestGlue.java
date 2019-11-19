// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.glue;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.widget.ThumbnailProvider;
import org.chromium.chrome.browser.widget.ThumbnailProvider.ThumbnailRequest;
import org.chromium.chrome.browser.widget.ThumbnailProviderImpl;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemVisuals;
import org.chromium.components.offline_items_collection.VisualsCallback;
import org.chromium.ui.display.DisplayAndroid;

/**
 * Glue class responsible for connecting the current downloads and {@link OfflineContentProvider}
 * thumbnail work to the {@link ThumbnailProvider} via a custom {@link ThumbnailProviderImpl}.
 */
public class ThumbnailRequestGlue implements ThumbnailRequest {
    private final OfflineContentProviderGlue mProvider;
    private final OfflineItem mItem;
    private final int mIconWidthPx;
    private final int mIconHeightPx;
    private final float mMaxThumbnailScaleFactor;
    private final VisualsCallback mCallback;

    /** Creates a {@link ThumbnailRequestGlue} instance. */
    public ThumbnailRequestGlue(OfflineContentProviderGlue provider, OfflineItem item,
            int iconWidthPx, int iconHeightPx, float maxThumbnailScaleFactor,
            VisualsCallback callback) {
        mProvider = provider;
        mItem = item;

        mIconWidthPx = iconWidthPx;
        mIconHeightPx = iconHeightPx;
        mMaxThumbnailScaleFactor = maxThumbnailScaleFactor;

        mCallback = callback;
    }

    // ThumbnailRequest implementation.
    @Override
    public String getFilePath() {
        return mItem.filePath;
    }

    @Override
    public String getMimeType() {
        return mItem.mimeType;
    }

    @Override
    public String getContentId() {
        return mItem.id.id;
    }

    @Override
    public void onThumbnailRetrieved(String contentId, Bitmap thumbnail) {
        OfflineItemVisuals visuals = null;
        if (thumbnail != null) {
            visuals = new OfflineItemVisuals();
            visuals.icon = thumbnail;
        }

        mCallback.onVisualsAvailable(mItem.id, visuals);
    }

    @Override
    public int getIconSize() {
        return mIconWidthPx;
    }

    @Override
    public boolean getThumbnail(Callback<Bitmap> callback) {
        return mProvider.getVisualsForItem(mItem.id, (id, visuals) -> {
            if (visuals == null || visuals.icon == null) {
                callback.onResult(null);
            } else {
                Bitmap bitmap = visuals.icon;
                int newWidth = bitmap.getWidth();
                int newHeight = bitmap.getHeight();

                // Downscale to save memory if the bitmap is not smaller than the icon view.
                if (newWidth > mIconWidthPx && newHeight > mIconHeightPx) {
                    newWidth = downscaleThumbnailSize(bitmap.getWidth());
                    newHeight = downscaleThumbnailSize(bitmap.getHeight());
                }

                // Fit the bitmap into the icon view. Note that we have to use width here because
                // the ThumbnailProviderImpl only keys off of width as well.
                int minDimension = Math.min(bitmap.getWidth(), bitmap.getHeight());
                if (minDimension > mIconWidthPx) {
                    newWidth = (int) (((long) bitmap.getWidth()) * mIconWidthPx / minDimension);
                    newHeight = (int) (((long) bitmap.getHeight()) * mIconWidthPx / minDimension);
                }

                if (bitmap.getWidth() != newWidth || bitmap.getHeight() != newHeight) {
                    bitmap = Bitmap.createScaledBitmap(bitmap, newWidth, newHeight, false);
                }

                callback.onResult(bitmap);
            }
        });
    }

    /**
     * Returns size in pixel used by the thumbnail request, considering dip scale factor.
     * @param currentSize The current size before considering the dip scale factor.
     */
    private int downscaleThumbnailSize(int currentSize) {
        DisplayAndroid display =
                DisplayAndroid.getNonMultiDisplay(ContextUtils.getApplicationContext());
        float scale = display.getDipScale();
        if (scale <= mMaxThumbnailScaleFactor) return currentSize;
        return (int) (mMaxThumbnailScaleFactor * currentSize / scale);
    }
}

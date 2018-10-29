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
    private final VisualsCallback mCallback;

    /** Creates a {@link ThumbnailRequestGlue} instance. */
    public ThumbnailRequestGlue(OfflineContentProviderGlue provider, OfflineItem item,
            int iconWidthPx, int iconHeightPx, float maxThumbnailScaleFactor,
            VisualsCallback callback) {
        mProvider = provider;
        mItem = item;

        // Scale the thumbnail quality to mdpi for high dpi devices.
        mIconWidthPx = downscaleThumbnailSize(iconWidthPx, maxThumbnailScaleFactor);
        mIconHeightPx = downscaleThumbnailSize(iconHeightPx, maxThumbnailScaleFactor);

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

                int minDimension = Math.min(bitmap.getWidth(), bitmap.getHeight());
                // Note that we have to use width here because the ThumbnailProviderImpl only keys
                // off of width as well.
                if (minDimension > mIconWidthPx) {
                    int newWidth = (int) (((long) bitmap.getWidth()) * mIconWidthPx / minDimension);
                    int newHeight =
                            (int) (((long) bitmap.getHeight()) * mIconWidthPx / minDimension);
                    bitmap = Bitmap.createScaledBitmap(bitmap, newWidth, newHeight, false);
                }

                callback.onResult(bitmap);
            }
        });
    }

    /**
     * Returns size in pixel used by the thumbnail request, considering dip scale factor.
     * @param currentSize The current size before considering the dip scale factor.
     * @param maxScaleFactor The maximum scale factor we expected to show as the thumbnail. Device
     *                       with higher scale factor will be downscaled to this level.
     */
    private int downscaleThumbnailSize(int currentSize, float maxScaleFactor) {
        DisplayAndroid display =
                DisplayAndroid.getNonMultiDisplay(ContextUtils.getApplicationContext());
        float scale = display.getDipScale();
        if (scale <= maxScaleFactor) return currentSize;
        return (int) (maxScaleFactor * currentSize / scale);
    }
}

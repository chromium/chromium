// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.os.SystemClock;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.text.TextUtils;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;

import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Holds on to a {@link PickerBitmapView} that displays information about a picker bitmap.
 */
public class PickerBitmapViewHolder
        extends ViewHolder implements DecoderServiceHost.ImageDecodedCallback {
    // Our parent category.
    private PickerCategoryView mCategoryView;

    // The bitmap view we are holding on to.
    private final PickerBitmapView mItemView;

    // The request we are showing the bitmap for.
    private PickerBitmap mBitmapDetails;

    /**
     * The PickerBitmapViewHolder.
     * @param itemView The {@link PickerBitmapView} view for showing the image.
     */
    public PickerBitmapViewHolder(PickerBitmapView itemView) {
        super(itemView);
        mItemView = itemView;
    }

    // DecoderServiceHost.ImageDecodedCallback

    @Override
    public void imageDecodedCallback(String filePath, Bitmap bitmap) {
        if (bitmap == null || bitmap.getWidth() == 0 || bitmap.getHeight() == 0) {
            return;
        }

        if (mCategoryView.getHighResBitmaps().get(filePath) == null) {
            mCategoryView.getHighResBitmaps().put(filePath, bitmap);
        }

        if (mCategoryView.getLowResBitmaps().get(filePath) == null) {
            Resources resources = mItemView.getContext().getResources();
            new BitmapScalerTask(mCategoryView.getLowResBitmaps(), filePath,
                    resources.getDimensionPixelSize(R.dimen.photo_picker_grainy_thumbnail_size),
                    bitmap)
                    .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }

        if (!TextUtils.equals(mBitmapDetails.getFilePath(), filePath)) {
            return;
        }

        if (mItemView.setThumbnailBitmap(bitmap)) {
            mItemView.fadeInThumbnail();
        }
    }

    /**
     * Display a single item from |position| in the PickerCategoryView.
     * @param categoryView The PickerCategoryView to use to fetch the image.
     * @param position The position of the item to fetch.
     * @return The decoding action required to display the item.
     */
    public @PickerAdapter.DecodeActions int displayItem(
            PickerCategoryView categoryView, int position) {
        mCategoryView = categoryView;

        List<PickerBitmap> pickerBitmaps = mCategoryView.getPickerBitmaps();
        mBitmapDetails = pickerBitmaps.get(position);

        if (mBitmapDetails.type() == PickerBitmap.TileTypes.CAMERA
                || mBitmapDetails.type() == PickerBitmap.TileTypes.GALLERY) {
            mItemView.initialize(mBitmapDetails, null, false);
            return PickerAdapter.DecodeActions.NO_ACTION;
        }

        String filePath = mBitmapDetails.getFilePath();
        Bitmap original = mCategoryView.getHighResBitmaps().get(filePath);
        if (original != null) {
            mItemView.initialize(mBitmapDetails, original, false);
            return PickerAdapter.DecodeActions.FROM_CACHE;
        }

        int size = mCategoryView.getImageSize();
        Bitmap placeholder = mCategoryView.getLowResBitmaps().get(filePath);
        if (placeholder != null) {
            // For performance stats see http://crbug.com/719919.
            long begin = SystemClock.elapsedRealtime();
            placeholder = BitmapUtils.scale(placeholder, size, false);
            long scaleTime = SystemClock.elapsedRealtime() - begin;
            RecordHistogram.recordTimesHistogram(
                    "Android.PhotoPicker.UpscaleLowResBitmap", scaleTime, TimeUnit.MILLISECONDS);

            mItemView.initialize(mBitmapDetails, placeholder, true);
        } else {
            mItemView.initialize(mBitmapDetails, null, true);
        }

        mCategoryView.getDecoderServiceHost().decodeImage(filePath, size, this);
        return PickerAdapter.DecodeActions.DECODE;
    }

    /**
     * Returns the file path of the current request.
     */
    public String getFilePath() {
        return mBitmapDetails == null ? null : mBitmapDetails.getFilePath();
    }
}

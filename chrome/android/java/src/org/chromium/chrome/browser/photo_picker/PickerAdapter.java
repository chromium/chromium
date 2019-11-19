// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.support.v7.widget.RecyclerView.Adapter;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;

import org.chromium.chrome.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A data adapter for the Photo Picker.
 */
public class PickerAdapter extends Adapter<ViewHolder> {
    // The possible types of actions required during decoding.
    @IntDef({DecodeActions.NO_ACTION, DecodeActions.FROM_CACHE, DecodeActions.DECODE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DecodeActions {
        int NO_ACTION = 0; // Gallery/Camera tile: No action.
        int FROM_CACHE = 1; // Image already decoded.
        int DECODE = 2; // Image needed to be decoded.
    }

    // The category view to use to show the images.
    private PickerCategoryView mCategoryView;

    // How many times the (high-res) cache was useful.
    @DecodeActions
    private int mCacheHits;

    // How many times a decoding was requested.
    @DecodeActions
    private int mDecodeRequests;

    /**
     * The PickerAdapter constructor.
     * @param categoryView The category view to use to show the images.
     */
    public PickerAdapter(PickerCategoryView categoryView) {
        mCategoryView = categoryView;
    }

    // RecyclerView.Adapter:

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View itemView = LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.photo_picker_bitmap_view, parent, false);
        PickerBitmapView bitmapView = (PickerBitmapView) itemView;
        bitmapView.setCategoryView(mCategoryView);
        return new PickerBitmapViewHolder(bitmapView);
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        if (holder instanceof PickerBitmapViewHolder) {
            PickerBitmapViewHolder myHolder = (PickerBitmapViewHolder) holder;
            @DecodeActions
            int result = myHolder.displayItem(mCategoryView, position);
            if (result == DecodeActions.FROM_CACHE) {
                mCacheHits++;
            } else if (result == DecodeActions.DECODE) {
                mDecodeRequests++;
            }
        }
    }

    @Override
    public int getItemCount() {
        return mCategoryView.getPickerBitmaps().size();
    }

    /** Returns the number of times the cache supplied a bitmap. */
    public int getCacheHitCount() {
        return mCacheHits;
    }

    /** Returns the number of decode requests (cache-misses). */
    public int getDecodeRequestCount() {
        return mDecodeRequests;
    }
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.R;

import java.util.List;

/** Adapter for the RecyclerView in the Chrome Colors bottom sheet. */
@NullMarked
public class NtpChromeColorsAdapter
        extends RecyclerView.Adapter<NtpChromeColorsAdapter.ColorViewHolder> {

    private final List<NtpThemeColorInfo> mColorInfoList;
    private final Context mContext;
    private final Callback<NtpThemeColorInfo> mOnClickCallback;

    // Variable to hold the position of the selected item.
    private int mSelectedPosition;

    public NtpChromeColorsAdapter(
            Context context,
            List<NtpThemeColorInfo> colorInfoList,
            Callback<NtpThemeColorInfo> onClickCallback,
            int selectedPosition) {
        mContext = context;
        mColorInfoList = colorInfoList;
        mOnClickCallback = onClickCallback;
        mSelectedPosition = selectedPosition;
    }

    @Override
    public ColorViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View view =
                LayoutInflater.from(mContext)
                        .inflate(
                                R.layout.ntp_customization_chrome_colors_grid_item_layout,
                                parent,
                                false);
        return new ColorViewHolder(view);
    }

    @Override
    public void onBindViewHolder(ColorViewHolder holder, int position) {
        NtpThemeColorInfo colorInfo = mColorInfoList.get(position);
        View.OnClickListener clickListener =
                v -> {
                    setSelectedPositionImpl(holder.getBindingAdapterPosition(), colorInfo);
                };

        holder.bind(colorInfo, clickListener, mSelectedPosition);
    }

    @Override
    public void onViewRecycled(ColorViewHolder holder) {
        holder.itemView.setOnClickListener(null);
    }

    @Override
    public int getItemCount() {
        return mColorInfoList.size();
    }

    /**
     * Selects the given position.
     *
     * @param position The position of the newly selected item
     */
    void setSelectedPosition(int position) {
        NtpThemeColorInfo colorInfo = null;

        if (position > RecyclerView.NO_POSITION && position < mColorInfoList.size()) {
            colorInfo = mColorInfoList.get(position);
        } else {
            // If the position is invalid, set to RecyclerView.NO_POSITION.
            position = RecyclerView.NO_POSITION;
        }

        setSelectedPositionImpl(position, colorInfo);
    }

    /**
     * Called when an new position is selected. It highlights the new position and removes the
     * highlight of the previously selected position if applied.
     *
     * @param newPosition The newly selected position
     * @param colorInfo The corresponding colorInfo of the newly selected position
     */
    private void setSelectedPositionImpl(int newPosition, @Nullable NtpThemeColorInfo colorInfo) {
        if (colorInfo != null) {
            mOnClickCallback.onResult(colorInfo);
        }

        if (mSelectedPosition == newPosition) {
            return;
        }

        // Notify the adapter to un-draw its selection.
        if (mSelectedPosition != RecyclerView.NO_POSITION) {
            notifyItemChanged(mSelectedPosition);
        }

        // Notify the adapter to draw the new selection.
        mSelectedPosition = newPosition;
        if (newPosition != RecyclerView.NO_POSITION) {
            notifyItemChanged(newPosition);
        }
    }

    Callback<NtpThemeColorInfo> getOnItemClickedCallbackForTesting() {
        return mOnClickCallback;
    }

    List<NtpThemeColorInfo> getColorsForTesting() {
        return mColorInfoList;
    }

    int getSelectedPositionForTesting() {
        return mSelectedPosition;
    }

    /** ColorViewHolder that holds references to the views for a single color item. */
    public static class ColorViewHolder extends RecyclerView.ViewHolder {
        public ColorViewHolder(View itemView) {
            super(itemView);
        }

        /** Binds a colorInfo, a click listener and the current selected position to the view. */
        void bind(
                NtpThemeColorInfo colorInfo,
                View.OnClickListener onClickListener,
                int selectedPosition) {
            bindImpl(colorInfo, onClickListener, selectedPosition, getBindingAdapterPosition());
        }

        @VisibleForTesting
        void bindImpl(
                NtpThemeColorInfo colorInfo,
                View.OnClickListener onClickListener,
                int selectedPosition,
                int bindingAdaptorPosition) {
            ImageView colorCircle = itemView.findViewById(R.id.color_circle);
            colorCircle.setForeground(colorInfo.iconDrawable);
            itemView.setOnClickListener(onClickListener);

            if (colorInfo.colorStringResId != NtpThemeColorUtils.INVALID_ID) {
                String contentDescription =
                        itemView.getResources().getString(colorInfo.colorStringResId);
                itemView.setContentDescription(contentDescription);
            }

            // Sets the activated status.
            boolean isSelected = bindingAdaptorPosition == selectedPosition;
            itemView.setActivated(isSelected);
            // It allows to pronounce "selected" when isSelected is true.
            itemView.setSelected(isSelected);
        }
    }
}

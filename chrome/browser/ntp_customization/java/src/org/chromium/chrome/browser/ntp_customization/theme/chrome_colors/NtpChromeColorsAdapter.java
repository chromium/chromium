// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.NonNull;
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
    // The last selected ItemView.
    private @Nullable View mSelectedItemView;

    // TODO(https://crbug.com/440584354): Make NtpChromeColorsAdapter follow the MVC design patten.
    public NtpChromeColorsAdapter(
            Context context,
            List<NtpThemeColorInfo> colorInfoList,
            Callback<NtpThemeColorInfo> onClickCallback) {
        mContext = context;
        mColorInfoList = colorInfoList;
        mOnClickCallback = onClickCallback;
    }

    @NonNull
    @Override
    public ColorViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view =
                LayoutInflater.from(mContext)
                        .inflate(
                                R.layout.ntp_customization_chrome_colors_grid_item_layout,
                                parent,
                                false);
        return new ColorViewHolder(view);
    }

    @SuppressWarnings("notifyDataSetChanged")
    @Override
    public void onBindViewHolder(@NonNull ColorViewHolder holder, int position) {
        NtpThemeColorInfo colorInfo = mColorInfoList.get(position);

        if (mSelectedPosition == position) {
            mSelectedItemView = holder.itemView;
            holder.itemView.setActivated(true);
        }

        View.OnClickListener clickListener =
                v -> {
                    mOnClickCallback.onResult(colorInfo);
                    int newPosition = holder.getBindingAdapterPosition();

                    if (mSelectedPosition == newPosition) {
                        return;
                    }

                    // De-select the previous selected item.
                    if (mSelectedItemView != null) {
                        mSelectedItemView.setActivated(false);
                    }
                    holder.itemView.setActivated(true);

                    // Set the new selected position.
                    mSelectedPosition = newPosition;
                    mSelectedItemView = holder.itemView;

                    notifyDataSetChanged();
                };
        holder.bind(colorInfo, clickListener);
    }

    @Override
    public void onViewRecycled(ColorViewHolder holder) {
        holder.itemView.setOnClickListener(null);
    }

    @Override
    public int getItemCount() {
        return mColorInfoList.size();
    }

    /** ColorViewHolder that holds references to the views for a single color item. */
    public static class ColorViewHolder extends RecyclerView.ViewHolder {
        private final ImageView mColorCircle;

        public ColorViewHolder(@NonNull View itemView) {
            super(itemView);
            mColorCircle = itemView.findViewById(R.id.color_circle);
        }

        /** Binds a colorInfo and a click listener to the view. */
        void bind(NtpThemeColorInfo colorInfo, View.OnClickListener onItemClickListener) {
            mColorCircle.setForeground(colorInfo.iconDrawable);
            itemView.setOnClickListener(onItemClickListener);
        }
    }
}

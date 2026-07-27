// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;

import java.util.List;

@NullMarked
public class NtpThemeSyncHistoryRecyclerViewAdaptor
        extends RecyclerView.Adapter<NtpThemeSyncHistoryRecyclerViewAdaptor.ImageViewHolder> {
    /** Callback interface for when an item in the adapter is clicked. */
    public interface OnItemClickCallback {
        void onClicked(NtpBackgroundDataBase backgroundData, int position);
    }

    private final Context mContext;
    private final List<NtpBackgroundDataBase> mBackgroundDataList;
    private final OnItemClickCallback mOnClickCallback;

    // Variable to hold the position of the selected item.
    private int mSelectedPosition;

    /**
     * Constructs a new {@link NtpThemeSyncHistoryRecyclerViewAdaptor}.
     *
     * @param context The {@link Context} used to inflate layouts and resolve resources.
     * @param backgroundDataList The list of {@link NtpBackgroundDataBase} representing the sync
     *     history.
     * @param onClickCallback The callback to invoke when a theme item is selected.
     * @param selectedPosition The initially selected position, or {@link
     *     androidx.recyclerview.widget.RecyclerView#NO_POSITION}.
     */
    public NtpThemeSyncHistoryRecyclerViewAdaptor(
            Context context,
            List<NtpBackgroundDataBase> backgroundDataList,
            OnItemClickCallback onClickCallback,
            int selectedPosition) {
        mContext = context;
        mBackgroundDataList = backgroundDataList;
        mOnClickCallback = onClickCallback;
        mSelectedPosition = selectedPosition;
    }

    @Override
    public NtpThemeSyncHistoryRecyclerViewAdaptor.ImageViewHolder onCreateViewHolder(
            ViewGroup parent, int viewType) {
        View view =
                LayoutInflater.from(mContext)
                        .inflate(
                                org.chromium.chrome.browser.ntp_customization.R.layout
                                        .ntp_customization_theme_sync_selected_item_layout,
                                parent,
                                false);
        return new NtpThemeSyncHistoryRecyclerViewAdaptor.ImageViewHolder(view);
    }

    @Override
    public void onBindViewHolder(ImageViewHolder holder, int position) {
        NtpBackgroundDataBase ntpBackgroundData = mBackgroundDataList.get(position);
        View.OnClickListener clickListener =
                v -> {
                    setSelectedPositionImpl(
                            holder.getBindingAdapterPosition(),
                            ntpBackgroundData,
                            /* isFromClick= */ true);
                };

        holder.bind(ntpBackgroundData, clickListener, mSelectedPosition);
    }

    @Override
    public void onViewRecycled(ImageViewHolder holder) {
        holder.itemView.setOnClickListener(null);
    }

    @Override
    public int getItemCount() {
        return mBackgroundDataList.size();
    }

    /**
     * Selects the given position.
     *
     * @param position The position of the newly selected item
     * @param isFromClick Whether this selection was triggered by a user click. If true, external
     *     listeners are notified via the selection callback. If false, only the visual highlight is
     *     updated.
     */
    @VisibleForTesting
    void setSelectedPosition(int position, boolean isFromClick) {
        NtpBackgroundDataBase backgroundData = null;

        if (position > RecyclerView.NO_POSITION && position < mBackgroundDataList.size()) {
            backgroundData = mBackgroundDataList.get(position);
        } else {
            // If the position is invalid, set to RecyclerView.NO_POSITION.
            position = RecyclerView.NO_POSITION;
        }

        setSelectedPositionImpl(position, backgroundData, isFromClick);
    }

    /**
     * Called when an new position is selected. It highlights the new position and removes the
     * highlight of the previously selected position if applied.
     *
     * @param newPosition The newly selected position
     * @param backgroundDataInfo The corresponding colorInfo of the newly selected position
     * @param isFromClick Whether this selection was explicitly triggered by a user click. The
     *     {@code mOnClickCallback} is invoked if this is true.
     */
    private void setSelectedPositionImpl(
            int newPosition,
            @Nullable NtpBackgroundDataBase backgroundDataInfo,
            boolean isFromClick) {
        if (mSelectedPosition == newPosition) {
            return;
        }

        if (backgroundDataInfo != null && isFromClick) {
            mOnClickCallback.onClicked(backgroundDataInfo, newPosition);
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

    /**
     * ColorViewHolder that holds references to the views for a single theme selection history item.
     */
    public static class ImageViewHolder extends RecyclerView.ViewHolder {
        public ImageViewHolder(View itemView) {
            super(itemView);
        }

        /**
         * Binds the background data, a click listener and the current selected position to the
         * view.
         *
         * @param backgroundData The background data to bind.
         * @param onClickListener The click listener for the item view.
         * @param selectedPosition The currently selected position in the adapter.
         */
        void bind(
                NtpBackgroundDataBase backgroundData,
                View.OnClickListener onClickListener,
                int selectedPosition) {
            bindImpl(
                    backgroundData, onClickListener, selectedPosition, getBindingAdapterPosition());
        }

        /**
         * Binds the background data, a click listener, the current selected position, and the
         * adapter position to the view.
         *
         * @param backgroundData The background data to bind.
         * @param onClickListener The click listener for the item view.
         * @param selectedPosition The currently selected position in the adapter.
         * @param bindingAdaptorPosition The position of this ViewHolder in the adapter.
         */
        @VisibleForTesting
        void bindImpl(
                NtpBackgroundDataBase backgroundData,
                View.OnClickListener onClickListener,
                int selectedPosition,
                int bindingAdaptorPosition) {
            ImageView backgroundView = itemView.findViewById(R.id.background_view);
            Drawable image = backgroundData.getImageDrawable();
            if (image != null) {
                backgroundView.setImageBitmap(null);
                backgroundView.setForeground(image);
            } else {
                backgroundView.setForeground(null);
                backgroundData.getBitmapOrLoadImage(
                        (result) -> backgroundView.setImageBitmap(result));
            }

            itemView.setOnClickListener(onClickListener);

            ImageView badgeView = itemView.findViewById(R.id.platform_badge);
            @PlatformType int platformType = backgroundData.getPlatformType();
            if (platformType != PlatformType.ANDROID) {
                boolean isMobile = platformType == PlatformType.IOS;
                if (isMobile) {
                    badgeView.setBackgroundResource(R.drawable.mobile_badge);
                } else {
                    badgeView.setBackgroundResource(R.drawable.desktop_badge);
                }
                badgeView.setVisibility(View.VISIBLE);
            } else {
                badgeView.setVisibility(View.GONE);
            }

            // Sets the activated status.
            boolean isSelected = bindingAdaptorPosition == selectedPosition;
            itemView.setActivated(isSelected);
            // It allows to pronounce "selected" when isSelected is true.
            itemView.setSelected(isSelected);
        }
    }

    public int getSelectedPositionForTesting() {
        return mSelectedPosition;
    }
}

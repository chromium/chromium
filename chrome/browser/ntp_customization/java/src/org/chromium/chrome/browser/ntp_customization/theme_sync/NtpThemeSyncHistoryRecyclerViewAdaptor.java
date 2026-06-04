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

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase.PlatformType;

import java.util.List;

@NullMarked
public class NtpThemeSyncHistoryRecyclerViewAdaptor
        extends RecyclerView.Adapter<NtpThemeSyncHistoryRecyclerViewAdaptor.ImageViewHolder> {
    private final Context mContext;
    private final List<NtpBackgroundDataBase> mBackgroundDataList;
    private final Callback<NtpBackgroundDataBase> mOnClickCallback;

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
            Callback<NtpBackgroundDataBase> onClickCallback,
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

        holder.bind(
                ntpBackgroundData.getPlatformType(),
                ntpBackgroundData.getImageDrawable(),
                clickListener,
                mSelectedPosition);
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
            mOnClickCallback.onResult(backgroundDataInfo);
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
         * Binds the platform type, a drawable, a click listener and the current selected position
         * to the view.
         */
        void bind(
                @PlatformType int platformType,
                @Nullable Drawable drawable,
                View.OnClickListener onClickListener,
                int selectedPosition) {
            bindImpl(
                    platformType,
                    drawable,
                    onClickListener,
                    selectedPosition,
                    getBindingAdapterPosition());
        }

        @VisibleForTesting
        void bindImpl(
                @PlatformType int platformType,
                @Nullable Drawable image,
                View.OnClickListener onClickListener,
                int selectedPosition,
                int bindingAdaptorPosition) {
            if (image != null) {
                ImageView backgroundView = itemView.findViewById(R.id.background_view);
                backgroundView.setForeground(image);
            }
            itemView.setOnClickListener(onClickListener);

            ImageView badgeView = itemView.findViewById(R.id.platform_badge);
            if (platformType != PlatformType.ANDROID_LOCAL) {
                boolean isMobile =
                        platformType == PlatformType.ANDROID_REMOTE
                                || platformType == PlatformType.IOS;
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

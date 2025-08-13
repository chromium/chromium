// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.R;

import java.util.List;

/** Adapter for the RecyclerView in the Chrome Colors bottom sheet. */
@NullMarked
public class NtpChromeColorsAdapter
        extends RecyclerView.Adapter<NtpChromeColorsAdapter.ColorViewHolder> {

    private final List<Integer> mColorResources;
    private final Context mContext;

    public NtpChromeColorsAdapter(Context context, List<Integer> colorResources) {
        mContext = context;
        mColorResources = colorResources;
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

    @Override
    public void onBindViewHolder(@NonNull ColorViewHolder holder, int position) {
        holder.bind(mColorResources.get(position));
    }

    @Override
    public int getItemCount() {
        return mColorResources.size();
    }

    /** ColorViewHolder that holds references to the views for a single color item. */
    public static class ColorViewHolder extends RecyclerView.ViewHolder {
        private final ImageView mColorCircle;

        public ColorViewHolder(@NonNull View itemView) {
            super(itemView);
            mColorCircle = itemView.findViewById(R.id.color_circle);
        }

        /**
         * Binds a color to the view.
         *
         * @param colorResId The resource ID of the color to bind.
         */
        void bind(int colorResId) {
            Context context = itemView.getContext();
            if (mColorCircle.getBackground() instanceof GradientDrawable) {
                GradientDrawable background = (GradientDrawable) mColorCircle.getBackground();
                background.setColor(ContextCompat.getColor(context, colorResId));
            }
        }
    }
}

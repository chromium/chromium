// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.R;

import java.util.List;

/** Adapter for the RecyclerView in the theme collections bottom sheet. */
@NullMarked
public class NtpThemeCollectionsAdapter
        extends RecyclerView.Adapter<NtpThemeCollectionsAdapter.ThemeCollectionViewHolder> {

    private final List<Pair<String, Integer>> mThemeCollections;

    public NtpThemeCollectionsAdapter(List<Pair<String, Integer>> themeCollections) {
        mThemeCollections = themeCollections;
    }

    @NonNull
    @Override
    public ThemeCollectionViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view =
                LayoutInflater.from(parent.getContext())
                        .inflate(
                                R.layout.ntp_customization_theme_collections_list_item_layout,
                                parent,
                                false);
        return new ThemeCollectionViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull ThemeCollectionViewHolder holder, int position) {
        Pair<String, Integer> collectionItem = mThemeCollections.get(position);

        holder.mTitle.setText(collectionItem.first);
        holder.mImage.setImageResource(collectionItem.second);
    }

    @Override
    public int getItemCount() {
        return mThemeCollections.size();
    }

    /**
     * ThemeCollectionViewHolder that holds references to the views for a single theme collections
     * item.
     */
    public static class ThemeCollectionViewHolder extends RecyclerView.ViewHolder {
        final ImageView mImage;
        final TextView mTitle;

        public ThemeCollectionViewHolder(@NonNull View itemView) {
            super(itemView);
            mImage = itemView.findViewById(R.id.theme_collection_cover_image);
            mTitle = itemView.findViewById(R.id.theme_collection_title);
        }
    }
}

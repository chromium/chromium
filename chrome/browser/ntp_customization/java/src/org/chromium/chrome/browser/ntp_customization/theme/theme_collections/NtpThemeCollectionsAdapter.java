// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsAdapter.ThemeCollectionsItemType.SINGLE_THEME_COLLECTION_ITEM;
import static org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsAdapter.ThemeCollectionsItemType.THEME_COLLECTIONS_ITEM;

import android.support.annotation.IntDef;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Adapter for the RecyclerView in both the theme collections bottom sheet and the single theme
 * collection bottom sheet.
 */
@NullMarked
public class NtpThemeCollectionsAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {

    private final List<Object> mItems;
    private final @ThemeCollectionsItemType int mThemeCollectionsItemType;
    private final View.@Nullable OnClickListener mOnClickListener;
    private @Nullable RecyclerView mRecyclerView;

    @IntDef({
        ThemeCollectionsItemType.THEME_COLLECTIONS_ITEM,
        ThemeCollectionsItemType.SINGLE_THEME_COLLECTION_ITEM,
        ThemeCollectionsItemType.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ThemeCollectionsItemType {
        int THEME_COLLECTIONS_ITEM = 0;
        int SINGLE_THEME_COLLECTION_ITEM = 1;
        int NUM_ENTRIES = 2;
    }

    /**
     * Constructor for the NTP theme collections adapter.
     *
     * @param items A list of items to display. Can be of type List<Pair<String, Integer>> for items
     *     with titles or List<Integer> for image-only items.
     * @param themeCollectionsItemType The type of the theme collections items in the RecycleView.
     * @param onClickListener The {@link View.OnClickListener} for each theme collection item.
     */
    public NtpThemeCollectionsAdapter(
            List<?> items,
            @ThemeCollectionsItemType int themeCollectionsItemType,
            View.@Nullable OnClickListener onClickListener) {
        mItems = new ArrayList<>(items);
        mThemeCollectionsItemType = themeCollectionsItemType;
        mOnClickListener = onClickListener;
    }

    @Override
    public void onAttachedToRecyclerView(@NonNull RecyclerView recyclerView) {
        super.onAttachedToRecyclerView(recyclerView);
        mRecyclerView = recyclerView;
    }

    @Override
    public int getItemViewType(int position) {
        return mThemeCollectionsItemType;
    }

    @NonNull
    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        LayoutInflater inflater = LayoutInflater.from(parent.getContext());
        View view =
                inflater.inflate(
                        R.layout.ntp_customization_theme_collections_list_item_layout,
                        parent,
                        false);
        return new ThemeCollectionViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull RecyclerView.ViewHolder holder, int position) {
        ThemeCollectionViewHolder viewHolder = (ThemeCollectionViewHolder) holder;

        switch (holder.getItemViewType()) {
            case THEME_COLLECTIONS_ITEM:
                Pair<String, Integer> collectionItem = (Pair<String, Integer>) mItems.get(position);
                viewHolder.mTitle.setText(collectionItem.first);
                viewHolder.mImage.setImageResource(collectionItem.second);
                break;
            case SINGLE_THEME_COLLECTION_ITEM:
                Integer imageRes = (Integer) mItems.get(position);
                viewHolder.mImage.setImageResource(imageRes);
                viewHolder.mTitle.setVisibility(View.GONE);
                break;
            default:
                assert false : "Theme collections item type not supported!";
        }
        viewHolder.mView.setOnClickListener(mOnClickListener);
    }

    @Override
    public int getItemCount() {
        return mItems.size();
    }

    /**
     * Updates the items in the adapter and notifies the RecyclerView of the change.
     *
     * @param newItems The new list of items to display.
     */
    public void setItems(List<?> newItems) {
        int oldSize = mItems.size();
        mItems.clear();
        notifyItemRangeRemoved(0, oldSize);
        mItems.addAll(newItems);
        notifyItemRangeInserted(0, newItems.size());
    }

    /** Clears the OnClickListener from all items in the RecyclerView. */
    public void clearOnClickListeners() {
        if (mRecyclerView == null) return;

        for (int i = 0; i < mRecyclerView.getChildCount(); i++) {
            RecyclerView.ViewHolder holder =
                    mRecyclerView.getChildViewHolder(mRecyclerView.getChildAt(i));
            if (holder != null) {
                holder.itemView.setOnClickListener(null);
            }
        }
    }

    /** ViewHolder for items that include an image and an optional title. */
    public static class ThemeCollectionViewHolder extends RecyclerView.ViewHolder {
        final View mView;
        final ImageView mImage;
        final TextView mTitle;

        public ThemeCollectionViewHolder(@NonNull View itemView) {
            super(itemView);
            mView = itemView;
            mImage = itemView.findViewById(R.id.theme_collection_image);
            mTitle = itemView.findViewById(R.id.theme_collection_title);
        }
    }
}

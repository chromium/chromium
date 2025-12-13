// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsAdapter.ThemeCollectionsItemType.SINGLE_THEME_COLLECTION_ITEM;
import static org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsAdapter.ThemeCollectionsItemType.THEME_COLLECTIONS_ITEM;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.url.GURL;

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
    private final ImageFetcher mImageFetcher;
    private @Nullable RecyclerView mRecyclerView;
    private @Nullable String mSelectedThemeCollectionId;
    private @Nullable GURL mSelectedThemeCollectionImageUrl;

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
     * @param items A list of items to display. Can be of type List<BackgroundCollection> for theme
     *     collections or List<CollectionImage> for theme collection images.
     * @param themeCollectionsItemType The type of the theme collections items in the RecyclerView.
     * @param onClickListener The {@link View.OnClickListener} for each theme collection item.
     * @param imageFetcher The {@link ImageFetcher} to fetch the images.
     */
    public NtpThemeCollectionsAdapter(
            List<?> items,
            @ThemeCollectionsItemType int themeCollectionsItemType,
            View.@Nullable OnClickListener onClickListener,
            ImageFetcher imageFetcher) {
        mItems = new ArrayList<>(items);
        mThemeCollectionsItemType = themeCollectionsItemType;
        mOnClickListener = onClickListener;
        mImageFetcher = imageFetcher;
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

        ImageView imageView = view.findViewById(R.id.theme_collection_image);
        ConstraintLayout.LayoutParams lp =
                (ConstraintLayout.LayoutParams) imageView.getLayoutParams();

        if (viewType == THEME_COLLECTIONS_ITEM) {
            lp.height =
                    parent.getContext()
                            .getResources()
                            .getDimensionPixelSize(
                                    R.dimen.ntp_customization_theme_collections_list_item_height);
            lp.dimensionRatio = null;
        } else if (viewType == SINGLE_THEME_COLLECTION_ITEM) {
            lp.height = ConstraintLayout.LayoutParams.MATCH_CONSTRAINT;
            lp.dimensionRatio = "1:1";
        }
        imageView.setLayoutParams(lp);

        return new ThemeCollectionViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull RecyclerView.ViewHolder holder, int position) {
        ThemeCollectionViewHolder viewHolder = (ThemeCollectionViewHolder) holder;

        switch (holder.getItemViewType()) {
            case THEME_COLLECTIONS_ITEM:
                BackgroundCollection collectionItem = (BackgroundCollection) mItems.get(position);
                viewHolder.itemView.setActivated(
                        collectionItem.id.equals(mSelectedThemeCollectionId));
                viewHolder.mTitle.setText(collectionItem.label);
                fetchImageWithPlaceholder(viewHolder, collectionItem.previewImageUrl);
                break;

            case SINGLE_THEME_COLLECTION_ITEM:
                CollectionImage imageItem = (CollectionImage) mItems.get(position);
                viewHolder.itemView.setActivated(
                        imageItem.collectionId.equals(mSelectedThemeCollectionId)
                                && imageItem.imageUrl.equals(mSelectedThemeCollectionImageUrl));
                viewHolder.mTitle.setVisibility(View.GONE);
                fetchImageWithPlaceholder(viewHolder, imageItem.previewImageUrl);
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

    /**
     * Updates the currently selected theme collections item and refreshes the views.
     *
     * @param collectionId The ID of the selected theme collection.
     * @param imageUrl The URL of the selected theme collection image.
     */
    @SuppressWarnings("notifyDataSetChanged")
    public void setSelection(@Nullable String collectionId, @Nullable GURL imageUrl) {
        mSelectedThemeCollectionId = collectionId;
        mSelectedThemeCollectionImageUrl = imageUrl;
        // TODO(https://crbug.com/440584354): Make NtpThemeCollectionsAdapter follow the MVC design
        // patten and try to forbid notifyDataSetChanged.
        notifyDataSetChanged();
    }

    /**
     * Asynchronously fetches an image from a URL and sets it on an ImageView. Handles view
     * recycling by tagging the view with the URL and clearing any previous image.
     *
     * @param viewHolder The ViewHolder containing the ImageView.
     * @param imageUrl The URL of the image to fetch.
     */
    private void fetchImageWithPlaceholder(ThemeCollectionViewHolder viewHolder, GURL imageUrl) {
        // Set a tag on the ImageView to the URL of the image we're about to load. This helps us
        // check if the view has been recycled for another item by the time the image has finished
        // loading.
        viewHolder.mImage.setTag(imageUrl);
        // Clear the previous image to avoid showing stale images in recycled views.
        viewHolder.mImage.setImageDrawable(null);

        NtpCustomizationUtils.fetchThemeCollectionImage(
                mImageFetcher,
                imageUrl,
                (bitmap) -> {
                    // Before setting the bitmap, check if the ImageView is still
                    // supposed to display this image.
                    if (imageUrl.equals(viewHolder.mImage.getTag()) && bitmap != null) {
                        viewHolder.mImage.setImageBitmap(bitmap);
                    }
                });
    }

    /** ViewHolder for items that include an image and an optional title. */
    public static class ThemeCollectionViewHolder extends RecyclerView.ViewHolder {
        final View mView;
        ImageView mImage;
        final TextView mTitle;

        public ThemeCollectionViewHolder(@NonNull View itemView) {
            super(itemView);
            mView = itemView;
            mImage = itemView.findViewById(R.id.theme_collection_image);
            mTitle = itemView.findViewById(R.id.theme_collection_title);
        }
    }
}

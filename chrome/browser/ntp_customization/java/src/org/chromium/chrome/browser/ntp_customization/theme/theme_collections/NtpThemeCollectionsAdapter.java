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
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
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
    private int mSelectedPosition;
    private @Nullable String mHandlingClickCollectionId;
    private @Nullable GURL mHandlingClickImageUrl;
    private int mHandlingClickPosition;

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
        mSelectedPosition = RecyclerView.NO_POSITION;
        mHandlingClickPosition = RecyclerView.NO_POSITION;
    }

    @Override
    public void onAttachedToRecyclerView(RecyclerView recyclerView) {
        super.onAttachedToRecyclerView(recyclerView);
        mRecyclerView = recyclerView;
    }

    @Override
    public int getItemViewType(int position) {
        return mThemeCollectionsItemType;
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
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
    public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {
        ThemeCollectionViewHolder viewHolder = (ThemeCollectionViewHolder) holder;
        Object item = mItems.get(position);

        viewHolder.bind(
                item,
                mThemeCollectionsItemType,
                mSelectedThemeCollectionId,
                mSelectedThemeCollectionImageUrl,
                mImageFetcher,
                createOnClickListener(holder, item),
                isItemHandlingClick(item));
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

        // Re-calculate mSelectedPosition based on the new list and the current selection ID/URL
        mSelectedPosition = findIndex(mSelectedThemeCollectionId, mSelectedThemeCollectionImageUrl);
        mHandlingClickPosition = findIndex(mHandlingClickCollectionId, mHandlingClickImageUrl);
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

    /** Cancels the loading state of any item that is currently showing a spinner. */
    public void cancelLoadingState() {
        int oldHandlingClickPosition = mHandlingClickPosition;
        mHandlingClickCollectionId = null;
        mHandlingClickImageUrl = null;
        mHandlingClickPosition = RecyclerView.NO_POSITION;
        if (oldHandlingClickPosition != RecyclerView.NO_POSITION) {
            notifyItemChanged(oldHandlingClickPosition);
        }
    }

    /**
     * Updates the currently selected theme collections item and refreshes the views.
     *
     * @param collectionId The ID of the selected theme collection.
     * @param imageUrl The URL of the selected theme collection image.
     */
    public void setSelection(@Nullable String collectionId, @Nullable GURL imageUrl) {
        mSelectedThemeCollectionId = collectionId;
        mSelectedThemeCollectionImageUrl = imageUrl;
        int newSelectedPosition =
                findIndex(mSelectedThemeCollectionId, mSelectedThemeCollectionImageUrl);

        // Checks if the new selection matches the pending loading item.
        boolean isSelectingHandlingItem =
                mHandlingClickCollectionId != null
                        && mHandlingClickCollectionId.equals(mSelectedThemeCollectionId)
                        && (mHandlingClickImageUrl == null
                                || mHandlingClickImageUrl.equals(mSelectedThemeCollectionImageUrl));
        int oldHandlingClickPosition = mHandlingClickPosition;
        if (isSelectingHandlingItem) {
            mHandlingClickCollectionId = null;
            mHandlingClickImageUrl = null;
            mHandlingClickPosition = RecyclerView.NO_POSITION;
        }

        if (mSelectedPosition == newSelectedPosition) {
            if (oldHandlingClickPosition != RecyclerView.NO_POSITION) {
                notifyItemChanged(oldHandlingClickPosition);
            }
            return;
        }

        int oldSelectedPosition = mSelectedPosition;
        mSelectedPosition = newSelectedPosition;

        // Notify the old item to redraw (it's no longer selected)
        if (oldSelectedPosition != RecyclerView.NO_POSITION) {
            notifyItemChanged(oldSelectedPosition);
        }

        // Notify the new item to redraw (it's now selected)
        if (newSelectedPosition != RecyclerView.NO_POSITION) {
            notifyItemChanged(newSelectedPosition);
        }

        // If the item that was handling the click (showing the spinner) is different from both the
        // old and new selected items, we need to refresh it to hide the spinner.
        if (oldHandlingClickPosition != RecyclerView.NO_POSITION
                && oldHandlingClickPosition != oldSelectedPosition
                && oldHandlingClickPosition != newSelectedPosition) {
            notifyItemChanged(oldHandlingClickPosition);
        }
    }

    /** Checks if the given item is currently in a loading state (handling a click). */
    private boolean isItemHandlingClick(Object item) {
        if (mThemeCollectionsItemType != SINGLE_THEME_COLLECTION_ITEM) {
            return false;
        }

        CollectionImage imageItem = (CollectionImage) item;
        return imageItem.collectionId.equals(mHandlingClickCollectionId)
                && imageItem.imageUrl.equals(mHandlingClickImageUrl);
    }

    /**
     * Creates an OnClickListener for the item view that handles selection and updates the loading
     * state.
     */
    private View.OnClickListener createOnClickListener(
            RecyclerView.ViewHolder holder, Object item) {
        return v -> {
            if (mThemeCollectionsItemType == SINGLE_THEME_COLLECTION_ITEM
                    && item instanceof CollectionImage) {
                int previousHandlingPosition = mHandlingClickPosition;
                CollectionImage imageItem = (CollectionImage) item;
                mHandlingClickCollectionId = imageItem.collectionId;
                mHandlingClickImageUrl = imageItem.imageUrl;
                mHandlingClickPosition = holder.getBindingAdapterPosition();

                if (previousHandlingPosition != RecyclerView.NO_POSITION
                        && previousHandlingPosition != mHandlingClickPosition) {
                    notifyItemChanged(previousHandlingPosition);
                }
            }
            if (mOnClickListener != null) {
                mOnClickListener.onClick(v);
            }
        };
    }

    private int findIndex(@Nullable String collectionId, @Nullable GURL imageUrl) {
        if (collectionId == null) {
            return RecyclerView.NO_POSITION;
        }

        for (int i = 0; i < mItems.size(); i++) {
            Object item = mItems.get(i);
            if (mThemeCollectionsItemType == THEME_COLLECTIONS_ITEM) {
                BackgroundCollection collectionItem = (BackgroundCollection) item;
                if (collectionItem.id.equals(collectionId)) {
                    return i;
                }
            } else if (mThemeCollectionsItemType == SINGLE_THEME_COLLECTION_ITEM) {
                CollectionImage imageItem = (CollectionImage) item;
                if (imageItem.collectionId.equals(collectionId)
                        && imageItem.imageUrl.equals(imageUrl)) {
                    return i;
                }
            }
        }
        return RecyclerView.NO_POSITION;
    }

    /** ViewHolder for items that include an image and an optional title. */
    @VisibleForTesting
    static class ThemeCollectionViewHolder extends RecyclerView.ViewHolder {
        final View mView;
        final TextView mTitle;
        final ProgressBar mSpinner;
        ImageView mImage;
        // When the click has been handled and is waiting for the reply from the native service.
        boolean mIsHandlingClick;

        public ThemeCollectionViewHolder(View itemView) {
            super(itemView);
            mView = itemView;
            mImage = itemView.findViewById(R.id.theme_collection_image);
            mTitle = itemView.findViewById(R.id.theme_collection_title);
            mSpinner = itemView.findViewById(R.id.spinner);
        }

        public void bind(
                Object item,
                @ThemeCollectionsItemType int itemType,
                @Nullable String selectedCollectionId,
                @Nullable GURL selectedImageUrl,
                ImageFetcher imageFetcher,
                View.@Nullable OnClickListener onClickListener,
                boolean isHandlingClick) {
            switch (itemType) {
                case THEME_COLLECTIONS_ITEM:
                    BackgroundCollection collectionItem = (BackgroundCollection) item;
                    boolean isThemeCollectionSelected =
                            collectionItem.id.equals(selectedCollectionId);
                    itemView.setActivated(isThemeCollectionSelected);
                    // It allows to pronounce "selected" when isSelected is true.
                    itemView.setSelected(isThemeCollectionSelected);
                    mTitle.setText(collectionItem.label);
                    fetchImageWithPlaceholder(imageFetcher, collectionItem.previewImageUrl);
                    mView.setOnClickListener(onClickListener);
                    break;

                case SINGLE_THEME_COLLECTION_ITEM:
                    CollectionImage imageItem = (CollectionImage) item;
                    String contentDescription = String.join(", ", imageItem.attribution);
                    mView.setContentDescription(contentDescription);
                    boolean isSingleThemeCollectionSelected =
                            imageItem.collectionId.equals(selectedCollectionId)
                                    && imageItem.imageUrl.equals(selectedImageUrl);
                    itemView.setActivated(isSingleThemeCollectionSelected);
                    // It allows to pronounce "selected" when isSelected is true.
                    itemView.setSelected(isSingleThemeCollectionSelected);
                    mTitle.setVisibility(View.GONE);
                    fetchImageWithPlaceholder(imageFetcher, imageItem.previewImageUrl);

                    mIsHandlingClick = isHandlingClick;
                    updateLoadingView();

                    View.OnClickListener clickListener =
                            view -> {
                                // If the item view is the current selected item, early exits now.
                                // It is because the click has been handled.
                                if (itemView.isActivated()) return;

                                mIsHandlingClick = true;
                                updateLoadingView();
                                if (onClickListener != null) {
                                    onClickListener.onClick(view);
                                }
                            };
                    mView.setOnClickListener(clickListener);
                    break;

                default:
                    assert false : "Theme collections item type not supported!";
            }
        }

        /**
         * Updates the visual state of the view holder (spinner visibility, image alpha, and
         * clickability) based on whether it is currently handling a click.
         */
        private void updateLoadingView() {
            if (mIsHandlingClick) {
                // When the image is selected by the user and waiting for the results from native
                // service, shows the spinner and reduces opacity of image to highlight the spinner.
                mSpinner.setVisibility(View.VISIBLE);
                mImage.setAlpha(0.5f);
                // Disables the image click so they can't be clicked while loading.
                mView.setClickable(false);
            } else {
                mSpinner.setVisibility(View.GONE);
                mImage.setAlpha(1.0f);
                mView.setClickable(true);
            }
        }

        /**
         * Asynchronously fetches an image from a URL and sets it on an ImageView. Handles view
         * recycling by tagging the view with the URL and clearing any previous image.
         *
         * @param imageFetcher The {@link ImageFetcher} used for fetch theme collection cover image.
         * @param imageUrl The URL of the image to fetch.
         */
        private void fetchImageWithPlaceholder(ImageFetcher imageFetcher, GURL imageUrl) {
            // Set a tag on the ImageView to the URL of the image we're about to load. This helps us
            // check if the view has been recycled for another item by the time the image has
            // finished loading.
            mImage.setTag(imageUrl);
            // Clear the previous image to avoid showing stale images in recycled views.
            mImage.setImageDrawable(null);

            NtpCustomizationUtils.fetchThemeCollectionImage(
                    imageFetcher,
                    imageUrl,
                    (bitmap) -> {
                        // Before setting the bitmap, check if the ImageView is still supposed to
                        // display this image.
                        if (imageUrl.equals(mImage.getTag()) && bitmap != null) {
                            mImage.setImageBitmap(bitmap);
                        }
                    });
        }
    }
}

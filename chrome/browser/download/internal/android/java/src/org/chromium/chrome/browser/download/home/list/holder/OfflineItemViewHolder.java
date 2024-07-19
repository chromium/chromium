// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import static org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils.buildMenuListItem;

import android.graphics.Matrix;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.CallSuper;

import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListProperties;
import org.chromium.chrome.browser.download.home.list.UiUtils;
import org.chromium.chrome.browser.download.home.metrics.UmaUtils;
import org.chromium.chrome.browser.download.home.view.SelectionView;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.async_image.AsyncImageView;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListUtils;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemVisuals;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/** Helper that supports all typical actions for OfflineItems. */
class OfflineItemViewHolder extends ListItemViewHolder implements ListMenuButtonDelegate {
    /** The {@link View} that visually represents the selected state of this list item. */
    protected final SelectionView mSelectionView;

    /** The {@link View} that visually represents the thumbnail of this list item. */
    protected final AsyncImageView mThumbnail;

    private final ListMenuButton mMore;

    // Persisted 'More' button properties.
    private Runnable mShareCallback;
    private Runnable mDeleteCallback;
    private Runnable mRenameCallback;

    // flag to hide rename list menu option for offline pages
    private boolean mCanRename;
    private boolean mCanShare;

    /** Creates a new instance of a {@link OfflineItemViewHolder}. */
    public OfflineItemViewHolder(View view) {
        super(view);
        mSelectionView = itemView.findViewById(R.id.selection);
        mMore = itemView.findViewById(R.id.more);
        mThumbnail = itemView.findViewById(R.id.thumbnail);

        if (mMore != null) mMore.setDelegate(this);
    }

    // ListItemViewHolder implementation.
    @CallSuper
    @Override
    public void bind(PropertyModel properties, ListItem item) {
        OfflineItem offlineItem = ((ListItem.OfflineItemListItem) item).item;
        mCanRename = offlineItem.canRename;
        mCanShare = UiUtils.canShare(offlineItem);

        // Push 'interaction' state
        bindOnClick(properties, item, offlineItem);

        // Push 'More' state.
        bindMenuButtonCallbacks(properties, offlineItem);

        // Push 'selection' state.
        if (shouldPushSelection(properties, item)) {
            mSelectionView.setSelectionState(
                    item.selected,
                    properties.get(ListProperties.SELECTION_MODE_ACTIVE),
                    item.showSelectedAnimation);
        }

        // Push 'thumbnail' state.
        if (mThumbnail != null) {
            if (offlineItem.ignoreVisuals) {
                mThumbnail.setVisibility(View.GONE);
                mThumbnail.setImageDrawable(null);
            } else {
                mThumbnail.setVisibility(View.VISIBLE);
                mThumbnail.setImageResizer(
                        new BitmapResizer(mThumbnail, Filters.fromOfflineItem(offlineItem)));
                mThumbnail.setAsyncImageDrawable(
                        (consumer, width, height) -> {
                            return properties
                                    .get(ListProperties.PROVIDER_VISUALS)
                                    .getVisuals(
                                            offlineItem,
                                            width,
                                            height,
                                            (id, visuals) -> {
                                                consumer.onResult(onThumbnailRetrieved(visuals));
                                            });
                        },
                        offlineItem.id);
            }
        }
    }

    private void bindOnClick(PropertyModel properties, ListItem item, OfflineItem offlineItem) {
        itemView.setOnClickListener(
                v -> {
                    if (mSelectionView != null && mSelectionView.isInSelectionMode()) {
                        properties.get(ListProperties.CALLBACK_SELECTION).onResult(item);
                    } else {
                        properties.get(ListProperties.CALLBACK_OPEN).onResult(offlineItem);
                    }
                });

        itemView.setOnLongClickListener(
                v -> {
                    properties.get(ListProperties.CALLBACK_SELECTION).onResult(item);
                    return true;
                });
    }

    private void bindMenuButtonCallbacks(PropertyModel properties, OfflineItem offlineItem) {
        if (mMore == null) return;

        if (mCanShare) {
            mShareCallback =
                    () -> properties.get(ListProperties.CALLBACK_SHARE).onResult(offlineItem);
        }

        if (mCanRename) {
            mRenameCallback =
                    () -> properties.get(ListProperties.CALLBACK_RENAME).onResult(offlineItem);
        }

        mDeleteCallback =
                () -> properties.get(ListProperties.CALLBACK_REMOVE).onResult(offlineItem);

        mMore.setClickable(!properties.get(ListProperties.SELECTION_MODE_ACTIVE));

        SelectableListUtils.setContentDescriptionContext(
                mMore.getContext(),
                mMore,
                offlineItem.title,
                SelectableListUtils.ContentDescriptionSource.MENU_BUTTON);
    }

    @Override
    public void recycle() {
        // This should cancel any outstanding async request as well as drop any currently visible
        // bitmap.
        mThumbnail.setImageDrawable(null);
    }

    @Override
    public ListMenu getListMenu() {
        ModelList listItems = new ModelList();

        if (mCanShare) listItems.add(buildMenuListItem(R.string.share, 0, 0));
        if (mCanRename) listItems.add(buildMenuListItem(R.string.rename, 0, 0));

        listItems.add(buildMenuListItem(R.string.delete, 0, 0));
        ListMenu.Delegate delegate =
                (model) -> {
                    int textId = model.get(ListMenuItemProperties.TITLE_ID);
                    if (textId == R.string.share) {
                        if (mShareCallback != null) mShareCallback.run();
                    } else if (textId == R.string.delete) {
                        if (mDeleteCallback != null) mDeleteCallback.run();
                    } else if (textId == R.string.rename) {
                        if (mRenameCallback != null) mRenameCallback.run();
                    }
                };
        return BrowserUiListMenuUtils.getBasicListMenu(mMore.getContext(), listItems, delegate);
    }

    /**
     * Called when a {@link OfflineItemVisuals} are retrieved and are used to build the
     * {@link Drawable} to use for the thumbnail {@link View}.  Can be overridden by subclasses who
     * want to do either build a custom {@link Drawable} here (like a circular bitmap).  By default
     * this builds a simple {@link BitmapDrawable} around {@code visuals.icon}.
     *
     * @param visuals The {@link OfflineItemVisuals} from the async request.
     * @return A {@link Drawable} to use for the thumbnail.
     */
    protected Drawable onThumbnailRetrieved(OfflineItemVisuals visuals) {
        if (visuals == null || visuals.icon == null) return null;
        return new BitmapDrawable(itemView.getResources(), visuals.icon);
    }

    private boolean shouldPushSelection(PropertyModel properties, ListItem item) {
        if (mSelectionView == null) return false;

        return mSelectionView.isSelected() != item.selected
                || mSelectionView.isInSelectionMode()
                        != properties.get(ListProperties.SELECTION_MODE_ACTIVE);
    }

    /**
     * A class that sets the correct image matrix on the given {@link ImageView} depending on the
     * size of the bitmap.
     */
    private static class BitmapResizer implements AsyncImageView.ImageResizer {
        private static final float IMAGE_VIEW_MAX_SCALE_FACTOR = 4.f;

        private ImageView mImageView;

        private @Filters.FilterType int mFilter;

        /** Constructor. */
        public BitmapResizer(ImageView imageView, @Filters.FilterType int filter) {
            mImageView = imageView;
            mFilter = filter;
        }

        @Override
        public void maybeResizeImage(Drawable drawable) {
            Matrix matrix = null;

            if (drawable instanceof BitmapDrawable) {
                matrix = upscaleBitmapIfNecessary((BitmapDrawable) drawable);
            }

            mImageView.setImageMatrix(matrix);
            mImageView.setScaleType(
                    matrix == null ? ImageView.ScaleType.CENTER_CROP : ImageView.ScaleType.MATRIX);
        }

        private Matrix upscaleBitmapIfNecessary(BitmapDrawable drawable) {
            if (drawable == null) return null;

            int width = drawable.getBitmap().getWidth();
            int height = drawable.getBitmap().getHeight();

            float scale = computeScaleFactor(width, height);
            if (scale <= 1.f) return null;

            // Compute the required matrix to scale and center the bitmap.
            float dx = (mImageView.getWidth() - width * scale) / 2.f;
            float dy = (mImageView.getHeight() - height * scale) / 2.f;

            Matrix matrix = new Matrix();
            matrix.postScale(scale, scale);
            matrix.postTranslate(dx, dy);
            return matrix;
        }

        /**
         * Computes a scale factor for the bitmap if the bitmap is too small compared to the view
         * dimensions. The scaled bitmap will be centered inside the view. No scaling if the
         * dimensions are comparable.
         */
        private float computeScaleFactor(int width, int height) {
            float widthRatio = (float) mImageView.getWidth() / width;
            float heightRatio = (float) mImageView.getHeight() / height;

            UmaUtils.recordImageViewRequiredStretch(widthRatio, heightRatio, mFilter);
            if (Math.max(widthRatio, heightRatio) < IMAGE_VIEW_MAX_SCALE_FACTOR) return 1.f;

            float minRequiredScale = Math.min(widthRatio, heightRatio);
            return Math.min(minRequiredScale, IMAGE_VIEW_MAX_SCALE_FACTOR);
        }
    }
}

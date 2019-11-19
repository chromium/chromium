// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.UiUtils;
import org.chromium.chrome.download.R;
import org.chromium.components.offline_items_collection.OfflineItemVisuals;
import org.chromium.ui.modelutil.PropertyModel;

/** A {@link RecyclerView.ViewHolder} specifically meant to display a generic {@code OfflineItem}.
 */
public class GenericViewHolder extends OfflineItemViewHolder {
    private final TextView mTitle;
    private final TextView mCaption;

    private @DrawableRes int mGenericIconId;

    /** Creates a new {@link GenericViewHolder} instance. */
    public static GenericViewHolder create(ViewGroup parent) {
        View view = LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.download_manager_generic_item, null);
        return new GenericViewHolder(view);
    }

    private GenericViewHolder(View view) {
        super(view);

        mTitle = itemView.findViewById(R.id.title);
        mCaption = itemView.findViewById(R.id.caption);

        mThumbnail.setForegroundScaleTypeCompat(ImageView.ScaleType.CENTER);
    }

    // OfflineItemViewHolder implementation.
    @Override
    public void bind(PropertyModel properties, ListItem item) {
        super.bind(properties, item);
        ListItem.OfflineItemListItem offlineItem = (ListItem.OfflineItemListItem) item;

        mTitle.setText(offlineItem.item.title);
        mCaption.setText(UiUtils.generateGenericCaption(offlineItem.item));

        // Build invalid icon.
        @DrawableRes
        int iconId = UiUtils.getIconForItem(offlineItem.item);
        if (iconId != mGenericIconId) {
            mGenericIconId = iconId;

            Drawable drawable = org.chromium.ui.UiUtils.getTintedDrawable(
                    itemView.getContext(), iconId, R.color.standard_mode_tint);

            mThumbnail.setUnavailableDrawable(drawable);
            mThumbnail.setWaitingDrawable(drawable);
        }

        mSelectionView.setVisibility(mSelectionView.isSelected() ? View.VISIBLE : View.INVISIBLE);
        mThumbnail.setVisibility(mSelectionView.isSelected() ? View.INVISIBLE : View.VISIBLE);
        updateThumbnailBackground(mThumbnail.getDrawable() != null);
    }

    @Override
    protected Drawable onThumbnailRetrieved(OfflineItemVisuals visuals) {
        boolean hasThumbnail = visuals != null && visuals.icon != null;
        updateThumbnailBackground(hasThumbnail);

        RoundedBitmapDrawable drawable = null;
        if (hasThumbnail) {
            drawable = RoundedBitmapDrawableFactory.create(itemView.getResources(), visuals.icon);
            drawable.setCircular(true);
        }
        return drawable;
    }

    private void updateThumbnailBackground(boolean hasThumbnail) {
        if (hasThumbnail) {
            mThumbnail.setBackground(null);
        } else if (mThumbnail.getBackground() == null) {
            Resources resources = itemView.getResources();
            Drawable background = ApiCompatibilityUtils.getDrawable(
                    resources, R.drawable.list_item_icon_modern_bg);
            background.setLevel(resources.getInteger(R.integer.list_item_level_default));
            mThumbnail.setBackground(background);
        }
    }
}

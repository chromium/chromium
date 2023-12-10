// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.browser_ui.widget.async_image.AutoAnimatorDrawable;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A {@link RecyclerView.ViewHolder} specifically meant to display an in-progress image {@code
 * OfflineItem}.
 */
public class InProgressImageViewHolder extends InProgressViewHolder {
    private final ImageView mPlaceholder;

    /** Creates a new {@link InProgressViewHolder} instance. */
    public static InProgressImageViewHolder create(ViewGroup parent) {
        View view =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.download_manager_in_progress_image_item, null);
        return new InProgressImageViewHolder(view);
    }

    /** Constructor. */
    public InProgressImageViewHolder(View view) {
        super(view, /* constrainCaption= */ true);

        mPlaceholder = view.findViewById(R.id.placeholder);
        mPlaceholder.setImageDrawable(
                AutoAnimatorDrawable.wrap(
                        AppCompatResources.getDrawable(
                                view.getContext(), R.drawable.async_image_view_waiting)));
    }

    // InProgressViewHolder implementation.
    @Override
    public void bind(PropertyModel properties, ListItem item) {
        super.bind(properties, item);
        mPlaceholder.setContentDescription(((ListItem.OfflineItemListItem) item).item.title);
    }
}

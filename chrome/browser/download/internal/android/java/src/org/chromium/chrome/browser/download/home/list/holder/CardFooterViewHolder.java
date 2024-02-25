// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListProperties;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.ui.modelutil.PropertyModel;

/** A {@link RecyclerView.ViewHolder} meant to display a card footer. */
public class CardFooterViewHolder extends ListItemViewHolder {
    /** Creates a new {@link CardFooterViewHolder} instance. */
    public static CardFooterViewHolder create(ViewGroup parent) {
        View view =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.download_manager_card_footer, null);
        return new CardFooterViewHolder(view);
    }

    public CardFooterViewHolder(View view) {
        super(view);
    }

    @Override
    public void bind(PropertyModel properties, ListItem item) {
        ListItem.CardFooterListItem footerListItem = (ListItem.CardFooterListItem) item;
        itemView.setOnClickListener(
                v -> {
                    properties
                            .get(ListProperties.CALLBACK_GROUP_PAGINATION_CLICK)
                            .onResult(footerListItem.dateAndDomain);
                });
    }
}

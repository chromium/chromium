// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.ui.modelutil.PropertyModel;

/** A {@link RecyclerView.ViewHolder} meant to display a divider at the top of a group card. */
public class CardDividerTopViewHolder extends ListItemViewHolder {
    /** Creates a new {@link CardDividerTopViewHolder} instance. */
    public static CardDividerTopViewHolder create(ViewGroup parent) {
        View view =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.download_manager_card_divider_top, null);
        return new CardDividerTopViewHolder(view);
    }

    private CardDividerTopViewHolder(View view) {
        super(view);
    }

    @Override
    public void bind(PropertyModel properties, ListItem item) {}
}

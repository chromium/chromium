// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.SectionHeaderListItem;
import org.chromium.chrome.browser.download.home.list.UiUtils;
import org.chromium.chrome.download.R;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A {@link ViewHolder} specifically meant to display a section header.
 */
public class SectionTitleViewHolder extends ListItemViewHolder {
    private final View mDivider;
    private final TextView mDate;


    /** Create a new {@link SectionTitleViewHolder} instance. */
    public static SectionTitleViewHolder create(ViewGroup parent) {
        View view = LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.download_manager_section_header, null);
        return new SectionTitleViewHolder(view);
    }

    private SectionTitleViewHolder(View view) {
        super(view);
        mDivider = view.findViewById(R.id.divider);
        mDate = (TextView) view.findViewById(R.id.date);
    }

    // ListItemViewHolder implementation.
    @Override
    public void bind(PropertyModel properties, ListItem item) {
        SectionHeaderListItem sectionItem = (SectionHeaderListItem) item;

        mDate.setText(sectionItem.isJustNow ? itemView.getContext().getResources().getString(
                              R.string.download_manager_just_now)
                                            : UiUtils.dateToHeaderString(sectionItem.date));

        mDivider.setVisibility(sectionItem.showDivider ? ViewGroup.VISIBLE : ViewGroup.GONE);
    }
}

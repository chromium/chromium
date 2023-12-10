// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.holder;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.SectionHeaderListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.SectionHeaderType;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.browser_ui.util.date.StringUtils;
import org.chromium.ui.modelutil.PropertyModel;

/** A {@link ViewHolder} specifically meant to display a section header. */
public class SectionTitleViewHolder extends ListItemViewHolder {
    private final TextView mTitle;

    /** Create a new {@link SectionTitleViewHolder} instance. */
    public static SectionTitleViewHolder create(ViewGroup parent) {
        View view =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.download_manager_section_header, null);
        return new SectionTitleViewHolder(view);
    }

    private SectionTitleViewHolder(View view) {
        super(view);
        mTitle = (TextView) view.findViewById(R.id.date);
    }

    // ListItemViewHolder implementation.
    @Override
    public void bind(PropertyModel properties, ListItem item) {
        SectionHeaderListItem sectionItem = (SectionHeaderListItem) item;
        mTitle.setText(getSectionTitle(sectionItem, itemView.getContext()));
    }

    private static CharSequence getSectionTitle(
            SectionHeaderListItem sectionItem, Context context) {
        switch (sectionItem.type) {
            case SectionHeaderType.DATE:
                return StringUtils.dateToHeaderString(sectionItem.date);
            case SectionHeaderType.JUST_NOW:
                return context.getResources().getString(R.string.download_manager_just_now);
        }
        assert false : "Unknown section header type.";
        return null;
    }
}

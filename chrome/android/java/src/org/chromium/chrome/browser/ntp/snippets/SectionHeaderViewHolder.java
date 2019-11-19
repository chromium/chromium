// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.content.res.Resources;
import android.view.LayoutInflater;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.ui.widget.displaystyle.UiConfig;

/**
 * View holder for the header of a section of cards.
 */
public class SectionHeaderViewHolder extends NewTabPageViewHolder {
    private final SectionHeaderView mSectionHeaderView;

    public SectionHeaderViewHolder(final SuggestionsRecyclerView recyclerView, UiConfig config) {
        super(LayoutInflater.from(recyclerView.getContext())
                        .inflate(R.layout.new_tab_page_snippets_expandable_header, recyclerView,
                                false));
        mSectionHeaderView = (SectionHeaderView) itemView;

        Resources resources = recyclerView.getResources();
        int defaultLateralMargin =
                resources.getDimensionPixelSize(R.dimen.content_suggestions_card_modern_margin);
        int wideLateralMargin =
                resources.getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);
    }

    public void onBindViewHolder(SectionHeader header) {
        mSectionHeaderView.setHeader(header);
    }

    @Override
    public void recycle() {
        mSectionHeaderView.setHeader(null);
        super.recycle();
    }

    /**
     * Triggers an update to the icon drawable. Intended to be used as
     * {@link NewTabPageViewHolder.PartialBindCallback}
     */
    static void updateVisuals(NewTabPageViewHolder holder) {
        SectionHeaderView view = ((SectionHeaderViewHolder) holder).mSectionHeaderView;
        if (view != null) view.updateVisuals();
    }
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;

/**
 * View for the header of a section of cards.
 */
public class SectionHeaderView extends LinearLayout implements View.OnClickListener {
    private TextView mTitleView;
    private TextView mStatusView;
    private @Nullable SectionHeader mHeader;

    public SectionHeaderView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTitleView = findViewById(org.chromium.chrome.R.id.header_title);
        mStatusView = findViewById(org.chromium.chrome.R.id.header_status);
    }

    @Override
    public void onClick(View view) {
        assert mHeader.isExpandable() : "onClick() is called on a non-expandable section header.";
        mHeader.toggleHeader();
        SuggestionsMetrics.recordExpandableHeaderTapped(mHeader.isExpanded());
        SuggestionsMetrics.recordArticlesListVisible();
    }

    /** @param header The {@link SectionHeader} that holds the data for this class. */
    public void setHeader(SectionHeader header) {
        mHeader = header;
        if (mHeader == null) return;

        mTitleView.setText(header.getHeaderText());
        mStatusView.setVisibility(header.isExpandable() ? View.VISIBLE : View.GONE);
        updateVisuals();
        setOnClickListener(header.isExpandable() ? this : null);
    }

    /** Update the header view based on whether the header is expanded. */
    public void updateVisuals() {
        if (mHeader == null || !mHeader.isExpandable()) return;

        mStatusView.setText(mHeader.isExpanded() ? R.string.hide : R.string.show);
        setBackgroundResource(
                mHeader.isExpanded() ? 0 : R.drawable.hairline_border_card_background);
    }
}

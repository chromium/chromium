// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.FeedPlaceholderLayout;

/** The coordinator to control the feed placeholder view. */
public class FeedPlaceholderCoordinator {
    @VisibleForTesting
    static final String FEEDS_PLACEHOLDER_SHOWN_TIME_UMA = "FeedsLoadingPlaceholderShown";

    private final Context mContext;
    private final ViewGroup mParentView;
    private FeedPlaceholderLayout mFeedPlaceholderView;

    public FeedPlaceholderCoordinator(
            Context context, ViewGroup parentView, boolean isBackgroundDark) {
        mParentView = parentView;
        mContext = context;
        setUpPlaceholderView();
    }

    private void setUpPlaceholderView() {
        mFeedPlaceholderView =
                (FeedPlaceholderLayout)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.feed_placeholder_layout, null, false);
        // Header blank size should be consistent with
        // R.layout.new_tab_page_snippets_expandable_header_with_menu.
        mFeedPlaceholderView.setBlankHeaderHeight(
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.snippets_article_header_menu_size));
        mParentView.addView(mFeedPlaceholderView);
        MarginLayoutParams lp = (MarginLayoutParams) mFeedPlaceholderView.getLayoutParams();
        int contentPadding =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.content_suggestions_card_modern_padding);
        lp.setMargins(contentPadding, 0, contentPadding, 0);
        mFeedPlaceholderView.requestLayout();
    }

    public void destroy() {
        if (mFeedPlaceholderView != null) {
            assert mParentView != null;
            mParentView.removeView(mFeedPlaceholderView);
            mFeedPlaceholderView = null;
        }
    }

    void onOverviewShownAtLaunch(long activityCreationTimeMs) {
        long layoutInflationCompleteMs = mFeedPlaceholderView.getLayoutInflationCompleteMs();
        assert layoutInflationCompleteMs >= activityCreationTimeMs;

        StartSurfaceConfiguration.recordHistogram(
                FEEDS_PLACEHOLDER_SHOWN_TIME_UMA,
                layoutInflationCompleteMs - activityCreationTimeMs,
                true);
    }
}

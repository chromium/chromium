// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.app.Activity;
import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.start_surface.R;

/** The coordinator to control the feed placeholder view. */
public class FeedPlaceholderCoordinator {
    @VisibleForTesting
    static final String FEEDS_PLACEHOLDER_SHOWN_TIME_UMA = "FeedsLoadingPlaceholderShown";

    private final Context mContext;
    private final ViewGroup mParentView;
    private FeedPlaceholderLayout mFeedPlaceholderView;

    public FeedPlaceholderCoordinator(
            Activity activity, ViewGroup parentView, boolean isBackgroundDark) {
        mParentView = parentView;
        mContext = new ContextThemeWrapper(
                activity, (isBackgroundDark ? R.style.Dark : R.style.Light));
    }

    public void setUpPlaceholderView() {
        mFeedPlaceholderView = (FeedPlaceholderLayout) LayoutInflater.from(mContext).inflate(
                R.layout.feed_placeholder_layout, null, false);
        mParentView.addView(mFeedPlaceholderView);
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

        StartSurfaceConfiguration.recordHistogram(FEEDS_PLACEHOLDER_SHOWN_TIME_UMA,
                layoutInflationCompleteMs - activityCreationTimeMs, true);
    }
}

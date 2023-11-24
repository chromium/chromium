// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.feed.ScrollListener.ScrollState;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerState;

/**
 * Creates a ScrollListener that triggers the IPH for swipe refresh. The listener removes itself
 * from the list of observers when the IPH is determined to be already triggered.
 *
 * Triggering the IPH is based on:
 * 1) The Discover section is expanded.
 * 2) The last feed content fetch is 5 minutes old.
 * 3) The user has scrolled up to the top.
 */
public class RefreshIphScrollListener implements ScrollListener {
    @VisibleForTesting static final long FETCH_TIME_AGE_THREASHOLD_MS = 5 * 60 * 1000; // 5 minutes.

    private final FeedBubbleDelegate mDelegate;
    private final ScrollableContainerDelegate mScrollableContainerDelegate;
    private final Runnable mShowIPHRunnable;

    /** Constructor for IPH triggering. */
    RefreshIphScrollListener(
            FeedBubbleDelegate delegate,
            ScrollableContainerDelegate scrollableContainerDelegate,
            Runnable showIPHRunnable) {
        mDelegate = delegate;
        mScrollableContainerDelegate = scrollableContainerDelegate;
        mShowIPHRunnable = showIPHRunnable;
    }

    @Override
    public void onScrollStateChanged(@ScrollState int state) {}

    @Override
    public void onScrolled(int dx, int dy) {
        if (dy == 0) return;
        maybeTriggerIPH();
    }

    @Override
    public void onHeaderOffsetChanged(int verticalOffset) {
        maybeTriggerIPH();
    }

    private void maybeTriggerIPH() {
        try (TraceEvent e = TraceEvent.scoped("RefreshIphScrollListener.maybeTriggerIPH")) {
            final String featureForIph = FeatureConstants.FEED_SWIPE_REFRESH_FEATURE;
            final Tracker tracker = mDelegate.getFeatureEngagementTracker();

            if (tracker.getTriggerState(featureForIph) == TriggerState.HAS_BEEN_DISPLAYED) {
                mScrollableContainerDelegate.removeScrollListener(this);
                return;
            }

            if (mDelegate.canScrollUp()) return;

            if (!mDelegate.isFeedExpanded()) return;

            long lastFetchTimeMs = mDelegate.getLastFetchTimeMs();
            // If last fetch time is not available, bail out.
            if (lastFetchTimeMs == 0) return;
            if (mDelegate.getCurrentTimeMs() - lastFetchTimeMs < FETCH_TIME_AGE_THREASHOLD_MS) {
                return;
            }

            mShowIPHRunnable.run();
        }
    }
}

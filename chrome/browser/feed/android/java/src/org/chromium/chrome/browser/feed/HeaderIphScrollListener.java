// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.feed.ScrollListener.ScrollState;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerState;

/**
 * Creates a ScrollListener that triggers the menu IPH. The listener removes itself from the
 * list of observers when the IPH is determined to be already triggered.
 *
 * Triggering the IPH is based on (1) the fraction of scroll done on the stream proportionally
 * to its height, (2) the transition fraction of the top search bar, and (3) the position of the
 * menu button in the stream.
 *
 * We want the IPH to be triggered when the section header is properly positioned in the stream
 * which has to meet the following conditions: (1) the IPH popup won't interfere with the search
 * bar at the top of the NTP, (2) the user has scrolled down a bit because they want to look at
 * the feed, and (3) the feed header with its menu button is high enough in the stream to have
 * the feed visible. The goal of conditions (2) and (3) is to show the IPH when the signals are
 * that the user wants to interact with the feed are strong.
 */
public class HeaderIphScrollListener implements ScrollListener {
    private static final float MIN_SCROLL_FRACTION = 0.1f;
    private static final float MAX_HEADER_POS_FRACTION = 0.35f;

    private final FeedBubbleDelegate mDelegate;
    private final ScrollableContainerDelegate mScrollableContainerDelegate;
    private final Runnable mShowIPHRunnable;

    private float mMinScrollFraction;
    private float mHeaderMaxPosFraction;

    HeaderIphScrollListener(
            FeedBubbleDelegate delegate,
            ScrollableContainerDelegate scrollableContainerDelegate,
            Runnable showIPHRunnable) {
        mDelegate = delegate;
        mScrollableContainerDelegate = scrollableContainerDelegate;
        mShowIPHRunnable = showIPHRunnable;

        mMinScrollFraction = MIN_SCROLL_FRACTION;
        mHeaderMaxPosFraction = MAX_HEADER_POS_FRACTION;
    }

    @Override
    public void onScrollStateChanged(@ScrollState int state) {
        if (state != ScrollState.IDLE) return;

        maybeTriggerIPH(mScrollableContainerDelegate.getVerticalScrollOffset());
    }

    @Override
    public void onScrolled(int dx, int dy) {}

    @Override
    public void onHeaderOffsetChanged(int verticalOffset) {
        if (verticalOffset == 0) return;

        // Negate the vertical offset because it is inversely proportional to the scroll offset.
        // For example, a header verical offset of -50px corresponds to a scroll offset of 50px.
        maybeTriggerIPH(-verticalOffset);
    }

    private void maybeTriggerIPH(int verticalScrollOffset) {
        try (TraceEvent e = TraceEvent.scoped("HeaderIphScrollListener.maybeTriggerIPH")) {
            // Get the feature tracker for the IPH and determine whether to show the IPH.
            final String featureForIph = FeatureConstants.FEED_HEADER_MENU_FEATURE;
            final Tracker tracker = mDelegate.getFeatureEngagementTracker();
            // Stop listening to scroll if the IPH was already displayed in the past.
            if (tracker.getTriggerState(featureForIph) == TriggerState.HAS_BEEN_DISPLAYED) {
                mScrollableContainerDelegate.removeScrollListener(this);
                return;
            }

            // Check whether the feed is expanded.
            if (!mDelegate.isFeedExpanded()) return;

            // Check whether the user is signed in.
            if (!mDelegate.isSignedIn()) return;

            // Check that enough scrolling was done proportionally to the stream height.
            if ((float) verticalScrollOffset
                    < (float) mScrollableContainerDelegate.getRootViewHeight()
                            * mMinScrollFraction) {
                return;
            }

            // Check that the feed header is well positioned in the recycler view to show the IPH.
            if (!mDelegate.isFeedHeaderPositionInContainerSuitableForIPH(mHeaderMaxPosFraction)) {
                return;
            }

            mShowIPHRunnable.run();
        }
    }
}

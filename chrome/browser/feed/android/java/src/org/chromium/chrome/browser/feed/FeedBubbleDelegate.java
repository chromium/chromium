// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.components.feature_engagement.Tracker;

/**
 * Delegate to provide necessary checks for bubble (IPH or Back-to-top) triggering in feeds.
 */
public interface FeedBubbleDelegate {
    /**
     * Gets the feature engagement tracker.
     */
    Tracker getFeatureEngagementTracker();

    /**
     * Determines whether the feed is expanded (turned on).
     */
    boolean isFeedExpanded();

    /**
     * Determines whether the user is signed in.
     */
    boolean isSignedIn();

    /**
     * Determines whether the position of the feed header in the NTP container is suitable for
     * showing the IPH.
     */
    boolean isFeedHeaderPositionInContainerSuitableForIPH(float headerMaxPosFraction);

    /**
     * Returns the current time in milliseconds.
     */
    long getCurrentTimeMs();

    /**
     * Returns the time of last feed content fetch in milliseconds.
     */
    long getLastFetchTimeMs();

    /**
     * Returns true if the user can scroll up the page.
     */
    boolean canScrollUp();

    /**
     * Returns true if the "Back to top" bubble is shown.
     */
    boolean isShowingBackToTopBubble();

    /**
     * Returns the number of header items.
     */
    int getHeaderCount();

    /**
     * Returns the number of items, including both headers and feed items.
     */
    int getItemCount();

    /**
     * Returns the position of the first visible position.
     */
    int getFirstVisiblePosition();

    /**
     * Returns the position of the last visible position.
     */
    int getLastVisiblePosition();
}

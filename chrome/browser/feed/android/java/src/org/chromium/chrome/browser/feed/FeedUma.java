// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.base.metrics.RecordHistogram;

/** Records UMA stats for the actions that the user takes on the feed in the NTP. */
public class FeedUma {
    // Possible actions taken by the user to control the feed. These values are also defined in
    // enums.xml as FeedControlsActions.
    // WARNING: these values must stay in sync with enums.xml.
    public static final int CONTROLS_ACTION_CLICKED_MY_ACTIVITY = 0;
    public static final int CONTROLS_ACTION_CLICKED_MANAGE_INTERESTS = 1;
    public static final int CONTROLS_ACTION_CLICKED_LEARN_MORE = 2;
    public static final int CONTROLS_ACTION_TOGGLED_FEED = 3;
    public static final int CONTROLS_ACTION_CLICKED_FEED_HEADER_MENU = 4;
    public static final int DEPRECATED_CONTROLS_ACTION_CLICKED_MANAGE_AUTOPLAY = 5;
    public static final int CONTROLS_ACTION_CLICKED_MANAGE = 6;
    public static final int NUM_CONTROLS_ACTIONS = 7;

    public static void recordFeedControlsAction(int action) {
        assert action >= 0;
        assert action < NUM_CONTROLS_ACTIONS;
        RecordHistogram.recordEnumeratedHistogram(
                "ContentSuggestions.Feed.Controls.Actions", action, NUM_CONTROLS_ACTIONS);
    }

    public static final String[] TOTAL_CARDS_HISTOGRAM_NAMES = {
        "ContentSuggestions.Feed.LoadMoreTrigger.TotalCards",
        "ContentSuggestions.Feed.WebFeed.LoadMoreTrigger.TotalCards",
        "ContentSuggestions.Feed.SingleWebFeed.LoadMoreTrigger.TotalCards",
        "ContentSuggestions.Feed.SupervisedFeed.LoadMoreTrigger.TotalCards",
    };

    public static final String[] OFFSET_FROM_END_OF_STREAM_HISTOGRAM_NAMES = {
        "ContentSuggestions.Feed.LoadMoreTrigger.OffsetFromEndOfStream",
        "ContentSuggestions.Feed.WebFeed.LoadMoreTrigger.OffsetFromEndOfStream",
        "ContentSuggestions.Feed.SingleWebFeed.LoadMoreTrigger.OffsetFromEndOfStream",
        "ContentSuggestions.Feed.SupervisedFeed.LoadMoreTrigger.OffsetFromEndOfStream",
    };

    /**
     * Records the number of remaining cards (for the user to scroll through) at which the feed is
     * triggered to load more content.
     *
     * @param numCardsRemaining the number of cards the user has yet to scroll through.
     */
    public static void recordFeedLoadMoreTrigger(
            int sectionType, int totalCards, int numCardsRemaining) {
        // TODO(crbug.com/40783878): annotate sectionType param with
        // @org.chromium.chrome.browser.feed.StreamKind
        assert totalCards >= 0;
        assert numCardsRemaining >= 0;
        assert OFFSET_FROM_END_OF_STREAM_HISTOGRAM_NAMES.length
                == TOTAL_CARDS_HISTOGRAM_NAMES.length;
        // Subtract 1 from sectionType to account for Unknown.
        sectionType -= 1;
        assert sectionType >= 0 || sectionType <= TOTAL_CARDS_HISTOGRAM_NAMES.length;
        RecordHistogram.recordCount1000Histogram(
                TOTAL_CARDS_HISTOGRAM_NAMES[sectionType], totalCards);
        RecordHistogram.recordCount100Histogram(
                OFFSET_FROM_END_OF_STREAM_HISTOGRAM_NAMES[sectionType], numCardsRemaining);
    }
}

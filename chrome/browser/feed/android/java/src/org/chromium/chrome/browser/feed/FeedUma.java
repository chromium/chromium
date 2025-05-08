// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;

/** Records UMA stats for the actions that the user takes on the feed in the NTP. */
@NullMarked
public class FeedUma {
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

    private static final String HISTOGRAM_ARTICLES_LIST_VISIBLE =
            "NewTabPage.ContentSuggestions.ArticlesListVisible";

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

    /**
     * Records distinct metrics for each click on the section or button of the feed settings bottom
     * sheet in the NTP customization.
     *
     * @param feedUserActionType The section or button got clicked in the Feed bottom sheet.
     */
    public static void recordFeedBottomSheetItemsClicked(
            @FeedUserActionType int feedUserActionType) {
        FeedServiceBridge.reportOtherUserAction(feedUserActionType);

        if (feedUserActionType == FeedUserActionType.TAPPED_TURN_ON) {
            recordArticlesListVisible(/* isArticlesListVisible= */ true);
        } else if (feedUserActionType == FeedUserActionType.TAPPED_TURN_OFF) {
            recordArticlesListVisible(/* isArticlesListVisible= */ false);
        }
    }

    /** Records whether article suggestions are set visible by user. */
    public static void recordArticlesListVisible(boolean isArticlesListVisible) {
        RecordHistogram.recordBooleanHistogram(
                HISTOGRAM_ARTICLES_LIST_VISIBLE, isArticlesListVisible);
    }
}

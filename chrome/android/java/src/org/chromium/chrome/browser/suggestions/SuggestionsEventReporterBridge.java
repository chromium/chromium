// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.cards.ActionItem;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;

/**
 * Exposes methods to report suggestions related events, for UMA or Fetch scheduling purposes.
 */
public class SuggestionsEventReporterBridge implements SuggestionsEventReporter {
    @Override
    public void onSurfaceOpened() {
        nativeOnSurfaceOpened();
    }

    @Override
    public void onPageShown(
            int[] categories, int[] suggestionsPerCategory, boolean[] isCategoryVisible) {
        nativeOnPageShown(categories, suggestionsPerCategory, isCategoryVisible);
    }

    @Override
    public void onSuggestionShown(SnippetArticle suggestion) {
        nativeOnSuggestionShown(suggestion.getGlobalRank(), suggestion.mCategory,
                suggestion.getPerSectionRank(), suggestion.mPublishTimestampMilliseconds,
                suggestion.mScore, suggestion.mFetchTimestampMilliseconds);
    }

    @Override
    public void onSuggestionOpened(SnippetArticle suggestion, int windowOpenDisposition,
            SuggestionsRanker suggestionsRanker) {
        int categoryIndex = suggestionsRanker.getCategoryRank(suggestion.mCategory);
        nativeOnSuggestionOpened(suggestion.getGlobalRank(), suggestion.mCategory, categoryIndex,
                suggestion.getPerSectionRank(), suggestion.mPublishTimestampMilliseconds,
                suggestion.mScore, windowOpenDisposition, suggestion.isPrefetched());
    }

    @Override
    public void onSuggestionMenuOpened(SnippetArticle suggestion) {
        nativeOnSuggestionMenuOpened(suggestion.getGlobalRank(), suggestion.mCategory,
                suggestion.getPerSectionRank(), suggestion.mPublishTimestampMilliseconds,
                suggestion.mScore);
    }
    @Override
    public void onMoreButtonShown(ActionItem actionItem) {
        nativeOnMoreButtonShown(actionItem.getCategory(), actionItem.getPerSectionRank());
    }

    @Override
    public void onMoreButtonClicked(ActionItem actionItem) {
        @CategoryInt
        int category = actionItem.getCategory();
        nativeOnMoreButtonClicked(category, actionItem.getPerSectionRank());
        switch (category) {
            case KnownCategories.BOOKMARKS:
                NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_BOOKMARKS_MANAGER);
                break;
            case KnownCategories.DOWNLOADS:
                NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_DOWNLOADS_MANAGER);
                break;
            default:
                // No action associated
                break;
        }
    }

    public static void onSuggestionTargetVisited(int category, long visitTimeMs) {
        nativeOnSuggestionTargetVisited(category, visitTimeMs);
    }

    public static void onActivityWarmResumed() {
        nativeOnActivityWarmResumed();
    }

    public static void onColdStart() {
        nativeOnColdStart();
    }

    private static native void nativeOnPageShown(
            int[] categories, int[] suggestionsPerCategory, boolean[] isCategoryVisible);
    private static native void nativeOnSuggestionShown(int globalPosition, int category,
            int positionInCategory, long publishTimestampMs, float score, long fetchTimestampMs);
    private static native void nativeOnSuggestionOpened(int globalPosition, int category,
            int categoryIndex, int positionInCategory, long publishTimestampMs, float score,
            int windowOpenDisposition, boolean isPrefetched);
    private static native void nativeOnSuggestionMenuOpened(int globalPosition, int category,
            int positionInCategory, long publishTimestampMs, float score);
    private static native void nativeOnMoreButtonShown(int category, int position);
    private static native void nativeOnMoreButtonClicked(int category, int position);
    private static native void nativeOnSurfaceOpened();

    private static native void nativeOnActivityWarmResumed();
    private static native void nativeOnColdStart();
    private static native void nativeOnSuggestionTargetVisited(int category, long visitTimeMs);
}

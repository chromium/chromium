// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ntp.cards.ActionItem;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;

/**
 * Exposes methods to report suggestions related events, for UMA or Fetch scheduling purposes.
 */
public class SuggestionsEventReporterBridge implements SuggestionsEventReporter {
    @Override
    public void onSurfaceOpened() {
        SuggestionsEventReporterBridgeJni.get().onSurfaceOpened();
    }

    @Override
    public void onPageShown(
            int[] categories, int[] suggestionsPerCategory, boolean[] isCategoryVisible) {
        SuggestionsEventReporterBridgeJni.get().onPageShown(
                categories, suggestionsPerCategory, isCategoryVisible);
    }

    @Override
    public void onSuggestionShown(SnippetArticle suggestion) {
        SuggestionsEventReporterBridgeJni.get().onSuggestionShown(suggestion.getGlobalRank(),
                suggestion.mCategory, suggestion.getPerSectionRank(),
                suggestion.mPublishTimestampMilliseconds, suggestion.mScore,
                suggestion.mFetchTimestampMilliseconds);
    }

    @Override
    public void onSuggestionOpened(SnippetArticle suggestion, int windowOpenDisposition,
            SuggestionsRanker suggestionsRanker) {
        int categoryIndex = suggestionsRanker.getCategoryRank(suggestion.mCategory);
        SuggestionsEventReporterBridgeJni.get().onSuggestionOpened(suggestion.getGlobalRank(),
                suggestion.mCategory, categoryIndex, suggestion.getPerSectionRank(),
                suggestion.mPublishTimestampMilliseconds, suggestion.mScore, windowOpenDisposition,
                suggestion.isPrefetched());
    }

    @Override
    public void onSuggestionMenuOpened(SnippetArticle suggestion) {
        SuggestionsEventReporterBridgeJni.get().onSuggestionMenuOpened(suggestion.getGlobalRank(),
                suggestion.mCategory, suggestion.getPerSectionRank(),
                suggestion.mPublishTimestampMilliseconds, suggestion.mScore);
    }
    @Override
    public void onMoreButtonShown(ActionItem actionItem) {
        SuggestionsEventReporterBridgeJni.get().onMoreButtonShown(
                actionItem.getCategory(), actionItem.getPerSectionRank());
    }

    @Override
    public void onMoreButtonClicked(ActionItem actionItem) {
        @CategoryInt
        int category = actionItem.getCategory();
        SuggestionsEventReporterBridgeJni.get().onMoreButtonClicked(
                category, actionItem.getPerSectionRank());
    }

    public static void onSuggestionTargetVisited(int category, long visitTimeMs) {
        SuggestionsEventReporterBridgeJni.get().onSuggestionTargetVisited(category, visitTimeMs);
    }

    public static void onActivityWarmResumed() {
        SuggestionsEventReporterBridgeJni.get().onActivityWarmResumed();
    }

    public static void onColdStart() {
        SuggestionsEventReporterBridgeJni.get().onColdStart();
    }

    @NativeMethods
    interface Natives {
        void onPageShown(
                int[] categories, int[] suggestionsPerCategory, boolean[] isCategoryVisible);
        void onSuggestionShown(int globalPosition, int category, int positionInCategory,
                long publishTimestampMs, float score, long fetchTimestampMs);
        void onSuggestionOpened(int globalPosition, int category, int categoryIndex,
                int positionInCategory, long publishTimestampMs, float score,
                int windowOpenDisposition, boolean isPrefetched);
        void onSuggestionMenuOpened(int globalPosition, int category, int positionInCategory,
                long publishTimestampMs, float score);
        void onMoreButtonShown(int category, int position);
        void onMoreButtonClicked(int category, int position);
        void onSurfaceOpened();
        void onActivityWarmResumed();
        void onColdStart();
        void onSuggestionTargetVisited(int category, long visitTimeMs);
    }
}

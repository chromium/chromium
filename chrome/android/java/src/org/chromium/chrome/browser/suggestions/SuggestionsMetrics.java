// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.support.v7.widget.RecyclerView;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.cards.ActionItem.State;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.FaviconFetchResult;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSitesBridge;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Exposes methods to report suggestions related events, for UMA or Fetch scheduling purposes.
 */
public abstract class SuggestionsMetrics {
    private SuggestionsMetrics() {}

    // UI Element interactions

    public static void recordSurfaceVisible() {
        if (!SharedPreferencesManager.getInstance().readBoolean(
                    ChromePreferenceKeys.CONTENT_SUGGESTIONS_SHOWN_KEY, false)) {
            RecordUserAction.record("Suggestions.FirstTimeSurfaceVisible");
            SharedPreferencesManager.getInstance().writeBoolean(
                    ChromePreferenceKeys.CONTENT_SUGGESTIONS_SHOWN_KEY, true);
        }

        RecordUserAction.record("Suggestions.SurfaceVisible");
    }

    public static void recordSurfaceHidden() {
        RecordUserAction.record("Suggestions.SurfaceHidden");
    }

    public static void recordTileTapped() {
        RecordUserAction.record("Suggestions.Tile.Tapped");
    }

    public static void recordExpandableHeaderTapped(boolean expanded) {
        if (expanded) {
            RecordUserAction.record("Suggestions.ExpandableHeader.Expanded");
        } else {
            RecordUserAction.record("Suggestions.ExpandableHeader.Collapsed");
        }
    }

    public static void recordCardTapped() {
        RecordUserAction.record("Suggestions.Card.Tapped");
    }

    public static void recordCardActionTapped() {
        RecordUserAction.record("Suggestions.Card.ActionTapped");
    }

    public static void recordCardSwipedAway() {
        RecordUserAction.record("Suggestions.Card.SwipedAway");
    }

    // Effect/Purpose of the interactions. Most are recorded in |content_suggestions_metrics.h|

    /**
     * Records metrics for the visit to the provided content suggestion, such as the time spent on
     * the website, or if the user comes back to the starting point.
     * @param tab The tab we want to record the visit on. It should have a live WebContents.
     * @param suggestion The suggestion that prompted the visit.
     */
    public static void recordVisit(Tab tab, SnippetArticle suggestion) {
        @CategoryInt
        final int category = suggestion.mCategory;
        NavigationRecorder.record(tab, visit -> {
            if (NewTabPage.isNTPUrl(visit.endUrl)) {
                RecordUserAction.record("MobileNTP.Snippets.VisitEndBackInNTP");
            }
            RecordUserAction.record("MobileNTP.Snippets.VisitEnd");
            SuggestionsEventReporterBridge.onSuggestionTargetVisited(category, visit.duration);
        });
    }

    // Histogram recordings

    /**
     * Records whether article suggestions are set visible by user.
     */
    public static void recordArticlesListVisible() {
        RecordHistogram.recordBooleanHistogram("NewTabPage.ContentSuggestions.ArticlesListVisible",
                PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE));
    }

    /**
     * Records the time it took to fetch a favicon for an article.
     *
     * @param fetchTime The time it took to fetch the favicon.
     */
    public static void recordArticleFaviconFetchTime(long fetchTime) {
        RecordHistogram.recordMediumTimesHistogram(
                "NewTabPage.ContentSuggestions.ArticleFaviconFetchTime", fetchTime);
    }

    /**
     * Records the result from a favicon fetch for an article.
     *
     * @param result {@link FaviconFetchResult} The result from the fetch.
     */
    public static void recordArticleFaviconFetchResult(@FaviconFetchResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "NewTabPage.ContentSuggestions.ArticleFaviconFetchResult", result,
                FaviconFetchResult.COUNT);
    }

    /**
     * Records which tiles are available offline once the site suggestions finished loading.
     * @param tileIndex index of a tile whose URL is available offline.
     */
    public static void recordTileOfflineAvailability(int tileIndex) {
        RecordHistogram.recordEnumeratedHistogram("NewTabPage.TileOfflineAvailable", tileIndex,
                MostVisitedSitesBridge.MAX_TILE_COUNT);
    }

    /**
     * @return A {@link SpinnerDurationTracker} to notify to report how long the spinner is visible
     * for.
     */
    public static SpinnerDurationTracker getSpinnerVisibilityReporter() {
        return new SpinnerDurationTracker();
    }

    /**
     * One-shot reporter that records the first time the user scrolls a {@link RecyclerView}. If it
     * should be reused, call {@link #reset()} to rearm it.
     */
    public static class ScrollEventReporter extends RecyclerView.OnScrollListener {
        private boolean mFired;
        @Override
        public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
            if (mFired) return;
            if (newState != RecyclerView.SCROLL_STATE_DRAGGING) return;

            RecordUserAction.record("Suggestions.ScrolledAfterOpen");
            mFired = true;
        }

        public void reset() {
            mFired = false;
        }
    }

    /**
     * Utility class to track the duration of a spinner. Call {@link #startTracking(state)} when a
     * Spinner start, call {@link #endCompleteTracking()} when a loading spinner finishes showing,
     * and call {@link #endIncompleteTracking()} when a spinner is destroyed without completing.
     * These methods are no-ops when called while tracking is not in the expected state.
     */
    public static class SpinnerDurationTracker {
        private long mTrackingStartTimeMs;
        private @State int mSpinnerType;

        private SpinnerDurationTracker() {
            mTrackingStartTimeMs = 0;
        }

        /**
         * Start tracking of the spinner.
         * @param state The state of the {@link ActionItem}.
         */
        public void startTracking(@State int state) {
            assert state == State.INITIAL_LOADING || state == State.MORE_BUTTON_LOADING;

            if (isTracking()) return;

            if (state == State.INITIAL_LOADING || state == State.MORE_BUTTON_LOADING) {
                mSpinnerType = state;
            }
            mTrackingStartTimeMs = System.currentTimeMillis();
            recordSpinnerShowUMA(state);
        }

        /**
         * Stop tracking of the spinner which is destroyed without completing.
         */
        public void endCompleteTracking() {
            if (!isTracking()) return;
            recordSpinnerTimeUMA("ContentSuggestions.Feed.FetchPendingSpinner.VisibleDuration");
        }

        /**
         * Stop tracking of the spinner which finishes showing.
         */
        public void endIncompleteTracking() {
            if (!isTracking()) return;
            recordSpinnerTimeUMA(
                    "ContentSuggestions.Feed.FetchPendingSpinner.VisibleDurationWithoutCompleting");
        }

        private boolean isTracking() {
            return mTrackingStartTimeMs > 0;
        }

        private void recordSpinnerTimeUMA(String baseName) {
            long duration = System.currentTimeMillis() - mTrackingStartTimeMs;
            RecordHistogram.recordTimesHistogram(baseName, duration);

            if (mSpinnerType == State.INITIAL_LOADING) {
                RecordHistogram.recordTimesHistogram(baseName + ".InitialLoad", duration);
            } else if (mSpinnerType == State.MORE_BUTTON_LOADING) {
                RecordHistogram.recordTimesHistogram(baseName + ".MoreButton", duration);
            }
            mTrackingStartTimeMs = 0;
        }

        private void recordSpinnerShowUMA(@State int state) {
            int feedSpinnerType;

            // Here is convert the to {@link SpinnerType} in /third_party/feed/src/main/java/com/
            // google/android/libraries/feed/host/logging/SpinnerType.java.
            // {@link SpinnerType} cannot be directly used here since feed libraries are not always
            // compiled.
            switch (state) {
                case State.INITIAL_LOADING:
                    feedSpinnerType = /*SpinnerType.INITIAL_LOAD=*/1;
                    break;
                case State.MORE_BUTTON_LOADING:
                    feedSpinnerType = /*SpinnerType.MORE_BUTTON=*/3;
                    break;
                default:
                    // This is not a spinner type, so do not record it.
                    return;
            }

            RecordHistogram.recordEnumeratedHistogram(
                    "ContentSuggestions.Feed.FetchPendingSpinner.Shown", feedSpinnerType,
                    /*SpinnerType.NEXT_VALUE=*/6);
        }
    }
}

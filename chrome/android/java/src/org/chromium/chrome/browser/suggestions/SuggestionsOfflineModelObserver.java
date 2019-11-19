// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.offlinepages.DeletedPageInfo;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;

/**
 * Handles checking the offline state of suggestions and notifications about related changes.
 * @param <T> type of suggestion to handle. Mostly a convenience parameter to avoid casts.
 */
public abstract class SuggestionsOfflineModelObserver<T extends OfflinableSuggestion>
        extends OfflinePageBridge.OfflinePageModelObserver implements DestructionObserver {
    private final OfflinePageBridge mOfflinePageBridge;

    /**
     * Constructor for an offline model observer. It registers itself with the bridge, but the
     * unregistration will have to be done by the caller, either directly or by registering the
     * created observer as {@link DestructionObserver}.
     * @param bridge source of the offline state data.
     */
    public SuggestionsOfflineModelObserver(OfflinePageBridge bridge) {
        mOfflinePageBridge = bridge;
        mOfflinePageBridge.addObserver(this);
    }

    @Override
    public void onDestroy() {
        mOfflinePageBridge.removeObserver(this);
    }

    @Override
    public void offlinePageModelLoaded() {
        updateAllSuggestionsOfflineAvailability(/* reportPrefetchedSuggestionsCount = */ false);
    }

    @Override
    public void offlinePageAdded(OfflinePageItem addedPage) {
        updateAllSuggestionsOfflineAvailability(/* reportPrefetchedSuggestionsCount = */ false);
    }

    @Override
    public void offlinePageDeleted(DeletedPageInfo deletedPage) {
        for (T suggestion : getOfflinableSuggestions()) {
            if (suggestion.requiresExactOfflinePage()) continue;

            Long suggestionOfflineId = suggestion.getOfflinePageOfflineId();
            if (suggestionOfflineId == null) continue;
            if (suggestionOfflineId != deletedPage.getOfflineId()) continue;

            // The old value cannot be simply removed without a request to the
            // model, because there may be an older offline page for the same
            // URL.
            updateSuggestionOfflineAvailability(suggestion, /* prefetchedReporter = */ null);
        }
    }

    /**
     * Update offline information for all offlinable suggestions by querying offline page model.
     * @param reportPrefetchedSuggestionsCount whether to report prefetched suggestions count after
     *         querying the model.
     */
    public void updateAllSuggestionsOfflineAvailability(boolean reportPrefetchedSuggestionsCount) {
        NumberPrefetchedReporter prefetchedReporter = null;
        if (reportPrefetchedSuggestionsCount) {
            int pendingRequestsCount = 0;
            for (T suggestion : getOfflinableSuggestions()) {
                ++pendingRequestsCount;
            }
            prefetchedReporter = new NumberPrefetchedReporter(pendingRequestsCount);
        }
        for (T suggestion : getOfflinableSuggestions()) {
            if (suggestion.requiresExactOfflinePage()) {
                if (prefetchedReporter != null) {
                    prefetchedReporter.requestCompleted(/* prefetched = */ false);
                }
                continue;
            }
            updateSuggestionOfflineAvailability(suggestion, prefetchedReporter);
        }
    }

    /**
     * Update offline information for given offlinable suggestion by querying offline page model.
     * @param suggestion given suggestion for which to update the offline information.
     * @param prefetchedReporter reporter of prefetched suggestions count, null if reporting is
     *         disabled.
     */
    public void updateSuggestionOfflineAvailability(
            final T suggestion, @Nullable final NumberPrefetchedReporter prefetchedReporter) {
        // This method is not applicable to articles for which the exact offline id must specified.
        assert !suggestion.requiresExactOfflinePage();
        if (!mOfflinePageBridge.isOfflinePageModelLoaded()) {
            if (prefetchedReporter != null) {
                prefetchedReporter.requestCompleted(/* prefetched = */ false);
            }
            return;
        }

        // TabId is relevant only for recent tab offline pages, which we do not handle here, so we
        // do not care about tab id.
        mOfflinePageBridge.selectPageForOnlineUrl(
                suggestion.getUrl(), /* tabId = */ 0, new Callback<OfflinePageItem>() {
                    @Override
                    public void onResult(OfflinePageItem item) {
                        if (prefetchedReporter != null) {
                            prefetchedReporter.requestCompleted(isPrefetchedOfflinePage(item));
                        }
                        onSuggestionOfflineIdChanged(suggestion, item);
                    }
                });
    }

    /**
     * Returns whether OfflinePageItem corresponds to a prefetched page.
     * @param item OfflinePageItem to check.
     */
    public static boolean isPrefetchedOfflinePage(@Nullable OfflinePageItem item) {
        return item != null
                && TextUtils.equals(item.getClientId().getNamespace(),
                           OfflinePageBridge.SUGGESTED_ARTICLES_NAMESPACE);
    }

    /**
     * Called when the offline state of a suggestion is retrieved.
     * @param suggestion the suggestion for which the offline state was checked.
     * @param item corresponding offline page.
     */
    public abstract void onSuggestionOfflineIdChanged(T suggestion, OfflinePageItem item);

    /** Handle to the suggestions for which to observe changes. */
    public abstract Iterable<T> getOfflinableSuggestions();

    private static class NumberPrefetchedReporter {
        private int mRemainingRequests;
        private int mPrefetched;

        public NumberPrefetchedReporter(int requests) {
            mRemainingRequests = requests;
        }

        public void requestCompleted(boolean prefetched) {
            mRemainingRequests--;
            if (prefetched) mPrefetched++;
            if (mRemainingRequests == 0) {
                NewTabPageUma.recordPrefetchedArticleSuggestionsCount(mPrefetched);
            }
        }
    }
}

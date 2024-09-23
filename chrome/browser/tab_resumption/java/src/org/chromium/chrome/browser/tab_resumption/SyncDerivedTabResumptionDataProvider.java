// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab_resumption.SyncDerivedSuggestionEntrySource.SourceDataChangedObserver;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.ResultStrength;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.SuggestionsResult;

import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * TabResumptionDataProvider that uses SyncDerivedSuggestionEntrySource data, while fulfilling the
 * following update requirements:
 *
 * <pre>
 * 1. Fast path: Read suggestions that are cached or quickly generated without network fetching
 *    (e.g., only uses Local Tabs) so Magic Stack can show module quickly.
 * 2. Slow path: Read up-to-date suggestions that needs time to fetch. This may not fire if (1) fast
 *    path data is already the most recent.
 * 3. Stability: Slow path data may produced unwanted result in some edge cases:
 *    (a) Arrive in quick succession from surge of updates, e.g., from multiple devices.
 *    (b) Arrive late if no new suggestion data exists when the module loads (so fast path data is
 *        correct), but then new suggestion data appears
 *    These results should be rejected to ensure results stability.
 * 4. Permission change: Handle suggestion (and module) removal if the permission changes.
 * </pre>
 *
 * This class not only provides suggestions, but can also coordinate with the caller to trigger
 * refresh via onDataChanged() -> (caller) -> fetchSuggestions(). Therefore some suggestions
 * filtering decisions are made in onDataChanged().
 */
public class SyncDerivedTabResumptionDataProvider extends TabResumptionDataProvider
        implements SourceDataChangedObserver {

    // Duration after initial suggestion for which non-permission data changes will be ignored, to
    // prevent (3b).
    private static final long TIMELY_THRESHOLD_MS = TimeUnit.SECONDS.toMillis(5);

    private final SyncDerivedSuggestionEntrySource mEntrySource;
    private final Runnable mCleanupCallback;

    // Monotonically increasing result strength.
    private @ResultStrength int mStrength;

    // Timestamp for when TENTATIVE suggestion is sent, to prevent (3b).
    private long mTentativeSuggestionTime;

    /**
     * @param entrySource Non-owned data source instance that may be shared.
     * @param cleanupCallback To be invoked in destroy() for potential cleanup of external data.
     */
    @VisibleForTesting
    public SyncDerivedTabResumptionDataProvider(
            SyncDerivedSuggestionEntrySource entrySource, Runnable cleanupCallback) {
        super();
        mEntrySource = entrySource;
        mCleanupCallback = cleanupCallback;
        mEntrySource.addObserver(this);
        mStrength = ResultStrength.TENTATIVE;
    }

    /** Implements {@link TabResumptionDataProvider} */
    @Override
    public void destroy() {
        mEntrySource.removeObserver(this);
        mCleanupCallback.run();
    }

    /** Implements {@link TabResumptionDataProvider} */
    @Override
    public void fetchSuggestions(Callback<SuggestionsResult> suggestionsCallback) {
        // Function is synchronous; no need to worry about contention with destroy().
        if (!mEntrySource.canUseData()) {
            mStrength = ResultStrength.FORCED_NULL;
        }
        if (mStrength == ResultStrength.FORCED_NULL) {
            suggestionsCallback.onResult(new SuggestionsResult(mStrength, null));
            return;
        }

        if (mStrength == ResultStrength.TENTATIVE && mTentativeSuggestionTime == 0) {
            mTentativeSuggestionTime = TabResumptionModuleUtils.getCurrentTimeMs();
        }
        mEntrySource.getSuggestions(
                (List<SuggestionEntry> suggestions) -> {
                    assert suggestions != null; // Not null, but may be empty.
                    suggestionsCallback.onResult(new SuggestionsResult(mStrength, suggestions));
                });
    }

    /** Implements {@link SyncDerivedSuggestionEntrySource.SourceDataChangedObserver} */
    @Override
    public void onDataChanged(boolean isPermissionUpdate) {
        // Assume permission updates are permission removals: If permission were granted, then it
        // was previously absent, and the module would have been gone to start with.
        if (isPermissionUpdate) {
            mStrength = ResultStrength.FORCED_NULL;
            dispatchStatusChangedCallback();

            // Require this to be the first update (for (3a)), and that it's timely (for (3b)).
        } else if (mStrength == ResultStrength.TENTATIVE
                && TabResumptionModuleUtils.getCurrentTimeMs() - mTentativeSuggestionTime
                        < TIMELY_THRESHOLD_MS) {
            mStrength = ResultStrength.STABLE;
            dispatchStatusChangedCallback();
        }
    }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab_resumption.ForeignSessionTabResumptionDataSource.DataChangedObserver;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.ResultStrength;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.SuggestionsResult;

import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * TabResumptionDataProvider that uses ForeignSessionTabResumptionDataSource data, while fulfilling
 * the following update requirements:
 *
 * <pre>
 * 1. Fast path: Read cached suggestions so Magic Stack can show module quickly.
 * 2. Slow path: Read up-to-date suggestions that needs time to fetch. This may not fire if (1) fast
 *    path data is already the most recent.
 * 3. Stability: Slow path data may produced unwanted result in some edge cases:
 *    (a) Arrive in quick succession from surge of updates, e.g., from multiple devices.
 *    (b) Arrive late if no new Foreign Session data exists when the module loads (so fast path data
 *        is correct), but then new Foreign Session data appears
 *    These results should be rejected to ensure results stability.
 * 4. Permission change: Handle suggestion (and module) removal if the permission changes.
 * </pre>
 *
 * This class not only provides suggestions, but can also coordinate with the caller to trigger
 * refresh via onForeignSessionDataChanged() -> (caller) -> fetchSuggestions(). Therefore some
 * suggestions filtering decisions are made in onForeignSessionDataChanged().
 */
public class ForeignSessionTabResumptionDataProvider extends TabResumptionDataProvider
        implements DataChangedObserver {

    // Duration after initial suggestion for which non-permission data changes will be ignored, to
    // prevent (3b).
    private static final long TIMELY_THRESHOLD_MS = TimeUnit.SECONDS.toMillis(5);

    private final ForeignSessionTabResumptionDataSource mDataSource;
    private final Runnable mCleanupCallback;

    // Monotonically increasing result strength.
    private @ResultStrength int mStrength;

    // Timestamp for when TENTATIVE suggestion is sent, to prevent (3b).
    private long mTentativeSuggestionTime;

    /**
     * @param dataSource Non-owned data source instance that may be shared.
     * @param cleanupCallback To be invoked in destroy() for potential cleanup of external data.
     */
    @VisibleForTesting
    public ForeignSessionTabResumptionDataProvider(
            ForeignSessionTabResumptionDataSource dataSource, Runnable cleanupCallback) {
        super();
        mDataSource = dataSource;
        mCleanupCallback = cleanupCallback;
        mDataSource.addObserver(this);
        mStrength = ResultStrength.TENTATIVE;
    }

    /** Implements {@link TabResumptionDataProvider} */
    @Override
    public void destroy() {
        mDataSource.removeObserver(this);
        mCleanupCallback.run();
    }

    /** Implements {@link TabResumptionDataProvider} */
    @Override
    public void fetchSuggestions(Callback<SuggestionsResult> suggestionsCallback) {
        if (!mDataSource.canUseData()) {
            mStrength = ResultStrength.FORCED_NULL;
        }
        if (mStrength == ResultStrength.FORCED_NULL) {
            suggestionsCallback.onResult(new SuggestionsResult(mStrength, null));
            return;
        }

        if (mStrength == ResultStrength.TENTATIVE) {
            mTentativeSuggestionTime = mDataSource.getCurrentTimeMs();
        }
        List<SuggestionEntry> suggestions = mDataSource.getSuggestions();
        assert suggestions != null; // Not null, but may be empty.
        suggestionsCallback.onResult(new SuggestionsResult(mStrength, suggestions));
    }

    /** Implements {@link ForeignSessionTabResumptionDataSource.DataChangedObserver} */
    @Override
    public void onForeignSessionDataChanged(boolean isPermissionUpdate) {
        // Assume permission updates are permission removals: If permission were granted, then it
        // was previously absent, and the module would have been gone to start with.
        if (isPermissionUpdate) {
            mStrength = ResultStrength.FORCED_NULL;
            dispatchStatusChangedCallback();

            // Require this to be the first update (for (3a)), and that it's timely (for (3b)).
        } else if (mStrength == ResultStrength.TENTATIVE
                && mDataSource.getCurrentTimeMs() - mTentativeSuggestionTime
                        < TIMELY_THRESHOLD_MS) {
            mStrength = ResultStrength.STABLE;
            dispatchStatusChangedCallback();
        }
    }
}

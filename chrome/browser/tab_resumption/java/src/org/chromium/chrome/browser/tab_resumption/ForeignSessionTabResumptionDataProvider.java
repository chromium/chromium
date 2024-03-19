// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab_resumption.ForeignSessionTabResumptionDataSource.DataChangedObserver;

import java.util.ArrayList;
import java.util.List;

/**
 * TabResumptionDataProvider that uses ForeignSessionTabResumptionDataSource data, while supporting
 * the following update requirements:
 *
 * <pre>
 * 1. Fast path: Read cached suggestions so Magic stack can show module quickly.
 * 2. Slow path: Read up-to-date suggestions that needs time to fetch. This may not fire if (1) fast
 *    path data is already the most recent.
 * 3. Stability: Slow path data may (a) arrive late (from post-start updates) or (b) arrive in
 *    quick successions (from frequent updates). Reject this so results are stable.
 * 4. Permission change: Handle suggestion (and module) removal if the permission changes.
 * </pre>
 *
 * The callback passed by fetchSuggestions() is single-use. To refresh data, the caller will need to
 * use setStatusChangedCallback() and call fetchSuggestions() again.
 */
public class ForeignSessionTabResumptionDataProvider extends TabResumptionDataProvider
        implements DataChangedObserver {
    private final ForeignSessionTabResumptionDataSource mDataSource;
    private final Runnable mCleanupCallback;

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
    }

    /** Implements {@link TabResumptionDataProvider} */
    @Override
    public void destroy() {
        mDataSource.removeObserver(this);
        mCleanupCallback.run();
    }

    /** Implements {@link TabResumptionDataProvider} */
    @Override
    public void fetchSuggestions(Callback<List<SuggestionEntry>> suggestionsCallback) {
        if (!mDataSource.canUseData()) {
            suggestionsCallback.onResult(new ArrayList<SuggestionEntry>());
            return;
        }

        List<SuggestionEntry> suggestions = mDataSource.getSuggestions();
        assert suggestions != null;

        // Results may be empty.
        suggestionsCallback.onResult(suggestions);
    }

    /** Implements {@link ForeignSessionTabResumptionDataSource.DataChangedObserver} */
    @Override
    public void onForeignSessionDataChanged(boolean isPermissionUpdate) {
        if (isPermissionUpdate || !mIsStable) {
            dispatchStatusChangedCallback();
        }
    }
}

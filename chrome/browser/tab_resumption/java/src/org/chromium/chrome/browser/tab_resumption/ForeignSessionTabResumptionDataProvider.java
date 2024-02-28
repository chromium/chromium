// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab_resumption.ForeignSessionTabResumptionDataSource.DataChangedObserver;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * TabResumptionDataProvider that uses ForeignSessionTabResumptionDataSource data, while supporting
 * the following update requirements:
 *
 * <pre>
 * 1. Get initial suggestions quickly so Magic stack can decide whether to show or hide the module.
 * 2. Get up-to-date suggestions that needs a more time to fetch, and may be unavailable if the data
 *    (1) is already up to date.
 * 3. Stabilize suggestion data beyond a certain time threshold.
 * 4. Handle suggestion (and module) removal if the permission changes.
 * </pre>
 *
 * The callback passed by fetchSuggestions() is single-use. To refresh data, the caller will need to
 * use setStatusChangedCallback() and call fetchSuggestions() again.
 */
public class ForeignSessionTabResumptionDataProvider extends TabResumptionDataProvider
        implements DataChangedObserver {
    // Duration after initial suggestion for which non-permission data changes will be ignored,
    // thus allowing enforcing data stability.
    private static final long STABLE_THRESHOLD_MS = TimeUnit.SECONDS.toMillis(10);

    private final ForeignSessionTabResumptionDataSource mDataSource;
    private final Runnable mCleanupCallback;

    // State to enforce suggestions stability.
    private long mLastWriteTimeMs;

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
        mLastWriteTimeMs = mDataSource.getCurrentTimeMs();
    }

    /** Implements {@link ForeignSessionTabResumptionDataSource.DataChangedObserver} */
    @Override
    public void onForeignSessionDataChanged(boolean isPermissionUpdate) {
        if (isPermissionUpdate || !shouldEnforceStability()) {
            dispatchStatusChangedCallback();
        }
    }

    private boolean shouldEnforceStability() {
        long currentTimeMs = mDataSource.getCurrentTimeMs();
        return mLastWriteTimeMs != 0 && currentTimeMs - mLastWriteTimeMs > STABLE_THRESHOLD_MS;
    }
}

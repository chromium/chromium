// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.ResultStrength;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.SuggestionsResult;

import java.util.ArrayList;
import java.util.List;

/**
 * TabResumptionDataProvider that encapsulates "sub-providers" LocalTabTabResumptionDataProvider and
 * SyncDerivedTabResumptionDataProvider to create suggestions mixing results from both.
 */
public class MixedTabResumptionDataProvider extends TabResumptionDataProvider {

    /** Container for sub-provider fetchSuggestions() results. */
    private class ResultMixer {
        private @NonNull final Callback<SuggestionsResult> mSuggestionsCallback;
        private @Nullable SuggestionsResult mLocalTabResult;
        private @Nullable SuggestionsResult mSyncDerivedResult;

        ResultMixer(@NonNull Callback<SuggestionsResult> suggestionsCallback) {
            mSuggestionsCallback = suggestionsCallback;
        }

        void onLocalTabResultAvailable(SuggestionsResult localTabResult) {
            if (!mIsAlive) return;

            mLocalTabResult = localTabResult;
            maybeDispatch();
        }

        void onSyncDerivedResultAvailable(SuggestionsResult syncDerivedResult) {
            if (!mIsAlive) return;

            mSyncDerivedResult = syncDerivedResult;
            maybeDispatch();
        }

        private SuggestionsResult computeMixedResult() {
            // Joint strength is the minimum of sub-provider strengths, due to the irreversible
            // nature of TENTATIVE -> STABLE -> FORCED_NULL. The resulting sequence of `strength`
            // also satisfies the monotonic increasing property.
            @ResultStrength
            int strength = Math.min(mLocalTabResult.strength, mSyncDerivedResult.strength);
            int size = mLocalTabResult.size() + mSyncDerivedResult.size();
            if (size == 0) return new SuggestionsResult(strength, null);

            List<SuggestionEntry> suggestions = new ArrayList<SuggestionEntry>(size);
            // Concatenate suggestions, with local suggestion appearing first always.
            if (mLocalTabResult.size() > 0) {
                assert mLocalTabResult.size() == 1;
                suggestions.addAll(mLocalTabResult.suggestions);
            }
            if (mSyncDerivedResult.size() > 0) {
                suggestions.addAll(mSyncDerivedResult.suggestions);
            }
            return new SuggestionsResult(strength, suggestions);
        }

        private void maybeDispatch() {
            if (mLocalTabResult != null && mSyncDerivedResult != null) {
                mSuggestionsCallback.onResult(computeMixedResult());
            }
        }
    }

    private final @Nullable LocalTabTabResumptionDataProvider mLocalTabProvider;
    private final @Nullable SyncDerivedTabResumptionDataProvider mSyncDerivedProvider;
    private final boolean mDisableBlend;
    private boolean mIsAlive;

    /**
     * @param localTabProvider Sub-provider for Local Tab suggestions.
     * @param syncDerivedProvider Sub-provider for Sync Derived suggestions.
     * @param disableBlend Whether to fetch suggestions one sub-provider at a time (Local Tab then
     *     Sync Derived), and serves the first nonempty results encountered.
     */
    public MixedTabResumptionDataProvider(
            @Nullable LocalTabTabResumptionDataProvider localTabProvider,
            @Nullable SyncDerivedTabResumptionDataProvider syncDerivedProvider,
            boolean disableBlend) {
        super();
        mIsAlive = true;
        assert localTabProvider != null || syncDerivedProvider != null;
        mLocalTabProvider = localTabProvider;
        mSyncDerivedProvider = syncDerivedProvider;
        mDisableBlend = disableBlend;
    }

    /** Implements {@link TabResumptionDataProvider} */
    @Override
    public void destroy() {
        if (mSyncDerivedProvider != null) {
            mSyncDerivedProvider.setStatusChangedCallback(null);
            mSyncDerivedProvider.destroy();
        }
        if (mLocalTabProvider != null) {
            mLocalTabProvider.setStatusChangedCallback(null);
            mLocalTabProvider.destroy();
        }
        mIsAlive = false;
    }

    /** Implements {@link TabResumptionDataProvider} */
    @Override
    public void fetchSuggestions(Callback<SuggestionsResult> suggestionsCallback) {
        if (mLocalTabProvider == null) {
            mSyncDerivedProvider.fetchSuggestions(suggestionsCallback);
        } else if (mSyncDerivedProvider == null) {
            mLocalTabProvider.fetchSuggestions(suggestionsCallback);
        } else {
            if (mDisableBlend) {
                mLocalTabProvider.fetchSuggestions(
                        (SuggestionsResult localTabResult) -> {
                            if (localTabResult.size() > 0) {
                                suggestionsCallback.onResult(localTabResult);
                                return;
                            }
                            mSyncDerivedProvider.fetchSuggestions(suggestionsCallback);
                        });
            } else {
                ResultMixer mixer = new ResultMixer(suggestionsCallback);
                mLocalTabProvider.fetchSuggestions(mixer::onLocalTabResultAvailable);
                mSyncDerivedProvider.fetchSuggestions(mixer::onSyncDerivedResultAvailable);
            }
        }
    }

    /** Implements {@link TabResumptionDataProvider} */
    @Override
    public void setStatusChangedCallback(@Nullable Runnable statusChangedCallback) {
        // No need to call super.setStatusChangedCallback().
        if (mSyncDerivedProvider != null) {
            mSyncDerivedProvider.setStatusChangedCallback(statusChangedCallback);
        }
        if (mLocalTabProvider != null) {
            mLocalTabProvider.setStatusChangedCallback(statusChangedCallback);
        }
    }
}

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
 * ForeignSessionTabResumptionDataProvider to create suggestions mixing results from both.
 */
public class MixedTabResumptionDataProvider extends TabResumptionDataProvider {

    /** Container for sub-provider fetchSuggestions() results. */
    private class ResultMixer {
        private @NonNull final Callback<SuggestionsResult> mSuggestionsCallback;
        private @Nullable SuggestionsResult mLocalTab;
        private @Nullable SuggestionsResult mForeignSession;

        ResultMixer(@NonNull Callback<SuggestionsResult> suggestionsCallback) {
            mSuggestionsCallback = suggestionsCallback;
        }

        void onLocalTabResults(SuggestionsResult localTab) {
            if (!mIsAlive) return;

            mLocalTab = localTab;
            maybeDispatch();
        }

        void onForeignSessionResults(SuggestionsResult foreignSession) {
            if (!mIsAlive) return;

            mForeignSession = foreignSession;
            maybeDispatch();
        }

        private SuggestionsResult computeMixedResults() {
            // Joint strength is the minimum of sub-provider strengths, due to the irreversible
            // nature of TENTATIVE -> STABLE -> FORCED_NULL. The resulting sequence of `strength`
            // also satisfies the monotonic increasing property.
            @ResultStrength int strength = Math.min(mLocalTab.strength, mForeignSession.strength);
            int size = mLocalTab.size() + mForeignSession.size();
            if (size == 0) return new SuggestionsResult(strength, null);

            List<SuggestionEntry> suggestions = new ArrayList<SuggestionEntry>(size);
            // Concatenate suggestions, with local suggestion appearing first always.
            if (mLocalTab.size() > 0) {
                assert mLocalTab.size() == 1;
                suggestions.addAll(mLocalTab.suggestions);
            }
            if (mForeignSession.size() > 0) {
                suggestions.addAll(mForeignSession.suggestions);
            }
            return new SuggestionsResult(strength, suggestions);
        }

        private void maybeDispatch() {
            if (mLocalTab != null && mForeignSession != null) {
                mSuggestionsCallback.onResult(computeMixedResults());
            }
        }
    }

    private final @Nullable LocalTabTabResumptionDataProvider mLocalTabProvider;
    private final @Nullable ForeignSessionTabResumptionDataProvider mForeignSessionProvider;
    private boolean mIsAlive;

    /**
     * @param localTabProvider Sub-provider for Local Tab suggestions.
     * @param foreignSessionProvider Sub-provider for Foreign Session suggestions.
     */
    public MixedTabResumptionDataProvider(
            @Nullable LocalTabTabResumptionDataProvider localTabProvider,
            @Nullable ForeignSessionTabResumptionDataProvider foreignSessionProvider) {
        super();
        mIsAlive = true;
        assert localTabProvider != null || foreignSessionProvider != null;
        mLocalTabProvider = localTabProvider;
        mForeignSessionProvider = foreignSessionProvider;
    }

    /** Implements {@link TabResumptionDataProvider} */
    @Override
    public void destroy() {
        if (mForeignSessionProvider != null) {
            mForeignSessionProvider.setStatusChangedCallback(null);
            mForeignSessionProvider.destroy();
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
            mForeignSessionProvider.fetchSuggestions(suggestionsCallback);
        } else if (mForeignSessionProvider == null) {
            mLocalTabProvider.fetchSuggestions(suggestionsCallback);
        } else {
            ResultMixer mixer = new ResultMixer(suggestionsCallback);
            mLocalTabProvider.fetchSuggestions(mixer::onLocalTabResults);
            mForeignSessionProvider.fetchSuggestions(mixer::onForeignSessionResults);
        }
    }

    /** Implements {@link TabResumptionDataProvider} */
    @Override
    public void setStatusChangedCallback(@Nullable Runnable statusChangedCallback) {
        // No need to call super.setStatusChangedCallback().
        if (mForeignSessionProvider != null) {
            mForeignSessionProvider.setStatusChangedCallback(statusChangedCallback);
        }
        if (mLocalTabProvider != null) {
            mLocalTabProvider.setStatusChangedCallback(statusChangedCallback);
        }
    }
}

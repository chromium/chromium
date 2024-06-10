// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.ResultStrength;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.SuggestionsResult;

import java.util.ArrayList;

/** TabResumptionDataProvider that suggests local tabs -- specifically, the last active tab. */
public class LocalTabTabResumptionDataProvider extends TabResumptionDataProvider {
    private @Nullable Tab mLastActiveTab;

    public LocalTabTabResumptionDataProvider(@Nullable Tab lastActiveTab) {
        super();
        mLastActiveTab = lastActiveTab;
    }

    /** Implements {@link TabResumptionDataProvider} */
    @Override
    public void destroy() {
        if (mLastActiveTab != null) {
            mLastActiveTab = null;
        }
    }

    /** Implements {@link TabResumptionDataProvider} */
    @Override
    public void fetchSuggestions(Callback<SuggestionsResult> suggestionsCallback) {
        // Function is synchronous; no need to worry about contention with destroy().
        if (mLastActiveTab == null) {
            suggestionsCallback.onResult(new SuggestionsResult(ResultStrength.FORCED_NULL, null));
            return;
        }

        ArrayList<SuggestionEntry> suggestions = new ArrayList<SuggestionEntry>();
        suggestions.add(SuggestionEntry.createFromLocalTab(mLastActiveTab));

        suggestionsCallback.onResult(new SuggestionsResult(ResultStrength.STABLE, suggestions));
    }
}

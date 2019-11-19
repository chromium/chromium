// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import org.chromium.base.Callback;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Implements {@link TabSuggestionsFetcher}. Abstracts the details of
 * communicating with all known client-side {@link TabSuggestionProvider}
 */
public final class TabSuggestionsClientFetcher implements TabSuggestionsFetcher {
    private List<TabSuggestionProvider> mClientSuggestionProviders;

    /**
     * Acquires suggestions for which tabs to close based on client side
     * heuristics.
     */
    public TabSuggestionsClientFetcher() {
        mClientSuggestionProviders =
                new ArrayList<>(Arrays.asList(new StaleTabSuggestionProvider()));
    }

    @Override
    public void fetch(TabContext tabContext, Callback<TabSuggestionsFetcherResults> callback) {
        List<TabSuggestion> retList = new ArrayList<>();

        for (TabSuggestionProvider provider : mClientSuggestionProviders) {
            List<TabSuggestion> suggestions = provider.suggest(tabContext);
            if (suggestions != null && !suggestions.isEmpty()) {
                retList.addAll(suggestions);
            }
        }
        callback.onResult(new TabSuggestionsFetcherResults(retList, tabContext));
    }

    @Override
    public boolean isEnabled() {
        return true;
    }
}

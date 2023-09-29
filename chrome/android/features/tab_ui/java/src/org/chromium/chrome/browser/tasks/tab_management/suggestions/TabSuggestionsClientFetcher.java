// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import org.chromium.base.Callback;

import java.util.ArrayList;
import java.util.List;

/**
 * Implements {@link TabSuggestionsFetcher}. Abstracts the details of
 * communicating with all known client-side {@link TabSuggestionProvider}
 */
public final class TabSuggestionsClientFetcher implements TabSuggestionsFetcher {

    /**
     * Acquires suggestions for which tabs to close based on client side
     * heuristics.
     */
    public TabSuggestionsClientFetcher() {}

    @Override
    public void fetch(TabContext tabContext, Callback<TabSuggestionsFetcherResults> callback) {
        List<TabSuggestion> retList = new ArrayList<>();

        callback.onResult(new TabSuggestionsFetcherResults(retList, tabContext));
    }

    @Override
    public boolean isEnabled() {
        return true;
    }
}

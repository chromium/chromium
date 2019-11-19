// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import org.chromium.base.Callback;

/**
 * Defines the interface for suggestion fetchers.
 */
public interface TabSuggestionsFetcher {
    /**
     * Acquires suggestions for closing tabs based on client side heuristics
     * and returns the result in a callback
     * @param tabContext snapshot of current tab and tab groups
     * @param callback   callback the results are returned in
     */
    void fetch(TabContext tabContext, Callback<TabSuggestionsFetcherResults> callback);

    /**
     * Returns true if the Fetcher is enabled.
     */
    boolean isEnabled();
}

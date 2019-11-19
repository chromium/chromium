// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import java.util.List;

/**
 * Represents results for recommendations regarding whether Tabs should be
 * closed.
 */
public class TabSuggestionsFetcherResults {
    public final List<TabSuggestion> tabSuggestions;
    public final TabContext tabContext;

    /**
     * Results from Tab suggestions fetcher
     * @param tabSuggestions tabs suggested to be closed
     * @param tabContext     snapshot of current tab and tab groups
     */
    TabSuggestionsFetcherResults(List<TabSuggestion> tabSuggestions, TabContext tabContext) {
        this.tabSuggestions = tabSuggestions;
        this.tabContext = tabContext;
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import java.util.List;

/**
 * Observes when new Tab Suggestions become available
 */
public interface TabSuggestionsObserver {
    /**
     * Notify when we have new Tab Suggestions
     * @param tabSuggestions tab suggestions acquired
     */
    // TODO(crbug.com/1023699): Pass back callbacks for dismissed, accepted, and modified
    // suggestion.
    void onNewSuggestion(List<TabSuggestion> tabSuggestions);

    /**
     * Notify when a {@link TabContext} is no longer valid/representative of the user's tabs.
     */
    void onTabSuggestionInvalidated();
}

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import org.chromium.base.Callback;

import java.util.List;

/** Observes when new Tab Suggestions become available */
public interface TabSuggestionsObserver {
    /**
     * Notify when we have new Tab Suggestions
     * @param tabSuggestions tab suggestions acquired
     * @param tabSuggestionFeedback callback for providing feedback on the suggestions
     */
    void onNewSuggestion(
            List<TabSuggestion> tabSuggestions,
            Callback<TabSuggestionFeedback> tabSuggestionFeedback);

    /** Notify when a {@link TabContext} is no longer valid/representative of the user's tabs. */
    void onTabSuggestionInvalidated();
}

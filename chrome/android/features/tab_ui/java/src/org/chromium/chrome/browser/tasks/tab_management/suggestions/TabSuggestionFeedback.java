// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import androidx.annotation.IntDef;

import java.util.List;

/**
 * Feedback acquired from the user about a {@link TabSuggestion}
 */
public class TabSuggestionFeedback {
    @IntDef({TabSuggestionFeedback.TabSuggestionResponse.NOT_CONSIDERED,
            TabSuggestionFeedback.TabSuggestionResponse.DISMISSED, TabSuggestionResponse.ACCEPTED})
    public @interface TabSuggestionResponse {
        /** User did not consider tab suggestion at all */
        int NOT_CONSIDERED = 0;
        /** User considered tab suggestion, but dismissed it */
        int DISMISSED = 1;
        /** User considered tab suggestion and accepted it */
        int ACCEPTED = 2;
    }

    public final TabSuggestion tabSuggestion;
    public final @TabSuggestionResponse int tabSuggestionResponse;
    public final List<Integer> selectedTabIds;
    public final int totalTabCount;

    /**
     * @param tabSuggestion tab suggestion feedback is provided for
     * @param tabSuggestionResponse user response - see {@link TabSuggestionResponse}
     * @param selectedTabIds final tab ids selected across all tabs (the user can edit
     * tabs among the suggestion to accept or reject and can add tabs outside the suggestion)
     * @param totalTabCount total number of tabs in the tab model
     */
    public TabSuggestionFeedback(TabSuggestion tabSuggestion,
            @TabSuggestionResponse int tabSuggestionResponse, List<Integer> selectedTabIds,
            int totalTabCount) {
        this.tabSuggestion = tabSuggestion;
        this.tabSuggestionResponse = tabSuggestionResponse;
        this.selectedTabIds = selectedTabIds;
        this.totalTabCount = totalTabCount;
    }
}

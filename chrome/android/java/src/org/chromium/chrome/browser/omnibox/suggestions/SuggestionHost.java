// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.ui.modelutil.PropertyModel;

/** A mechanism for creating {@link SuggestionViewDelegate}s. */
public interface SuggestionHost {
    /**
     * @param processor The suggestion processor that manages the suggestion.
     * @param model The property model representing the suggestion.
     * @param suggestion The suggestion to create the delegate for.
     * @param position The position of the delegate in the list.
     * @return A delegate for the specified suggestion.
     */
    SuggestionViewDelegate createSuggestionViewDelegate(DropdownItemProcessor processor,
            PropertyModel model, OmniboxSuggestion suggestion, int position);

    /**
     * Triggered when the user selects to refine one of the omnibox suggestions.
     *
     * @param suggestion Suggestion to use to refine Omnibox query.
     */
    void onRefineSuggestion(OmniboxSuggestion suggestion);

    /**
     * Triggered when the user selects switch to tab .
     *
     * @param suggestion Suggestion which sugegstions a URL that is opened in another tab.
     * @param position The position of the button in the list.
     */
    void onSwitchToTab(OmniboxSuggestion suggestion, int position);

    /**
     * Toggle expanded state of suggestion items belonging to specific group.
     *
     * @param groupId ID of Suggestion Group whose visibility changed.
     * @param isCollapsed True if group should appear collapsed, otherwise false.
     */
    void setGroupCollapsedState(int groupId, boolean isCollapsed);
}

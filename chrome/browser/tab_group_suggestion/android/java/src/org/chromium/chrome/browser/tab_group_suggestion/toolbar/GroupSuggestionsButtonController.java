// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion.toolbar;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabwindow.WindowId;

/**
 * Controller that handles the state of the group suggestion toolbar button. it handles availability
 * requests from TabGroupingActionProvider, it handles clicks from
 * GroupSuggestionsButtonDataProvider and it informs the backend of the outcome (button suppressed,
 * ignored, clicked, etc).
 */
public interface GroupSuggestionsButtonController {

    /**
     * Checks if there's a suggestion available for the current tab. Suggestion is cached, as the
     * backend needs to be informed about the outcome of it.
     *
     * @param tab Tab to check.
     * @param windowId Window tab belongs to.
     * @return True if there's a grouping suggestion that applies to the tab, false otherwise.
     */
    boolean shouldShowButton(Tab tab, @WindowId int windowId);

    /**
     * Called when the group suggestions button is shown for a tab. Used to determine if a
     * suggestion was actually shown or if it was suppressed (e.g. by a higher priority button).
     *
     * @param tab Tab on which the button was shown.
     */
    void onButtonShown(Tab tab);

    /**
     * Called when the group suggestion button is going away. Used to inform the backend that a
     * suggestion was ignored.
     */
    void onButtonHidden();

    /**
     * Called when the group suggestion button is clicked, it implements the actual tab grouping
     * operation and it tells the backend that the suggestion was accepted by the user.
     *
     * @param tab Tab on which the button was clicked.
     * @param tabGroupModelFilter Tab group model, used to retrieve tabs and perform the grouping.
     */
    void onButtonClicked(Tab tab, TabGroupModelFilter tabGroupModelFilter);

    /** Destroys any cached suggestion callbacks that need to be deleted. */
    void destroy();
}

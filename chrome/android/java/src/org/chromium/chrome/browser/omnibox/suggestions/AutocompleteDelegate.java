// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionProcessor;
import org.chromium.ui.base.PageTransition;

/**
 * Provides the additional functionality to trigger and interact with autocomplete suggestions.
 */
public interface AutocompleteDelegate extends EditUrlSuggestionProcessor.LocationBarDelegate {
    /**
     * Notified that the URL text has changed.
     */
    void onUrlTextChanged();

    /**
     * Notified that suggestions have changed.
     * @param autocompleteText The inline autocomplete text that can be appended to the
     *                         currently entered user text.
     */
    void onSuggestionsChanged(String autocompleteText);

    /**
     * Notified that the suggestions have been hidden.
     */
    void onSuggestionsHidden();

    /**
     * Requests the keyboard be hidden.
     */
    void hideKeyboard();

    /**
     * Requests that the given URL be loaded in the current tab.
     *
     * @param url The URL to be loaded.
     * @param transition The transition type associated with the url load.
     * @param inputStart The time the input started for the load request.
     */
    void loadUrl(String url, @PageTransition int transition, long inputStart);

    /**
     * @return Whether the omnibox was focused via the NTP fakebox.
     */
    boolean didFocusUrlFromFakebox();

    /**
     * @return Whether the URL currently has focus.
     */
    boolean isUrlBarFocused();
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.chromium.chrome.browser.query_tiles.QueryTileSection.QueryInfo;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;

/**
 * Manages the view interaction with the rest of the system.
 */
public interface NewTabPageManager extends SuggestionsUiDelegate {
    /** @return Whether the location bar is shown in the NTP. */
    boolean isLocationBarShownInNTP();

    /** @return Whether voice search is enabled and the microphone should be shown. */
    boolean isVoiceSearchEnabled();

    /**
     * Animates the search box up into the omnibox and bring up the keyboard.
     * @param beginVoiceSearch Whether to begin a voice search.
     * @param pastedText Text to paste in the omnibox after it's been focused. May be null.
     */
    void focusSearchBox(boolean beginVoiceSearch, String pastedText);

    /**
     * Performs a search query on the current {@link Tab}.
     * @param queryInfo Object containing the query text and additional search params.
     */
    void performSearchQuery(QueryInfo queryInfo);

    /**
     * @return whether the {@link NewTabPage} associated with this manager is the current page
     * displayed to the user.
     */
    boolean isCurrentPage();

    /**
     * Called when the NTP has completely finished loading (all views will be inflated
     * and any dependent resources will have been loaded).
     */
    void onLoadingComplete();
}
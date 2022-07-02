// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import android.text.TextUtils;
import android.view.ViewGroup;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.AutocompleteMatch;

import java.util.List;

/**
 * Utility class that builds a set of {@link SearchResumptionTileView} into a provided
 * {@link ViewGroup}, creating and manipulating the views as needed.
 */
public class SearchResumptionTileBuilder {
    public static final int MAX_TILES_NUMBER = 3;

    private OnSuggestionClickCallback mCallback;

    /**
     *  The callback when a {@link SearchResumptionTileView} is clicked.
     */
    interface OnSuggestionClickCallback {
        void onSuggestionClick(AutocompleteMatch tile);
    }

    public SearchResumptionTileBuilder(OnSuggestionClickCallback callback) {
        mCallback = callback;
    }

    /**
     * Returns Whether the given suggestion is a qualified {@link
     * OmniboxSuggestionType.SEARCH_SUGGEST}.
     */
    static boolean isSearchSuggestion(AutocompleteMatch suggestion) {
        return !TextUtils.isEmpty(suggestion.getDisplayText())
                && suggestion.getType() == OmniboxSuggestionType.SEARCH_SUGGEST;
    }

    /**
     * Iterators the suggestions and chooses the top MAX_TILES_NUMBER ones or less depending on the
     * number of available suggestions to build on the parent ViewGroup.
     */
    void buildSuggestionTile(
            List<AutocompleteMatch> suggestions, SearchResumptionContainerView parent) {
        try (TraceEvent e = TraceEvent.scoped("SearchSuggestionTileProvider.addTileSection")) {
            assert parent.getChildCount() == 0;

            int suggestionCount = suggestions.size();
            int visibleTilesCount = Math.min(suggestionCount, MAX_TILES_NUMBER);
            int tileIndex = 0;
            int suggestionIndex = 0;
            while (tileIndex < visibleTilesCount && suggestionIndex < suggestions.size()) {
                AutocompleteMatch tile = suggestions.get(suggestionIndex);
                if (!isSearchSuggestion(tile)) {
                    suggestionIndex++;
                    continue;
                }
                SearchResumptionTileView tileView = buildTileView(tile, parent);
                parent.addView(tileView);
                tileIndex++;
                suggestionIndex++;
            }

            int childSize = parent.getChildCount();
            for (int i = 0; i < childSize; i++) {
                ((SearchResumptionTileView) parent.getChildAt(i)).mayUpdateBackground(i, childSize);
            }
        }
    }

    /**
     * Builds a {@link SearchResumptionTileView} based on the given suggestion.
     */
    SearchResumptionTileView buildTileView(
            AutocompleteMatch suggestion, SearchResumptionContainerView parent) {
        SearchResumptionTileView tileView = parent.buildTileView();
        tileView.updateSuggestionData(suggestion);
        tileView.addOnSuggestionClickCallback(mCallback);
        return tileView;
    }

    void destroy() {
        mCallback = null;
    }
}

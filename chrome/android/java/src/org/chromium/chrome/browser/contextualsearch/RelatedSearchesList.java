// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.List;

/**
 * A list of Related Searches built from JSON that represents the suggestions that we show in the
 * UI.
 */
class RelatedSearchesList {
    private static final String TAG = "ContextualSearch";

    /** JSON keys sent by the server. */
    private static final String SELECTION_SUGGESTIONS = "selection";

    private static final String TITLE = "title";
    private static final String SEARCH_URL = "searchUrl";

    /** The parsed Json suggestions. */
    private final JSONObject mJsonSuggestions;

    /**
     * Constructs an instance from a JSON string.
     * @param jsonString The JSON string for all the suggestions typically returned by the server,
     *        or an empty or {@code null} string.
     */
    RelatedSearchesList(@Nullable String jsonString) {
        JSONObject suggestions = new JSONObject();
        if (!TextUtils.isEmpty(jsonString)) {
            try {
                suggestions = new JSONObject(jsonString);
            } catch (JSONException e) {
                Log.w(
                        TAG,
                        "RelatedSearchesList cannot parse JSON: "
                                + jsonString
                                + "\n"
                                + e.getMessage());
            }
        }
        mJsonSuggestions = suggestions;
    }

    /**
     * Returns a list of queries. This implementation may change based on whether we're showing
     * suggestions in more than one place or not. This just returns the "default" list with
     * the current interpretation of that concept.
     * @return A {@code List<String>} of search suggestions.
     */
    List<String> getQueries() {
        List<String> results = new ArrayList<String>();
        JSONArray suggestions = getSuggestions();
        if (suggestions == null) return results;
        for (int i = 0; i < suggestions.length(); i++) {
            try {
                results.add(suggestions.getJSONObject(i).getString(TITLE));
            } catch (JSONException e) {
                Log.w(
                        TAG,
                        "RelatedSearchesList cannot find a query with a title at suggestion "
                                + "index: "
                                + i
                                + "\n"
                                + e.getMessage());
            }
        }
        return results;
    }

    /**
     * Returns the URI for the search request for the given suggestion.
     * @param suggestionIndex Which suggestion to get, zero-based from the list sent by the server.
     * @return A URI that can be used to load the SERP in the Panel, or {@code null} in case of an
     *         error.
     */
    @Nullable
    Uri getSearchUri(int suggestionIndex) {
        JSONArray suggestions = getSuggestions();
        if (suggestions == null) return null;
        try {
            String searchUrl = suggestions.getJSONObject(suggestionIndex).getString(SEARCH_URL);
            Uri searchUri = Uri.parse(searchUrl);
            return RelatedSearchesStamp.updateUriForSuggestionPosition(searchUri, suggestionIndex);
        } catch (JSONException e) {
            Log.w(
                    TAG,
                    "RelatedSearchesList cannot find a searchUrl in suggestion "
                            + suggestionIndex
                            + "\n"
                            + e.getMessage());
        }
        return null;
    }

    /**
     * Returns the suggestions array to show in the panel, or {@code null} if none.
     * @return A {@link JSONArray} of suggestions, or {@code null} in case of an error.
     */
    @Nullable
    JSONArray getSuggestions() {
        try {
            return mJsonSuggestions.getJSONArray(SELECTION_SUGGESTIONS);
        } catch (JSONException e) {
            Log.w(TAG, "No suggestions found!\n" + e.getMessage());
            return null;
        }
    }
}

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

/**
 * A list of Related Searches built from JSON that represents the suggestions that we show in the
 * UI.
 */
class RelatedSearchesList {
    private static final String CONTENT_SUGGESTIONS = "content";
    private static final String TITLE = "title";

    private final JSONObject mJsonSuggestions;

    /**
     * Constructs an instance from a JSON string.
     * @param jsonString The JSON string for all the suggestions typically returned by the server.
     */
    RelatedSearchesList(String jsonString) {
        JSONObject suggestions;
        try {
            suggestions = new JSONObject(jsonString);
        } catch (JSONException e) {
            // TODO(donnd): log an error
            suggestions = new JSONObject();
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
        try {
            JSONArray suggestions = mJsonSuggestions.getJSONArray(CONTENT_SUGGESTIONS);
            for (int i = 0; i < suggestions.length(); i++) {
                results.add(suggestions.getJSONObject(i).getString(TITLE));
            }
        } catch (JSONException e) {
            // TODO(donnd): log an error
        }
        return results;
    }
}

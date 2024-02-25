// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import androidx.annotation.Nullable;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;

/** Class for storing scroll state of a feed surface. */
public class FeedScrollState {
    private static final String TAG = "FeedScrollState";

    private static final String SCROLL_POSITION = "pos";
    private static final String SCROLL_LAST_POSITION = "lpos";
    private static final String SCROLL_OFFSET = "off";
    private static final String TAB_ID = "tabId";
    private static final String FEED_CONTENT_STATE = "contentState";

    public int position;
    public int lastPosition;
    public int offset;
    public int tabId;
    // Represents the state of Feed content. If it changes,
    // the scroll state should not be retained.
    public String feedContentState = "";

    /** Turns the fields into json. */
    public String toJson() {
        JSONObject jsonSavedState = new JSONObject();
        try {
            jsonSavedState.put(SCROLL_POSITION, position);
            jsonSavedState.put(SCROLL_LAST_POSITION, lastPosition);
            jsonSavedState.put(SCROLL_OFFSET, offset);
            jsonSavedState.put(TAB_ID, tabId);
            jsonSavedState.put(FEED_CONTENT_STATE, feedContentState);
            return jsonSavedState.toString();
        } catch (JSONException e) {
            Log.d(TAG, "Unable to write to a JSONObject.");
        }
        return "";
    }

    /** Reads from json to recover a FeedScrollState object. */
    static @Nullable FeedScrollState fromJson(String json) {
        if (json == null) return null;
        FeedScrollState result = new FeedScrollState();
        try {
            JSONObject jsonSavedState = new JSONObject(json);
            result.position = jsonSavedState.getInt(SCROLL_POSITION);
            result.lastPosition = jsonSavedState.getInt(SCROLL_LAST_POSITION);
            result.offset = jsonSavedState.getInt(SCROLL_OFFSET);
            result.tabId = jsonSavedState.getInt(TAB_ID);
            result.feedContentState = jsonSavedState.getString(FEED_CONTENT_STATE);
        } catch (JSONException e) {
            Log.d(TAG, "Unable to parse a JSONObject from a string.");
            return null;
        }
        return result;
    }
}

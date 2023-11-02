// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import android.util.Base64;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.net.test.util.WebServer;

import java.io.IOException;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.nio.charset.Charset;
import java.util.ArrayList;

/** A fake suggestions service which returns canned suggestions. */
class TestSuggestionsService {
    private static final String TAG = "TestSuggestSvc";
    private ArrayList<JSONObject> mSuggestions = new ArrayList<JSONObject>();
    /** Add a suggestion to be returned in |handleRequest|. */
    public void addSuggestion(String title, String url, String imageUrl) {
        try {
            JSONObject s = new JSONObject();
            JSONArray ids = new JSONArray();
            ids.put("0xa" + String.valueOf(mSuggestions.size()));
            ids.put("0xb" + String.valueOf(mSuggestions.size()));
            s.put("ids", ids);
            s.put("title", title);
            s.put("fullPageUrl", url);
            s.put("creationTime", "2017-05-31T13:11:00Z");
            s.put("expirationTime", "2017-06-03T13:11:00Z");
            s.put("attribution", "Testing");
            s.put("imageUrl", imageUrl);
            s.put("snippet", "Snippet Text");
            s.put("score", 0.5);
            s.put("contentType", "UNKNOWN");
            mSuggestions.add(s);
        } catch (org.json.JSONException e) {
            Log.wtf(TAG, e.getMessage());
        }
    }
    /** Returns a small PNG image data. */
    public static byte[] getImageData() {
        final String imageBase64 =
                "iVBORw0KGgoAAAANSUhEUgAAAAQAAAAECAYAAACp8Z5+AAAABHNCSVQICAgIfAhkiAAAADRJREFU"
                + "CJlNwTERgDAABLA8h7AaqCwUdcYCmOFYn5UkbSsBWvsU7/GAM7H5u4a07RTrHuADaewQm6Wdp7oA"
                + "AAAASUVORK5CYII=";
        return Base64.decode(imageBase64, Base64.DEFAULT);
    }

    /** Writes an HTTP response containing a PNG image. */
    public void writeImageResponse(OutputStream output) throws IOException {
        WebServer.writeResponse(output, WebServer.STATUS_OK, getImageData());
    }

    /**
     * Handles a request for suggestions. Returns a list of suggestions previously added through
     * |addSuggestion|. Throws an exception if the request is not a valid suggestions request.
     */
    public void handleRequest(WebServer.HTTPRequest request, OutputStream output)
            throws JSONException, UnsupportedEncodingException, IOException {
        String bodyString = new String(request.getBody(), "UTF-8");
        JSONObject json = new JSONObject(bodyString);
        // Throw if these expected fields do not exist.
        json.getString("excludedSuggestionIds");
        json.getString("priority");
        json.getString("userActivenessClass");
        json.getString("uiLanguage");

        JSONArray suggestions = new JSONArray();
        for (JSONObject suggestion : mSuggestions) {
            suggestions.put(suggestion);
        }
        JSONObject articlesCategory = new JSONObject();
        articlesCategory.put("id", 1);
        articlesCategory.put("localizedTitle", "Articles for you");
        articlesCategory.put("suggestions", suggestions);
        JSONArray categories = new JSONArray();
        categories.put(articlesCategory);

        org.json.JSONObject response = new JSONObject();
        response.put("categories", categories);
        WebServer.writeResponse(output, WebServer.STATUS_OK,
                response.toString(2).getBytes(Charset.forName("UTF-8")));
    }
}

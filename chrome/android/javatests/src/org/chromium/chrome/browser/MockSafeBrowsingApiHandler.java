// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.safe_browsing.SafeBrowsingApiHandler;

import java.util.HashMap;
import java.util.Map;

/**
 * SafeBrowsingApiHandler that vends fake responses.
 */
public class MockSafeBrowsingApiHandler implements SafeBrowsingApiHandler {
    private Observer mObserver;
    // Mock time it takes for a lookup request to complete.
    private static final long DEFAULT_CHECK_DELTA_US = 10;
    private static final String SAFE_METADATA = "{}";

    // Global url -> metadataResponse map. In practice there is only one SafeBrowsingApiHandler, but
    // it is cumbersome for tests to reach into the singleton instance directly. So just make this
    // static and modifiable from java tests using a static method.
    private static final Map<String, String> sResponseMap = new HashMap<>();

    @Override
    public boolean init(Observer observer) {
        mObserver = observer;
        return true;
    }

    @Override
    public void startUriLookup(final long callbackId, String uri, int[] threatsOfInterest) {
        final String metadata = getMetadata(uri, threatsOfInterest);
        // clang-format off
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT,
                (Runnable) () -> mObserver.onUrlCheckDone(
                        callbackId, SafeBrowsingResult.SUCCESS, metadata, DEFAULT_CHECK_DELTA_US));
        // clang-format on
    }

    @Override
    public boolean startAllowlistLookup(final String uri, int threatType) {
        return false;
    }

    private String getMetadata(String uri, int[] threatsOfInterest) {
        if (sResponseMap.containsKey(uri)) {
            try {
                // Make sure this is a threat of interest.
                String metaCandidate = sResponseMap.get(uri);
                JSONObject json = new JSONObject(metaCandidate);
                JSONArray matches = json.getJSONArray("matches");
                for (int i = 0; i < matches.length(); i++) {
                    JSONObject match = matches.getJSONObject(i);
                    int threatType = Integer.parseInt(match.getString("threat_type"));
                    for (int threatOfInterest : threatsOfInterest) {
                        if (threatType == threatOfInterest) return metaCandidate;
                    }
                }
            } catch (JSONException e) {
                // Just return SAFE_METADATA if we were passed invalid JSON.
            }
        }
        return SAFE_METADATA;
    }

    /*
     * Adds a mock response to the static map.
     * Should be called before the main activity starts up, to avoid thread-unsafe behavior.
     */
    public static void addMockResponse(String uri, String metadata) {
        sResponseMap.put(uri, metadata);
    }

    /*
     * Clears the mock responses from the static map.
     * Should be called in the test tearDown method.
     */
    public static void clearMockResponses() {
        sResponseMap.clear();
    }
}

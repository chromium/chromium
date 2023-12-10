// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test.util;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.base.test.util.UrlUtils;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.LinkedList;
import java.util.Queue;

/** Describes the set of Actions from a .test file. */
public class CapturedSitesInstructions {
    private static final String TAG = "CapturedSites";

    private static final String EXTERNAL_PREFIX = UrlUtils.getIsolatedTestRoot() + "/";
    final String mPrefix =
            EXTERNAL_PREFIX + "android_webview/tools/captured_sites_tests/test/data/";

    // Stores the actions in order for processing.
    private Queue<Action> mActions;

    public CapturedSitesInstructions(String jsonUrl) throws Throwable {
        mActions = new LinkedList<Action>();
        JSONObject json;
        try {
            String text =
                    new String(
                            Files.readAllBytes(Paths.get(mPrefix + jsonUrl)),
                            StandardCharsets.UTF_8);
            json = new JSONObject(text);
        } catch (Exception e) {
            Log.e(TAG, "Cannot read test file into JSONObject", e);
            throw e;
        }
        completeParsing(json);
    }

    public CapturedSitesInstructions(JSONObject json) throws Throwable {
        mActions = new LinkedList<Action>();
        completeParsing(json);
    }

    // Parses startingUrl and actions.
    private void completeParsing(JSONObject json) throws Throwable {
        try {
            parseStartingUrl(json);
            parseActions(json);
        } catch (Exception e) {
            Log.e(TAG, "Cannot parse test url into JSONObject", e);
            throw e;
        }
    }

    // Parses starting Url and adds startingPageAction to beginning of queue.
    private void parseStartingUrl(JSONObject json) throws Throwable {
        String startingUrl = json.getString("startingURL");
        Action startingAction = Action.createStartingPageAction(startingUrl);
        mActions.offer(startingAction);
    }

    // Parses the actions of the .test file and stores them in a queue.
    private void parseActions(JSONObject test) throws Throwable {
        try {
            JSONArray actionList = test.getJSONArray("actions");
            for (int i = 0; i < actionList.length(); i++) {
                JSONObject action = actionList.getJSONObject(i);
                mActions.offer(createAction(action));
            }
        } catch (JSONException e) {
            Log.e(TAG, "Cannot parse test file with null 'actions' field", e);
            throw e;
        }
    }

    // Helper method which parses individual actions and returns generalizable Actions.
    private Action createAction(JSONObject action) throws Throwable {
        String type = action.getString("type");
        String url;
        String selector;
        String expectedValue;
        boolean force;
        switch (type) {
            case "loadPage":
                url = action.getString("url");
                force = action.has("force") && action.getBoolean("force");
                return Action.createLoadPageAction(url, force);
            case "click":
                selector = action.getString("selector");
                return Action.createClickAction(selector);
            case "autofill":
                selector = action.getString("selector");
                return Action.createAutofillAction(selector);
            case "validateField":
                selector = action.getString("selector");
                expectedValue = action.getString("expectedValue");
                return Action.createValidateFieldAction(selector, expectedValue);
            default:
                throw new IOException("Unknown Action type: " + type);
        }
    }

    public Action getNextAction() {
        return mActions.poll();
    }

    public boolean isEmpty() {
        return mActions.isEmpty();
    }
}

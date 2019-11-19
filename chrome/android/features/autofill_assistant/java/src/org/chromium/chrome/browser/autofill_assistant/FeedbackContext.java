// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.chrome.browser.ChromeActivity;

/**
 * Automatically extracts context information and serializes it in JSON form.
 */
class FeedbackContext extends JSONObject {
    static String buildContextString(
            ChromeActivity activity, String debugContext, int indentSpaces) {
        try {
            return new FeedbackContext(activity, debugContext).toString(indentSpaces);
        } catch (JSONException e) {
            // Note: it is potentially unsafe to return e.getMessage(): the exception message
            // could be wrangled and used as an attack vector when arriving at the JSON parser.
            return "{\"error\": \"Failed to convert feedback context to string.\"}";
        }
    }

    private FeedbackContext(ChromeActivity activity, String debugContext) throws JSONException {
        addActivityInformation(activity);
        addClientContext(debugContext);
    }

    private void addActivityInformation(ChromeActivity activity) throws JSONException {
        put("intent-action", activity.getInitialIntent().getAction());
        put("intent-data", activity.getInitialIntent().getDataString());
    }

    private void addClientContext(String debugContext) throws JSONException {
        // Try to parse the debug context as JSON object. If that fails, just add the string as-is.
        try {
            put("debug-context", new JSONObject(debugContext));
        } catch (JSONException encodingException) {
            put("debug-context", debugContext);
        }
    }
}

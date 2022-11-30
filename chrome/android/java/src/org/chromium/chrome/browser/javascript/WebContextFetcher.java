// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.javascript;

import android.util.JsonReader;
import android.util.JsonToken;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.RenderFrameHost;

import java.io.IOException;
import java.io.StringReader;
import java.util.HashMap;
import java.util.Map;

/**
 * Provides ability to fetch content from the document using Javascript.
 */
public class WebContextFetcher {
    private static final String TAG = "WebContextFetcher";

    /**
     * Wrapper object to encapsulate response.
     */
    public static class WebContextFetchResponse {
        /**
         * The context returned from the page converted from a JSON object
         */
        public Map<String, String> context = new HashMap<>();

        /**
         * A descriptive error message set if web context fetching failed.
         * Currently if this is set 'context' will necessarily be empty.
         */
        @Nullable
        public String error;
    }

    // TODO(benwgold): Delete this temporary stub kept to avoid temporarily breaking compilation.
    public static void fetchContextWithJavascript(String script,
            Callback<Map<String, String>> callback, RenderFrameHost renderFrameHost) {}

    /**
     * A utility method which allows Java code to extract content from the page using
     * a Javascript string. The script should be a self executing function returning
     * a javascript dictionary object. The return value must be flat (so no nested fields)
     * and must only contain string values (no integers or other objects) and will throw
     * an assertion error otherwise.
     * @param script The javascript string.
     * @param callback The callback to execute after executing the JS and converting to a
     *      java map.
     * @param renderFrameHost The frame to execute the JS on.
     *
     */
    public static void fetchContextWithJavascriptUpdated(String script,
            Callback<WebContextFetchResponse> callback, RenderFrameHost renderFrameHost) {
        WebContextFetcherJni.get().fetchContextWithJavascript(script, (jsonString) -> {
            callback.onResult(convertJsonToResponse(jsonString));
        }, renderFrameHost);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static WebContextFetchResponse convertJsonToResponse(String jsonString) {
        WebContextFetchResponse fetchResponse = new WebContextFetchResponse();
        try {
            JsonReader jsonReader = new JsonReader(new StringReader(jsonString));
            // The JSON should be an object and not an array.
            if (jsonReader.peek() != JsonToken.BEGIN_OBJECT) {
                throw new AssertionError("Error reading JSON object value.");
            }
            jsonReader.beginObject();
            while (jsonReader.hasNext()) {
                // The JSON object should be a flat key value map with non-null values.
                // Otherwise fire an assertion error.
                if (jsonReader.peek() != JsonToken.NAME) {
                    throw new AssertionError("Error reading JSON name value.");
                }
                String key = jsonReader.nextName();
                if (jsonReader.peek() != JsonToken.STRING) {
                    throw new AssertionError("Error reading JSON string value.");
                }
                String value = jsonReader.nextString();
                fetchResponse.context.put(key, value);
            }
            jsonReader.endObject();
        } catch (IOException | AssertionError e) {
            Log.e(TAG, "Web context json was malformed: %s", e.getMessage());
            fetchResponse.error = e.getMessage();
        }
        return fetchResponse;
    }

    @NativeMethods
    interface Natives {
        void fetchContextWithJavascript(
                String script, Callback<String> callback, RenderFrameHost renderFrameHost);
    }
}

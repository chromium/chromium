// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test.util;

import android.util.Log;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;

public class AutofillProfile {
    private static final String EXTERNAL_PREFIX = "/storage/emulated/0/chromium_tests_root/";
    private static final String TAG = "AutofillProfile";
    private static final String FULL_PREFIX =
            EXTERNAL_PREFIX + "android_webview/tools/captured_sites_tests/test/data/";
    public Map<String, String> profileMap;

    public AutofillProfile(String profile) throws Exception {
        this(parseInstructions(profile));
    }

    // For testing.
    public AutofillProfile(JSONObject json) throws Exception {
        profileMap = new HashMap<>();
        parseProfile(json);
    }

    // Reads the given .profile file and stores the fields in map.
    private static JSONObject parseInstructions(String path) throws Exception {
        try {
            String out =
                    new String(
                            Files.readAllBytes(Paths.get(FULL_PREFIX + path)),
                            StandardCharsets.UTF_8);
            JSONObject test = new JSONObject(out);
            return test;
        } catch (Exception e) {
            Log.w(TAG, "Cannot parse test url into JSONObject", e);
            throw e;
        }
    }

    // Parses the AutofillProfile of .test into a map.
    private void parseProfile(JSONObject test) throws Exception {
        JSONArray profile;
        try {
            profile = test.getJSONArray("autofillProfile");
        } catch (NullPointerException e) {
            Log.e(TAG, "Cannot parse test file with null 'autofillProfile' field", e);
            throw e;
        }
        if (profile.length() == 0) {
            throw new IOException("AutofillProfile has no content.");
        }
        for (int i = 0; i < profile.length(); i++) {
            try {
                JSONObject attribute = profile.getJSONObject(i);
                String type = attribute.getString("type");
                String value = attribute.getString("value");
                if (profileMap.put(type, value) != null) { // put() returns previous value
                    throw new IOException("Autofill profile has duplicate type " + type);
                }
            } catch (NullPointerException e) {
                Log.w(
                        TAG,
                        "Attribute from autofillProfile discarded"
                                + "due to incomplete type or value");
            }
        }
    }
}

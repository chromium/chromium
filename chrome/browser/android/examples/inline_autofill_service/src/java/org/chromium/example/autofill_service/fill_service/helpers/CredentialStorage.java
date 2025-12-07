// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.example.autofill_service.fill_service.helpers;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Helper class to store and retrieve credentials from SharedPreferences. This class stores
 * credentials as a Set of JSONObjects in SharedPreferences.
 */
@NullMarked
public class CredentialStorage {
    private static final String PREFS_NAME = "autofill_credentials";
    private static final String KEY_CREDENTIALS = "credentials";

    private final SharedPreferences mPrefs;

    /** Represents a single username/password credential. */
    public static class Credential {
        public final String username;
        public final String password;

        public Credential(String username, String password) {
            this.username = username;
            this.password = password;
        }
    }

    public CredentialStorage(Context context) {
        mPrefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
    }

    /**
     * Saves a credential to SharedPreferences.
     *
     * @param username The username to save.
     * @param password The password to save.
     */
    public void saveCredential(String username, String password) {
        Set<String> credentials =
                new HashSet<>(mPrefs.getStringSet(KEY_CREDENTIALS, new HashSet<>()));
        JSONObject credentialJson = new JSONObject();
        try {
            credentialJson.put("username", username);
            credentialJson.put("password", password);
            credentials.add(credentialJson.toString());
            mPrefs.edit().putStringSet(KEY_CREDENTIALS, credentials).apply();
        } catch (JSONException e) {
            // In a real app, you'd want to handle this error more gracefully.
            e.printStackTrace();
        }
    }

    /**
     * @return A list of saved credentials.
     */
    public List<Credential> getCredentials() {
        Set<String> credentialsSet = mPrefs.getStringSet(KEY_CREDENTIALS, new HashSet<>());
        List<Credential> credentialsList = new ArrayList<>();
        for (String credString : credentialsSet) {
            try {
                JSONObject credentialJson = new JSONObject(credString);
                String username = credentialJson.getString("username");
                String password = credentialJson.getString("password");
                credentialsList.add(new Credential(username, password));
            } catch (JSONException e) {
                e.printStackTrace();
            }
        }
        // For demonstration, add some example data if storage is empty.
        if (credentialsList.isEmpty()) {
            return Arrays.asList(
                    new Credential("testuser", "password123"),
                    new Credential("example@gmail.com", "foobar"));
        }
        return credentialsList;
    }
}

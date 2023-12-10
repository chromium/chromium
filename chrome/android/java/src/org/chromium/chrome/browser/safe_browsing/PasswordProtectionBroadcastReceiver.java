// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.Locale;

/**
 * Listens for Android sign-in events. This will most commonly happen when a
 * user is first adding an account to their Android device. When the user
 * eventually sets up their profile in Chrome, the data from this sign-in event
 * will be used to provide their gaia password phishing protection by comparing
 * their keystrokes/pastes in Chrome to the hashed password.
 */
public final class PasswordProtectionBroadcastReceiver extends BroadcastReceiver {
    public static final String EXTRA_ACCOUNT_IDENTIFIER = "Login.accountIdentifier";
    public static final String EXTRA_HASHED_PASSWORD = "Login.hashedPassword";
    public static final String EXTRA_SALT = "Login.salt";
    private static final String TAG = "PPBR";

    @Override
    public void onReceive(final Context context, Intent intent) {
        String accountIdentifier = intent.getStringExtra(EXTRA_ACCOUNT_IDENTIFIER);
        String salt = intent.getStringExtra(EXTRA_SALT);
        long hashedPassword = intent.getLongExtra(EXTRA_HASHED_PASSWORD, 0);
        // This should never happen. However, if it does, we should quit early.
        if (accountIdentifier.isEmpty() || salt.isEmpty() || hashedPassword == 0) {
            Log.w(
                    TAG,
                    String.format(
                            Locale.US,
                            "Invalid extras seen. Account: %s, Salt: %s, HashedPassword: %d",
                            accountIdentifier,
                            salt,
                            hashedPassword));
            return;
        }
        try {
            JSONObject entry =
                    new JSONObject()
                            .put(EXTRA_ACCOUNT_IDENTIFIER, accountIdentifier)
                            .put(EXTRA_SALT, salt)
                            .put(EXTRA_HASHED_PASSWORD, hashedPassword);
            SharedPreferencesManager manager = ChromeSharedPreferences.getInstance();
            String accounts =
                    manager.readString(ChromePreferenceKeys.PASSWORD_PROTECTION_ACCOUNTS, null);
            JSONArray entries;
            if (accounts == null) {
                entries = new JSONArray().put(entry);
            } else {
                // Remove any existing entries for the account before
                // adding it in.
                entries = new JSONArray(accounts);
                int indexToRemove = -1;
                for (int i = 0; i < entries.length(); i++) {
                    if (((JSONObject) entries.get(i))
                            .getString(EXTRA_ACCOUNT_IDENTIFIER)
                            .equals(accountIdentifier)) {
                        indexToRemove = i;
                        break;
                    }
                }
                if (indexToRemove > -1) {
                    entries.remove(indexToRemove);
                }
                entries.put(entry);
            }
            manager.writeString(
                    ChromePreferenceKeys.PASSWORD_PROTECTION_ACCOUNTS, entries.toString());
        } catch (JSONException e) {
            Log.i(TAG, "There was a problem parsing JSON: " + e.toString());
        }
    }
}

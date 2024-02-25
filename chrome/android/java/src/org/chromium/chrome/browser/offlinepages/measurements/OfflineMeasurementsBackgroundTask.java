// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.measurements;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** This class clears the persisted data in prefs from OfflineMeasurementsBackgroundTask. */
public class OfflineMeasurementsBackgroundTask {
    public static void clearPersistedDataFromPrefs() {
        // Clear any data persisted in prefs.
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();

        sharedPreferencesManager.removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_LAST_CHECK_MILLIS);
        sharedPreferencesManager.removeKey(
                ChromePreferenceKeys
                        .OFFLINE_MEASUREMENTS_CURRENT_TASK_MEASUREMENT_INTERVAL_IN_MINUTES);
        sharedPreferencesManager.removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_USER_AGENT_STRING);
        sharedPreferencesManager.removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_URL);
        sharedPreferencesManager.removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_TIMEOUT_MS);
        sharedPreferencesManager.removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_METHOD);
        sharedPreferencesManager.removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_SYSTEM_STATE_LIST);
    }
}

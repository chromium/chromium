// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.measurements;

import static org.junit.Assert.assertFalse;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for {@link OfflineMeasurementsBackgroundTask}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class OfflineMeasurementsBackgroundTaskUnitTest {
    @Test
    public void cancelTaskAndclearPersistedDataFromPrefs() {
        // Simulates the task writing data to prefs.
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        sharedPreferencesManager.writeString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_LAST_CHECK_MILLIS, "test data");
        sharedPreferencesManager.writeString(
                ChromePreferenceKeys
                        .OFFLINE_MEASUREMENTS_CURRENT_TASK_MEASUREMENT_INTERVAL_IN_MINUTES,
                "test data");
        sharedPreferencesManager.writeString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_USER_AGENT_STRING, "test data");
        sharedPreferencesManager.writeString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_URL, "test data");
        sharedPreferencesManager.writeString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_TIMEOUT_MS, "test data");
        sharedPreferencesManager.writeString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_METHOD, "test data");
        sharedPreferencesManager.writeString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_SYSTEM_STATE_LIST, "test data");

        // Clears all data stored in prefs.
        OfflineMeasurementsBackgroundTask.clearPersistedDataFromPrefs();

        // Checks that all the prefs have been cleared.
        assertFalse(
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys.OFFLINE_MEASUREMENTS_LAST_CHECK_MILLIS));
        assertFalse(
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys
                                .OFFLINE_MEASUREMENTS_CURRENT_TASK_MEASUREMENT_INTERVAL_IN_MINUTES));
        assertFalse(
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys.OFFLINE_MEASUREMENTS_USER_AGENT_STRING));
        assertFalse(
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_URL));
        assertFalse(
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_TIMEOUT_MS));
        assertFalse(
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_METHOD));
        assertFalse(
                sharedPreferencesManager.contains(
                        ChromePreferenceKeys.OFFLINE_MEASUREMENTS_SYSTEM_STATE_LIST));
    }
}

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Gets and sets preferences associated with cloud management.
 */
@JNINamespace("policy::android")
public class CloudManagementSharedPreferences {
    /**
     * Sets the "Cloud management DM token" preference.
     *
     * @param dmToken The token provided by the DM server when browser registration succeeds.
     */
    @CalledByNative
    public static void saveDmToken(String dmToken) {
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.CLOUD_MANAGEMENT_DM_TOKEN, dmToken);
    }
    /**
     * Deletes the "Cloud management DM token" preference.
     */
    @CalledByNative
    public static void deleteDmToken() {
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.CLOUD_MANAGEMENT_DM_TOKEN);
    }

    /**
     * Returns the value of the "Cloud management DM token" preference, which is non-empty
     * if browser registration succeeded.
     */
    @CalledByNative
    public static String readDmToken() {
        return SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.CLOUD_MANAGEMENT_DM_TOKEN, "");
    }

    /**
     * Sets the "Cloud management client ID" preference.
     *
     * @param clientId The ID generated to represent the current browser installation.
     */
    public static void saveClientId(String clientId) {
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.CLOUD_MANAGEMENT_CLIENT_ID, clientId);
    }

    /**
     * Returns the value of the "Cloud management client ID" preference.
     */
    public static String readClientId() {
        return SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.CLOUD_MANAGEMENT_CLIENT_ID, "");
    }
}

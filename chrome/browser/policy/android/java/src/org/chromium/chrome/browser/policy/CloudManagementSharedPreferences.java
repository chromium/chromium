// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Gets and sets preferences associated with cloud management. */
@JNINamespace("policy::android")
public class CloudManagementSharedPreferences {
    /**
     * Sets the "Cloud management DM token" preference.
     *
     * @param dmToken The token provided by the DM server when browser registration succeeds.
     */
    @CalledByNative
    public static void saveDmToken(String dmToken) {
        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.CLOUD_MANAGEMENT_DM_TOKEN, dmToken);
    }

    /** Deletes the "Cloud management DM token" preference. */
    @CalledByNative
    public static void deleteDmToken() {
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.CLOUD_MANAGEMENT_DM_TOKEN);
    }

    /**
     * Returns the value of the "Cloud management DM token" preference, which is non-empty
     * if browser registration succeeded.
     */
    @CalledByNative
    public static String readDmToken() {
        return ChromeSharedPreferences.getInstance()
                .readString(ChromePreferenceKeys.CLOUD_MANAGEMENT_DM_TOKEN, "");
    }

    /**
     * Sets the "Cloud management client ID" preference.
     *
     * @param clientId The ID generated to represent the current browser installation.
     */
    public static void saveClientId(String clientId) {
        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.CLOUD_MANAGEMENT_CLIENT_ID, clientId);
    }

    /** Returns the value of the "Cloud management client ID" preference. */
    public static String readClientId() {
        return ChromeSharedPreferences.getInstance()
                .readString(ChromePreferenceKeys.CLOUD_MANAGEMENT_CLIENT_ID, "");
    }
}

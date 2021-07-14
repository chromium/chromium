// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.measurements;

import android.util.Base64;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Unit test helper for OfflineMeasurementsPageLoadMetricsObserver.
 */
public class OfflineMeasurementsTestHelper {
    @CalledByNative
    public static void addSystemStateListToPrefs(byte[] encodedSystemStateList) {
        // Write the encoded system state list directly to prefs.
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_SYSTEM_STATE_LIST,
                Base64.encodeToString(encodedSystemStateList, Base64.DEFAULT));
    }
}

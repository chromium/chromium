// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;

/**
 * Helper class for features that can be run in the ServiceManager only mode.
 */
public class ServiceManagerStartupUtils {
    public static final String TASK_TAG = "Servicification Startup Task";

    // Key in the SharedPreferences for storing all features that will start ServiceManager.
    private static final String SERVICE_MANAGER_FEATURES_OLD_KEY = "ServiceManagerFeatures";

    // Remove the {@link SERVICE_MANAGER_FEATURES_OLD_KEY} from the SharedPreferences. The same
    // features are now cached in the FeatureUtilities.
    public static void cleanupSharedPreferences() {
        SharedPreferences sharedPreferences = ContextUtils.getAppSharedPreferences();
        if (sharedPreferences.contains(SERVICE_MANAGER_FEATURES_OLD_KEY)) {
            SharedPreferences.Editor editor = sharedPreferences.edit();
            editor.remove(SERVICE_MANAGER_FEATURES_OLD_KEY);
            editor.apply();
        }
    }
}
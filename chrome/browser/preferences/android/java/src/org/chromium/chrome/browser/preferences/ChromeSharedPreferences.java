// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.shared_preferences.PreferenceKeyRegistry;
import org.chromium.build.BuildConfig;

@JNINamespace("android::shared_preferences")
public class ChromeSharedPreferences {
    // clang-format off
    public static final PreferenceKeyRegistry REGISTRY =
            (BuildConfig.ENABLE_ASSERTS
                     ? new PreferenceKeyRegistry(
                             "chrome",
                             ChromePreferenceKeys.getKeysInUse(),
                             LegacyChromePreferenceKeys.getKeysInUse(),
                             LegacyChromePreferenceKeys.getPrefixesInUse())
                     : null);
    // clang-format on

    /**
     * @return The //base SharedPreferencesManager singleton.
     */
    @CalledByNative
    public static org.chromium.base.shared_preferences.SharedPreferencesManager getInstance() {
        // TODO(crbug.com/1484291): After the migration is complete, remove lambda to let //base
        // SharedPreferencesManager create an instance of itself.
        return org.chromium.base.shared_preferences.SharedPreferencesManager.getInstanceForRegistry(
                REGISTRY, () -> new SharedPreferencesManager(REGISTRY));
    }
}

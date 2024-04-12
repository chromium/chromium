// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.shared_preferences.PreferenceKeyRegistry;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.AssumeNonNull;

@JNINamespace("android::shared_preferences")
public class ChromeSharedPreferences {
    public static final PreferenceKeyRegistry REGISTRY =
            (BuildConfig.ENABLE_ASSERTS
                    ? new PreferenceKeyRegistry(
                            "chrome",
                            ChromePreferenceKeys.getKeysInUse(),
                            LegacyChromePreferenceKeys.getKeysInUse(),
                            LegacyChromePreferenceKeys.getPrefixesInUse())
                    : null);

    /**
     * @return The //base SharedPreferencesManager singleton.
     */
    @AssumeNonNull
    @CalledByNative
    public static SharedPreferencesManager getInstance() {
        return SharedPreferencesManager.getInstanceForRegistry(REGISTRY);
    }
}

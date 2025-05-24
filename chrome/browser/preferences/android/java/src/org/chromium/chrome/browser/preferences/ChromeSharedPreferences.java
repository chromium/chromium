// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.shared_preferences.PreferenceKeyRegistry;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.OptimizeAsNonNull;

@JNINamespace("android::shared_preferences")
@NullMarked
public class ChromeSharedPreferences {
    public static final @Nullable PreferenceKeyRegistry REGISTRY =
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
    @OptimizeAsNonNull
    @CalledByNative
    public static SharedPreferencesManager getInstance() {
        return SharedPreferencesManager.getInstanceForRegistry(REGISTRY);
    }
}

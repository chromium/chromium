// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import org.chromium.base.shared_preferences.PreferenceKeyRegistry;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Collections;

/**
 * Provides access to the {@link SharedPreferencesManager} for multi-instance shared preferences
 * data.
 */
@NullMarked
public class MultiInstanceSharedPreferences {
    public static final @Nullable PreferenceKeyRegistry REGISTRY =
            (BuildConfig.ENABLE_ASSERTS
                    ? new PreferenceKeyRegistry(
                            "MultiInstance",
                            MultiInstancePreferenceKeys.getKeysInUse(),
                            Collections.emptyList(),
                            Collections.emptyList())
                    : null);

    /**
     * Returns The //base SharedPreferencesManager singleton.
     */
    public static SharedPreferencesManager getInstance() {
        return SharedPreferencesManager.getInstanceForRegistry(REGISTRY);
    }
}

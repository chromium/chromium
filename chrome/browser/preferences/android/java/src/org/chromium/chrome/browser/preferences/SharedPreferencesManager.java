// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import org.chromium.base.shared_preferences.PreferenceKeyRegistry;

/**
 * @deprecated Use {@link ChromeSharedPreferences} and
 * {@link org.chromium.base.shared_preferences.SharedPreferencesManager} instead.
 *
 * TODO(crbug.com/1484291): Remove this class once usages have been migrated.
 */
@SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
@Deprecated
public class SharedPreferencesManager
        extends org.chromium.base.shared_preferences.SharedPreferencesManager {
    protected SharedPreferencesManager(PreferenceKeyRegistry registry) {
        super(registry);
    }

    /**
     * @return The //chrome SharedPreferencesManager singleton.
     *
     * @deprecated Use {@link ChromeSharedPreferences} instead.
     *
     * TODO(crbug.com/1484291): This is a facade that should be cleaned up after the mass
     * migration to ChromeSharedPreferences.
     */
    @Deprecated
    public static SharedPreferencesManager getInstance() {
        return (SharedPreferencesManager) ChromeSharedPreferences.getInstance();
    }
}

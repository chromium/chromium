// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import androidx.annotation.LayoutRes;
import androidx.preference.Preference;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsFeatureList;
import org.chromium.components.user_prefs.UserPrefs;

/** A ManagedPreferenceDelegate with Chrome-specific default behavior. */
public interface ChromeManagedPreferenceDelegate extends ManagedPreferenceDelegate {
    @Override
    default boolean isPreferenceControlledByCustodian(Preference preference) {
        return false;
    }

    @Override
    default boolean doesProfileHaveMultipleCustodians() {
        return !UserPrefs.get(Profile.getLastUsedRegularProfile())
                        .getString(Pref.SUPERVISED_USER_SECOND_CUSTODIAN_NAME)
                        .isEmpty();
    }

    @Override
    default @LayoutRes int defaultPreferenceLayoutResource() {
        return SettingsFeatureList.isEnabled(
                       SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
                ? R.layout.chrome_managed_preference
                : 0;
    }
}

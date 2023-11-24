// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import androidx.annotation.LayoutRes;
import androidx.preference.Preference;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.user_prefs.UserPrefs;

/** A ManagedPreferenceDelegate with Chrome-specific default behavior. */
public abstract class ChromeManagedPreferenceDelegate implements ManagedPreferenceDelegate {
    private Profile mProfile;

    /** Builds a ChromeManagedPreferenceDelegate for the given Profile. */
    public ChromeManagedPreferenceDelegate(Profile profile) {
        assert profile != null : "Attempting to use a null profile for the managed delegate.";
        mProfile = profile;
    }

    @Override
    public boolean isPreferenceControlledByCustodian(Preference preference) {
        return false;
    }

    @Override
    public boolean doesProfileHaveMultipleCustodians() {
        return !UserPrefs.get(mProfile)
                .getString(Pref.SUPERVISED_USER_SECOND_CUSTODIAN_NAME)
                .isEmpty();
    }

    @Override
    public @LayoutRes int defaultPreferenceLayoutResource() {
        return R.layout.chrome_managed_preference;
    }
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox.v4;

import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsBaseFragment;
import org.chromium.chrome.browser.privacy_sandbox.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Fragment for the Privacy Sandbox -> Fledge preferences.
 */
public class FledgeFragmentV4 extends PrivacySandboxSettingsBaseFragment
        implements Preference.OnPreferenceChangeListener {
    private static final String FLEDGE_TOGGLE_PREFERENCE = "fledge_toggle";

    private ChromeSwitchPreference mFledgeTogglePreference;

    static boolean isFledgePrefEnabled() {
        PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
        return prefService.getBoolean(Pref.PRIVACY_SANDBOX_M1_FLEDGE_ENABLED);
    }

    static void setFledgePrefEnabled(boolean isEnabled) {
        PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
        prefService.setBoolean(Pref.PRIVACY_SANDBOX_M1_FLEDGE_ENABLED, isEnabled);
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        super.onCreatePreferences(bundle, s);
        getActivity().setTitle(R.string.settings_fledge_page_title);
        SettingsUtils.addPreferencesFromResource(this, R.xml.fledge_preference_v4);

        mFledgeTogglePreference = findPreference(FLEDGE_TOGGLE_PREFERENCE);
        mFledgeTogglePreference.setChecked(isFledgePrefEnabled());
        mFledgeTogglePreference.setOnPreferenceChangeListener(this);
        // TODO(http://b/254411473): Make the preference managed.
    }

    @Override
    public boolean onPreferenceChange(@NonNull Preference preference, Object value) {
        if (preference.getKey().equals(FLEDGE_TOGGLE_PREFERENCE)) {
            setFledgePrefEnabled((boolean) value);
            return true;
        }

        return false;
    }
}

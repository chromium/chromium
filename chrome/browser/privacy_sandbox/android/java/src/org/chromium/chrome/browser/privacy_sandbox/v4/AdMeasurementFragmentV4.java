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
 * Fragment for the Privacy Sandbox -> Ad Measurement preferences.
 */
public class AdMeasurementFragmentV4 extends PrivacySandboxSettingsBaseFragment
        implements Preference.OnPreferenceChangeListener {
    public static final String TOGGLE_PREFERENCE = "ad_measurement_toggle";

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        super.onCreatePreferences(bundle, s);
        getActivity().setTitle(R.string.settings_ad_measurement_page_title);
        SettingsUtils.addPreferencesFromResource(this, R.xml.ad_measurement_preference_v4);

        ChromeSwitchPreference adMeasurementToggle = findPreference(TOGGLE_PREFERENCE);
        PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
        adMeasurementToggle.setChecked(
                prefService.getBoolean(Pref.PRIVACY_SANDBOX_M1_AD_MEASUREMENT_ENABLED));
        adMeasurementToggle.setOnPreferenceChangeListener(this);
        // TODO(b/254412966): Make the preference managed.
    }

    @Override
    public boolean onPreferenceChange(@NonNull Preference preference, Object value) {
        if (preference.getKey().equals(TOGGLE_PREFERENCE)) {
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            prefService.setBoolean(Pref.PRIVACY_SANDBOX_M1_AD_MEASUREMENT_ENABLED, (boolean) value);
            return true;
        }
        return false;
    }
}

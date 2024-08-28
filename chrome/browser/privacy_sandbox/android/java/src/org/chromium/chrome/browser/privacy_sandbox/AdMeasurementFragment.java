// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Fragment for the Privacy Sandbox -> Ad Measurement preferences. */
public class AdMeasurementFragment extends PrivacySandboxSettingsBaseFragment
        implements Preference.OnPreferenceChangeListener {
    public static final String TOGGLE_PREFERENCE = "ad_measurement_toggle";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    static boolean isAdMeasurementPrefEnabled(Profile profile) {
        PrefService prefService = UserPrefs.get(profile);
        return prefService.getBoolean(Pref.PRIVACY_SANDBOX_M1_AD_MEASUREMENT_ENABLED);
    }

    static void setAdMeasurementPrefEnabled(Profile profile, boolean isEnabled) {
        PrefService prefService = UserPrefs.get(profile);
        prefService.setBoolean(Pref.PRIVACY_SANDBOX_M1_AD_MEASUREMENT_ENABLED, isEnabled);
    }

    static boolean isAdMeasurementPrefManaged(Profile profile) {
        PrefService prefService = UserPrefs.get(profile);
        return prefService.isManagedPreference(Pref.PRIVACY_SANDBOX_M1_AD_MEASUREMENT_ENABLED);
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        super.onCreatePreferences(bundle, s);
        mPageTitle.set(getString(R.string.settings_ad_measurement_page_title));
        SettingsUtils.addPreferencesFromResource(this, R.xml.ad_measurement_preference);

        ChromeSwitchPreference adMeasurementToggle = findPreference(TOGGLE_PREFERENCE);
        adMeasurementToggle.setChecked(isAdMeasurementPrefEnabled(getProfile()));
        adMeasurementToggle.setOnPreferenceChangeListener(this);
        adMeasurementToggle.setManagedPreferenceDelegate(createManagedPreferenceDelegate());
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public boolean onPreferenceChange(@NonNull Preference preference, Object value) {
        if (preference.getKey().equals(TOGGLE_PREFERENCE)) {
            boolean enabled = (boolean) value;
            RecordUserAction.record(
                    enabled
                            ? "Settings.PrivacySandbox.AdMeasurement.Enabled"
                            : "Settings.PrivacySandbox.AdMeasurement.Disabled");
            setAdMeasurementPrefEnabled(getProfile(), enabled);
            return true;
        }
        return false;
    }

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return new ChromeManagedPreferenceDelegate(getProfile()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                if (TOGGLE_PREFERENCE.equals(preference.getKey())) {
                    return isAdMeasurementPrefManaged(getProfile());
                }
                return false;
            }
        };
    }
}

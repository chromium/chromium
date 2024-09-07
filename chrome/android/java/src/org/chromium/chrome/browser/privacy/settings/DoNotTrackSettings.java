// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import android.os.Bundle;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Fragment to manage 'Do Not Track' preference and to explain to the user what it does. */
public class DoNotTrackSettings extends ChromeBaseSettingsFragment {
    // Must match key in do_not_track_preferences.xml.
    private static final String PREF_DO_NOT_TRACK_SWITCH = "do_not_track_switch";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.do_not_track_preferences);
        mPageTitle.set(getString(R.string.do_not_track_title));

        ChromeSwitchPreference doNotTrackSwitch =
                (ChromeSwitchPreference) findPreference(PREF_DO_NOT_TRACK_SWITCH);

        PrefService prefService = UserPrefs.get(getProfile());
        boolean isDoNotTrackEnabled = prefService.getBoolean(Pref.ENABLE_DO_NOT_TRACK);
        doNotTrackSwitch.setChecked(isDoNotTrackEnabled);

        doNotTrackSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    prefService.setBoolean(Pref.ENABLE_DO_NOT_TRACK, (boolean) newValue);
                    return true;
                });
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }
}

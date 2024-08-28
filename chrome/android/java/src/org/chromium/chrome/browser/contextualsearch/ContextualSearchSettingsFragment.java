// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Fragment to manage the Contextual Search Settings in Chrome Settings, and to explain to the user
 * what Contextual Search (aka Touch to Search) actually does.
 */
public class ContextualSearchSettingsFragment extends ChromeBaseSettingsFragment {
    static final String PREF_CONTEXTUAL_SEARCH_SWITCH = "contextual_search_switch";
    static final String PREF_WAS_FULLY_ENABLED_SWITCH = "see_better_results_switch";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.contextual_search_preferences);
        mPageTitle.set(getString(R.string.contextual_search_title));
        setHasOptionsMenu(true);
        initSwitches();
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    private void initSwitches() {
        ChromeSwitchPreference contextualSearchSwitch =
                (ChromeSwitchPreference) findPreference(PREF_CONTEXTUAL_SEARCH_SWITCH);
        ChromeSwitchPreference seeBetterResultsSwitch =
                (ChromeSwitchPreference) findPreference(PREF_WAS_FULLY_ENABLED_SWITCH);

        Profile profile = getProfile();
        boolean isContextualSearchEnabled =
                !ContextualSearchPolicy.isContextualSearchDisabled(profile);
        contextualSearchSwitch.setChecked(isContextualSearchEnabled);

        contextualSearchSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    ContextualSearchPolicy.setContextualSearchState(profile, (boolean) newValue);
                    ContextualSearchUma.logMainPreferenceChange((boolean) newValue);
                    seeBetterResultsSwitch.setVisible((boolean) newValue);
                    return true;
                });

        contextualSearchSwitch.setManagedPreferenceDelegate(
                new ChromeManagedPreferenceDelegate(profile) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return ContextualSearchPolicy.isContextualSearchDisabledByPolicy(profile);
                    }
                });

        seeBetterResultsSwitch.setChecked(
                ContextualSearchPolicy.isContextualSearchPrefFullyOptedIn(profile));
        seeBetterResultsSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    ContextualSearchUma.logPrivacyOptInPreferenceChange((boolean) newValue);
                    ContextualSearchPolicy.setContextualSearchFullyOptedIn(
                            profile, (boolean) newValue);
                    return true;
                });

        seeBetterResultsSwitch.setVisible(isContextualSearchEnabled);
    }
}
